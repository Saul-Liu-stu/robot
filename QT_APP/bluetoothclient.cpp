#include "bluetoothclient.h"
#include <QCoreApplication>
#include <QJniObject>
#include <QJniEnvironment>
#include <QDateTime>
#include <QDebug>

const QString BluetoothClient::SPP_UUID_STR =
    QStringLiteral("00001101-0000-1000-8000-00805F9B34FB");

// ============================================================
//  RfcommReader
// ============================================================

RfcommReader::RfcommReader(void *inputStream, QObject *parent)
    : QThread(parent), m_inputStream(inputStream) {}

void RfcommReader::stop() { m_running = false; }

void RfcommReader::run()
{
    QJniEnvironment env;
    QJniObject is(static_cast<jobject>(m_inputStream));
    QByteArray buf;
    jbyteArray jbuf = env->NewByteArray(1024);

    while (m_running) {
        jint n = is.callMethod<jint>("read", "([B)I", jbuf);
        if (n <= 0) {
            if (n < 0 && m_running) emit readError(QStringLiteral("流中断"));
            break;
        }

        jbyte *d = env->GetByteArrayElements(jbuf, nullptr);
        buf.append(reinterpret_cast<const char *>(d), n);
        env->ReleaseByteArrayElements(jbuf, d, JNI_ABORT);

        // 按 \r\n 分行，发送给主线程处理
        while (true) {
            int idx = buf.indexOf("\r\n");
            if (idx < 0) break;
            QByteArray line = buf.left(idx);
            buf.remove(0, idx + 2);
            emit dataReceived(line);
        }
        if (buf.size() > 16384) buf.clear();
    }
    env->DeleteLocalRef(jbuf);
}

// ============================================================
//  BluetoothClient
// ============================================================

BluetoothClient::BluetoothClient(QObject *parent) : QObject(parent)
{
    m_local = new QBluetoothLocalDevice(this);
    m_disco = new QBluetoothDeviceDiscoveryAgent(this);
    connect(m_disco, &QBluetoothDeviceDiscoveryAgent::deviceDiscovered,
            this, &BluetoothClient::onDeviceDiscovered);
    connect(m_disco, &QBluetoothDeviceDiscoveryAgent::finished,
            this, &BluetoothClient::onScanFinished);
    connect(m_disco, &QBluetoothDeviceDiscoveryAgent::errorOccurred,
            this, &BluetoothClient::onScanError);
}

BluetoothClient::~BluetoothClient() { disconnect(); }

void BluetoothClient::setState(State s)
{
    if (m_state != s) { m_state = s; emit stateChanged(m_state); }
}

// ── 扫描 + 权限 ────────────────────────────────────────────────

void BluetoothClient::startScan()
{
    if (m_state == Scanning) return;
    stopScan();
    requestPermAndScan();
}

void BluetoothClient::requestPermAndScan()
{
    QBluetoothPermission bp;
    QLocationPermission lp;
    lp.setAccuracy(QLocationPermission::Precise);

    auto bs = qApp->checkPermission(bp);
    auto ls = qApp->checkPermission(lp);

    if (bs == Qt::PermissionStatus::Denied && ls == Qt::PermissionStatus::Denied) {
        emit errorOccurred(QStringLiteral("缺少蓝牙/位置权限，请在系统设置中授予"));
        return;
    }
    if (bs == Qt::PermissionStatus::Undetermined) {
        qApp->requestPermission(bp, [this, lp]() {
            if (qApp->checkPermission(lp) == Qt::PermissionStatus::Undetermined)
                qApp->requestPermission(lp, [this]() { doStartScan(); });
            else
                doStartScan();
        });
        return;
    }
    if (ls == Qt::PermissionStatus::Undetermined) {
        qApp->requestPermission(lp, [this]() { doStartScan(); });
        return;
    }
    doStartScan();
}

void BluetoothClient::doStartScan()
{
    setState(Scanning);
    m_disco->start();
}

void BluetoothClient::stopScan()
{
    if (m_disco->isActive()) m_disco->stop();
}

void BluetoothClient::onDeviceDiscovered(const QBluetoothDeviceInfo &info)
{
    emit deviceDiscovered(info);
}

void BluetoothClient::onScanFinished()
{
    if (m_state == Scanning) setState(Idle);
    emit scanFinished();
}

void BluetoothClient::onScanError(QBluetoothDeviceDiscoveryAgent::Error)
{
    QString m = m_disco->errorString();
    if (m.contains(QStringLiteral("permission"), Qt::CaseInsensitive))
        m = QStringLiteral("缺少权限");
    else if (m.contains(QStringLiteral("off"), Qt::CaseInsensitive))
        m = QStringLiteral("请开蓝牙");
    emit errorOccurred(QStringLiteral("扫描失败: ") + m);
    if (m_state == Scanning) setState(Idle);
}

// ── 连接 ───────────────────────────────────────────────────────

void BluetoothClient::connectToDevice(const QBluetoothDeviceInfo &d)
{
    connectToAddress(d.address().toString());
}

void BluetoothClient::connectToAddress(const QString &a)
{
    if (m_state == Connected) disconnect();
    stopScan();

    QBluetoothPermission bp;
    auto s = qApp->checkPermission(bp);
    if (s == Qt::PermissionStatus::Denied) {
        emit errorOccurred(QStringLiteral("权限被拒"));
        return;
    }
    if (s == Qt::PermissionStatus::Undetermined) {
        qApp->requestPermission(bp, [this, a]() { doConnect(a); });
        return;
    }
    doConnect(a);
}

void BluetoothClient::doConnect(const QString &a)
{
    setState(Connecting);
    m_frameCount = 0;
    m_buffer.clear();

    if (connectRfcomm(a)) {
        setState(Connected);
    } else {
        emit errorOccurred(QStringLiteral("连接失败\n请确认 HC-05 已配对且开机"));
        setState(Idle);
    }
}

// ── RFCOMM JNI (参考 lanya: 反射 channel=1) ──────────────────────

bool BluetoothClient::connectRfcomm(const QString &address)
{
    QJniEnvironment env;

    QJniObject adapter = QJniObject::callStaticObjectMethod(
        "android/bluetooth/BluetoothAdapter", "getDefaultAdapter",
        "()Landroid/bluetooth/BluetoothAdapter;");
    if (!adapter.isValid()) return false;

    QJniObject jAddr = QJniObject::fromString(address);
    QJniObject dev = adapter.callObjectMethod(
        "getRemoteDevice",
        "(Ljava/lang/String;)Landroid/bluetooth/BluetoothDevice;",
        jAddr.object<jstring>());
    if (!dev.isValid()) return false;

    // 方法1: 反射直连 channel=1 (绕过SDP, HC-05 最可靠)
    QJniObject dc = dev.callObjectMethod("getClass", "()Ljava/lang/Class;");
    QJniObject it = QJniObject::getStaticObjectField(
        "java/lang/Integer", "TYPE", "Ljava/lang/Class;");

    jobjectArray pt = env->NewObjectArray(1, env->FindClass("java/lang/Class"), nullptr);
    env->SetObjectArrayElement(pt, 0, it.object<jclass>());

    jstring mn = env->NewStringUTF("createRfcommSocket");
    QJniObject method = dc.callObjectMethod(
        "getMethod", "(Ljava/lang/String;[Ljava/lang/Class;)Ljava/lang/reflect/Method;",
        mn, pt);
    env->DeleteLocalRef(mn);
    env->DeleteLocalRef(pt);

    if (!method.isValid()) {
        // 方法2: SPP UUID 标准方式
        QJniObject sppUuid = QJniObject::fromString(SPP_UUID_STR);
        QJniObject sock = dev.callObjectMethod(
            "createRfcommSocketToServiceRecord",
            "(Ljava/util/UUID;)Landroid/bluetooth/BluetoothSocket;",
            sppUuid.object<jobject>());
        if (!sock.isValid()) return false;

        sock.callMethod<void>("connect", "()V");
        if (env->ExceptionCheck()) {
            env->ExceptionClear();
            sock.callMethod<void>("close", "()V");
            return false;
        }

        QJniObject is = sock.callObjectMethod(
            "getInputStream", "()Ljava/io/InputStream;");
        QJniObject os = sock.callObjectMethod(
            "getOutputStream", "()Ljava/io/OutputStream;");
        if (!is.isValid() || !os.isValid()) {
            sock.callMethod<void>("close", "()V");
            return false;
        }

        m_javaSocket = env->NewGlobalRef(sock.object());
        m_outputStream = env->NewGlobalRef(os.object());
        void *isRef = env->NewGlobalRef(is.object());
        m_reader = new RfcommReader(isRef, this);
        connect(m_reader, &RfcommReader::dataReceived,
                this, &BluetoothClient::onReaderData);
        connect(m_reader, &RfcommReader::readError,
                this, &BluetoothClient::onReaderError);
        m_reader->start();
        return true;
    }

    // 反射方式
    QJniObject ci = QJniObject::callStaticObjectMethod(
        "java/lang/Integer", "valueOf", "(I)Ljava/lang/Integer;", jint(1));
    jobjectArray ia = env->NewObjectArray(1, env->FindClass("java/lang/Object"), nullptr);
    env->SetObjectArrayElement(ia, 0, ci.object());

    QJniObject sock = method.callObjectMethod(
        "invoke", "(Ljava/lang/Object;[Ljava/lang/Object;)Ljava/lang/Object;",
        dev.object(), ia);
    env->DeleteLocalRef(ia);

    if (!sock.isValid()) return false;

    sock.callMethod<void>("connect", "()V");
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
        return false;
    }

    QJniObject is = sock.callObjectMethod(
        "getInputStream", "()Ljava/io/InputStream;");
    QJniObject os = sock.callObjectMethod(
        "getOutputStream", "()Ljava/io/OutputStream;");
    if (!is.isValid() || !os.isValid()) {
        sock.callMethod<void>("close", "()V");
        return false;
    }

    m_javaSocket = env->NewGlobalRef(sock.object());
    m_outputStream = env->NewGlobalRef(os.object());
    void *isRef = env->NewGlobalRef(is.object());
    m_reader = new RfcommReader(isRef, this);
    connect(m_reader, &RfcommReader::dataReceived,
            this, &BluetoothClient::onReaderData);
    connect(m_reader, &RfcommReader::readError,
            this, &BluetoothClient::onReaderError);
    m_reader->start();
    return true;
}

// ── 数据接收 ───────────────────────────────────────────────────

void BluetoothClient::onReaderData(const QByteArray &data)
{
    m_buffer.append(data);
    emit rawLineReceived(QString::fromUtf8(data));
    processBuffer();
}

void BluetoothClient::onReaderError(const QString &msg)
{
    if (m_state == Connected) {
        setState(Idle);
        emit errorOccurred(QStringLiteral("连接断开: ") + msg);
    }
}

void BluetoothClient::processBuffer()
{
    // RfcommReader 已按 \r\n 分行，每次发一整行
    QString lineStr = QString::fromUtf8(m_buffer).trimmed();
    m_buffer.clear();
    if (lineStr.isEmpty()) return;

    // 阶段一: 舵机校准
    // 诊断: 所有接收到的行都显示在日志
    emit rawLineReceived(lineStr);

    ServoResponse servoRsp = parseServoLine(lineStr);
    if (servoRsp.event != ServoResponse::None) {
        emit servoResponseReceived(servoRsp);
        return;
    }

    // 电机应答 "MA:50:1"
    MotorResponse motorRsp = parseMotorLine(lineStr);
    if (motorRsp.valid) {
        emit motorResponseReceived(motorRsp);
        return;
    }

    // 阶段二: PID 调参
    PidMessage pidMsg = parsePidLine(lineStr);
    if (pidMsg.type != PidNone) { emit pidMessageReceived(pidMsg); return; }

    // 阶段三: IMU/编码器 (后期)
    TelemetryData data;
    if (parseTelemetryLine(lineStr, data) != 0) {
        data.ts = QDateTime::currentMSecsSinceEpoch();
        m_frameCount++;
        emit telemetryReceived(data);
    }
}

// ── 发送舵机角度 ───────────────────────────────────────────────

void BluetoothClient::sendServoAngle(int angle)
{
    if (m_state != Connected || !m_outputStream) {
        emit errorOccurred(QStringLiteral("未连接，无法发送"));
        return;
    }

    QByteArray data = buildServoCmd(angle);
    QJniEnvironment env;

    jbyteArray jbuf = env->NewByteArray(data.size());
    env->SetByteArrayRegion(jbuf, 0, data.size(),
        reinterpret_cast<const jbyte *>(data.constData()));

    QJniObject os(static_cast<jobject>(m_outputStream));
    os.callMethod<void>("write", "([B)V", jbuf);
    os.callMethod<void>("flush", "()V");
    env->DeleteLocalRef(jbuf);

    if (env->ExceptionCheck()) {
        env->ExceptionClear();
        emit errorOccurred(QStringLiteral("发送失败"));
    }
}

void BluetoothClient::sendServoAngle(int srv, int angle)
{
    if (m_state != Connected || !m_outputStream) return;
    QByteArray data = buildServoCmd(srv, angle);  // "S3:150\r\n"
    QJniEnvironment env;
    jbyteArray jbuf = env->NewByteArray(data.size());
    env->SetByteArrayRegion(jbuf, 0, data.size(), reinterpret_cast<const jbyte *>(data.constData()));
    QJniObject os(static_cast<jobject>(m_outputStream));
    os.callMethod<void>("write", "([B)V", jbuf);
    os.callMethod<void>("flush", "()V");
    env->DeleteLocalRef(jbuf);
    if (env->ExceptionCheck()) env->ExceptionClear();
}

void BluetoothClient::sendServoSwitch(int srv)
{
    if (m_state != Connected || !m_outputStream) return;
    QByteArray data = buildServoSwitchCmd(srv);  // "5\r\n"
    QJniEnvironment env;
    jbyteArray jbuf = env->NewByteArray(data.size());
    env->SetByteArrayRegion(jbuf, 0, data.size(), reinterpret_cast<const jbyte *>(data.constData()));
    QJniObject os(static_cast<jobject>(m_outputStream));
    os.callMethod<void>("write", "([B)V", jbuf);
    os.callMethod<void>("flush", "()V");
    env->DeleteLocalRef(jbuf);
    if (env->ExceptionCheck()) env->ExceptionClear();
}

// ── 发送电机命令 ──────────────────────────────────────────────

void BluetoothClient::sendMotorCmd(int motorNum, int speed, int dir)
{
    if (m_state != Connected || !m_outputStream) return;
    QByteArray data = buildMotorCmd(motorNum, speed, dir);
    QJniEnvironment env;
    jbyteArray jbuf = env->NewByteArray(data.size());
    env->SetByteArrayRegion(jbuf, 0, data.size(), reinterpret_cast<const jbyte *>(data.constData()));
    QJniObject os(static_cast<jobject>(m_outputStream));
    os.callMethod<void>("write", "([B)V", jbuf);
    os.callMethod<void>("flush", "()V");
    env->DeleteLocalRef(jbuf);
    if (env->ExceptionCheck()) env->ExceptionClear();
}

// ── 发送 PID 命令 ──────────────────────────────────────────────

void BluetoothClient::sendPidCmd(const QString &cmd)
{
    if (m_state != Connected || !m_outputStream) {
        emit errorOccurred(QStringLiteral("未连接"));
        return;
    }
    QByteArray data = buildPidCmd(cmd);
    QJniEnvironment env;
    jbyteArray jbuf = env->NewByteArray(data.size());
    env->SetByteArrayRegion(jbuf, 0, data.size(), reinterpret_cast<const jbyte *>(data.constData()));
    QJniObject os(static_cast<jobject>(m_outputStream));
    os.callMethod<void>("write", "([B)V", jbuf);
    os.callMethod<void>("flush", "()V");
    env->DeleteLocalRef(jbuf);
    if (env->ExceptionCheck()) { env->ExceptionClear(); emit errorOccurred(QStringLiteral("发送失败")); }
}

// ── 发送纯文本 ──────────────────────────────────────────────────

void BluetoothClient::sendRawText(const QString &text)
{
    if (m_state != Connected || !m_outputStream) return;
    QByteArray data = text.toUtf8() + "\r\n";
    QJniEnvironment env;
    jbyteArray jbuf = env->NewByteArray(data.size());
    env->SetByteArrayRegion(jbuf, 0, data.size(), reinterpret_cast<const jbyte *>(data.constData()));
    QJniObject os(static_cast<jobject>(m_outputStream));
    os.callMethod<void>("write", "([B)V", jbuf);
    os.callMethod<void>("flush", "()V");
    env->DeleteLocalRef(jbuf);
    if (env->ExceptionCheck()) env->ExceptionClear();
}

// ── 发送 A5/5A 帧 (阶段三保留) ──────────────────────────────────

void BluetoothClient::sendCommand(uint8_t cmd)
{
    if (m_state != Connected || !m_outputStream) {
        emit errorOccurred(QStringLiteral("未连接，无法发送命令"));
        return;
    }

    QByteArray frame = buildCommand(cmd);
    QJniEnvironment env;

    jbyteArray jbuf = env->NewByteArray(frame.size());
    env->SetByteArrayRegion(jbuf, 0, frame.size(),
        reinterpret_cast<const jbyte *>(frame.constData()));

    QJniObject os(static_cast<jobject>(m_outputStream));
    os.callMethod<void>("write", "([B)V", jbuf);
    os.callMethod<void>("flush", "()V");
    env->DeleteLocalRef(jbuf);

    if (env->ExceptionCheck()) {
        env->ExceptionClear();
        emit errorOccurred(QStringLiteral("命令发送失败"));
    }
}

// ── 断开 ───────────────────────────────────────────────────────

void BluetoothClient::disconnect()
{
    QJniEnvironment env;

    if (m_reader) {
        m_reader->stop();
        m_reader->wait(3000);
        delete m_reader;
        m_reader = nullptr;
    }
    if (m_outputStream) {
        env->DeleteGlobalRef(static_cast<jobject>(m_outputStream));
        m_outputStream = nullptr;
    }
    if (m_javaSocket) {
        QJniObject(static_cast<jobject>(m_javaSocket)).callMethod<void>(
            "close", "()V");
        env->DeleteGlobalRef(static_cast<jobject>(m_javaSocket));
        m_javaSocket = nullptr;
    }
    setState(Idle);
}
