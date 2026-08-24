#include "mainwindow.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QSlider>
#include <QGroupBox>
#include <QFrame>
#include <QScrollArea>
#include <QDateTime>
#include <QBluetoothDeviceInfo>
#include <QTimer>
#include <QSettings>
#include <QCoreApplication>
#include <QJniObject>
#if defined(Q_OS_ANDROID)
#include <QtCore/qnativeinterface.h>
#endif

// 石墨极简风 (v6.10): 近黑灰底 + 琥珀强调 + 多色语义保留
const QString MainWindow::C_BG     = QStringLiteral("#141518");
const QString MainWindow::C_CARD   = QStringLiteral("#1e2024");
const QString MainWindow::C_ACCENT = QStringLiteral("#2a2d33");
const QString MainWindow::C_BLUE   = QStringLiteral("#3b82f6");
const QString MainWindow::C_GREEN  = QStringLiteral("#22c55e");
const QString MainWindow::C_ORANGE = QStringLiteral("#f59e0b");
const QString MainWindow::C_RED    = QStringLiteral("#ef4444");
const QString MainWindow::C_TXT    = QStringLiteral("#e5e7eb");
const QString MainWindow::C_DIM    = QStringLiteral("#6b7280");

static bool isTarget(const QString &n)
{
    QString u = n.toUpper();
    return u.contains(QStringLiteral("HC-05")) || u.contains(QStringLiteral("HC-06"))
        || u.contains(QStringLiteral("ROBOT"));
}

static void sGrp(QGroupBox *g, const QString &t)
{
    g->setTitle(t);
    g->setStyleSheet(QString(
        "QGroupBox{font-size:11px;font-weight:bold;color:%1;"
        "border:1px solid %2;border-radius:10px;margin-top:10px;padding-top:14px;}"
        "QGroupBox::title{subcontrol-origin:margin;left:10px;padding:0 4px;color:%3;}")
        .arg(MainWindow::C_TXT, MainWindow::C_ACCENT, MainWindow::C_DIM));
}
static QString mkStyleB(const QString &bg)
{
    return QString(
        "QPushButton{border:none;border-radius:10px;padding:4px 10px;"
        "font-size:11px;font-weight:bold;color:#fff;background:%1;}"
        "QPushButton:disabled{background:#2a2c31;color:#5a5f68;}").arg(bg);
}
static QPushButton *mkB(const QString &t, const QString &bg)
{
    auto *b = new QPushButton(t); b->setMinimumHeight(30);
    b->setStyleSheet(mkStyleB(bg));
    return b;
}

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent) { setupUi();
    m_lastAddr = QSettings().value(QStringLiteral("bt/lastAddr")).toString();  // v6.10 一键重连
    m_btnReconnect->setVisible(!m_lastAddr.isEmpty());
    m_bt = new BluetoothClient(this);
    connect(m_bt, &BluetoothClient::stateChanged, this, &MainWindow::onBtStateChanged);
    connect(m_bt, &BluetoothClient::deviceDiscovered, this, &MainWindow::onDeviceDiscovered);
    connect(m_bt, &BluetoothClient::servoResponseReceived, this, &MainWindow::onServoResponse);
    connect(m_bt, &BluetoothClient::pidMessageReceived, this, &MainWindow::onPidMessage);
    connect(m_bt, &BluetoothClient::motorResponseReceived, this, &MainWindow::onMotorResponse);
    connect(m_bt, &BluetoothClient::telemetryReceived, this, &MainWindow::onTelemetry);
    connect(m_bt, &BluetoothClient::rawLineReceived, this, [this](const QString &l) {
        if (l.startsWith(QStringLiteral("READY"))) {
            appendLog(QStringLiteral("🤝 READY — 固件就绪"), C_GREEN);
            setPoseState(QStringLiteral("上电初始"));
            setActState(QStringLiteral("校准模式"), C_ORANGE);
            // v6.14 固件上电/看门狗复位: 云台自动回正 90/120, 同步 APP 轮盘显示
            m_gimbalPanDeg = 90; m_gimbalTiltDeg = 120;
            if (m_gimbalDial) m_gimbalDial->setValue(90, 120);
            if (m_gimbalStateLabel) {
                m_gimbalStateLabel->setText(QStringLiteral("✅ 水平 90° / 俯仰 120° (正对前方)"));
                m_gimbalStateLabel->setStyleSheet(QString(
                    "font-size:13px;font-weight:bold;color:%1;background:%2;border-radius:10px;padding:8px;")
                    .arg(C_TXT, C_CARD));
            }
        }
        else if (l.startsWith(QStringLiteral("UNK:")))
            appendLog(QStringLiteral("⚠ 固件不认识: %1").arg(l.mid(4)), C_ORANGE);
        else if (l.startsWith(QStringLiteral("CALIB"))) {
            // v6.12 Z = 自稳标定 (放平静止2s, 每姿态单独存; 重复按重标)
            appendLog(QStringLiteral("⚖ 自稳标定中 (放平静止2s)..."), C_ORANGE);
            m_slopeState->setText(QStringLiteral("状态: 标定中..."));
            m_slopeState->setStyleSheet(QString("font-size:11px;font-weight:bold;color:%1;background:transparent;").arg(C_ORANGE));
            setActState(QStringLiteral("自稳标定中"), C_ORANGE);
        }
        else if (l.startsWith(QStringLiteral("CAL:OK"))) {
            // v6.12 标定完成: 零偏已存, 可开自稳 L:1 (V 模式1 自动跟随)
            appendLog(QStringLiteral("⚖ 自稳标定完成 — 零偏已存, 可开自稳 L:1"), C_GREEN);
            m_slopeState->setText(QStringLiteral("状态: 已标定"));
            m_slopeState->setStyleSheet(QString("font-size:11px;font-weight:bold;color:%1;background:transparent;").arg(C_GREEN));
            setActState(QStringLiteral("站立"), C_GREEN);
        }
        else if (l.startsWith(QStringLiteral("LV:ON"))) {
            // v6.14 站立自稳开启
            appendLog(QStringLiteral("🦿 站立自稳已开启 (H/K 高站姿 z 差动补偿)"), C_GREEN);
            m_lvOn = true;
            if (m_lvState) {
                m_lvState->setText(QStringLiteral("状态: 已开启"));
                m_lvState->setStyleSheet(QString("font-size:11px;font-weight:bold;color:%1;background:transparent;").arg(C_GREEN));
            }
            if (m_btnLv) m_btnLv->setText(QStringLiteral("L:0 关"));
        }
        else if (l.startsWith(QStringLiteral("LV:OFF"))) {
            appendLog(QStringLiteral("🦿 站立自稳已关闭"), C_DIM);
            m_lvOn = false;
            if (m_lvState) {
                m_lvState->setText(QStringLiteral("状态: 关闭"));
                m_lvState->setStyleSheet(QString("font-size:11px;font-weight:bold;color:%1;background:transparent;").arg(C_DIM));
            }
            if (m_btnLv) m_btnLv->setText(QStringLiteral("L:1 开"));
        }
        else if (l.startsWith(QStringLiteral("LV:NOCAL"))) {
            appendLog(QStringLiteral("⚠ 自稳未标定 — 先发 Z 标定当前姿态 (静止2s)"), C_ORANGE);
            m_lvOn = false;
            if (m_lvState) {
                m_lvState->setText(QStringLiteral("状态: 未标定"));
                m_lvState->setStyleSheet(QString("font-size:11px;font-weight:bold;color:%1;background:transparent;").arg(C_ORANGE));
            }
            if (m_btnLv) m_btnLv->setText(QStringLiteral("L:1 开"));
        }
        else if (l.startsWith(QStringLiteral("IMU:ON")))
            appendLog(QStringLiteral("📡 IMU 上报已开启"), C_GREEN);
        else if (l.startsWith(QStringLiteral("IMU:OFF")))
            appendLog(QStringLiteral("📡 IMU 上报已关闭"), C_DIM);
        else if (l.startsWith(QStringLiteral("J:ON")))
            appendLog(QStringLiteral("🔢 编码器转速上报已开启 (10Hz)"), C_GREEN);
        else if (l.startsWith(QStringLiteral("J:OFF")))
            appendLog(QStringLiteral("🔢 编码器转速上报已关闭"), C_DIM);
        else if (l.startsWith(QStringLiteral("DRV:"))) {
            // v6.5 "DRV:dir:spd" — 方向按键应答
            QStringList dp = l.mid(4).split(QLatin1Char(':'));
            int dir = dp.value(0).toInt();
            int spd = dp.value(1).toInt();
            static const char *dirNames[4] = {"前进", "后退", "左前", "右前"};
            QString dn = (dir >= 0 && dir < 4) ? QString::fromUtf8(dirNames[dir])
                                               : QStringLiteral("方向%1").arg(dir);
            appendLog(QStringLiteral("🧭 方向按键 %1 %2%% (倾斜保持)").arg(dn).arg(spd), C_GREEN);
        }
        else if (l.startsWith(QStringLiteral("STAND...")) || l.startsWith(QStringLiteral("OK:STAND"))) {
            appendLog(QStringLiteral("🧍 站立完成 (狗高 280)"), C_BLUE);
            setPoseState(QStringLiteral("狗高 280"));
            setActState(QStringLiteral("站立"), C_GREEN);
            setGaitEnabled(true);
        }
        else if (l.startsWith(QStringLiteral("GM:"))) {
            // v6.14 云台回显 "GM:pan,tilt" (固件限幅后的实际值)
            // 拖动中 (5Hz 高频): 只更新记录值和状态文字, 不同步轮盘防抢指针, 不打日志防刷屏
            QStringList gp = l.mid(3).split(QLatin1Char(','));
            int pan = gp.value(0).trimmed().toInt();
            int tilt = gp.value(1).trimmed().toInt();
            if (!m_gimbalDragging)
                appendLog(QStringLiteral("🎥 云台 → 水平 %1° / 俯仰 %2°").arg(pan).arg(tilt), C_BLUE);
            m_gimbalPanDeg = pan;
            m_gimbalTiltDeg = tilt;
            if (m_gimbalDial && !m_gimbalDragging)
                m_gimbalDial->setValue(pan, tilt);
            if (m_gimbalStateLabel) {
                m_gimbalStateLabel->setText(QStringLiteral("✅ 水平 %1° / 俯仰 %2° 已到位").arg(pan).arg(tilt));
                m_gimbalStateLabel->setStyleSheet(QString("font-size:11px;font-weight:bold;color:%1;background:transparent;").arg(C_GREEN));
            }
        }
        else if (l.startsWith(QStringLiteral("XSH:"))) {
            m_footShift = l.mid(4).trimmed().toInt();
            m_shiftLabel->setText(QStringLiteral("当前: %1 mm").arg(m_footShift));
            appendLog(QStringLiteral("⚖ 前移修正 %1mm").arg(m_footShift), C_GREEN);
        }
        else if (l.compare(QStringLiteral("REARH"), Qt::CaseInsensitive) == 0) {
            appendLog(QStringLiteral("🦵 后顶高站姿 280mm"), C_BLUE);
            setPoseState(QStringLiteral("后顶高 280"));
            setActState(QStringLiteral("站立"), C_GREEN);
            setGaitEnabled(true);
        }
        else if (l.compare(QStringLiteral("PARK"), Qt::CaseInsensitive) == 0) {
            appendLog(QStringLiteral("📦 贴地趴收纳 72mm (肚皮离地~53mm, 断电放置)"), C_BLUE);
            setPoseState(QStringLiteral("收纳贴地趴 72mm"));
            setActState(QStringLiteral("收纳中"), C_ORANGE);
            setGaitEnabled(false);
        }
        else if (l.compare(QStringLiteral("RISE"), Qt::CaseInsensitive) == 0) {
            appendLog(QStringLiteral("⬆ 规划式站起中 (单段3.5s → 前顶低趴191, 免翻膝)"), C_GREEN);
            setActState(QStringLiteral("规划站起中 (~3.5s)"), C_GREEN);
            setGaitEnabled(false);
        }
        else if (l.startsWith(QStringLiteral("NOT PARK")))
            appendLog(QStringLiteral("⚠ U 被拒: 未处于趴地状态 (已站立)"), C_ORANGE);
        else if (l.compare(QStringLiteral("REAR"), Qt::CaseInsensitive) == 0) {
            appendLog(QStringLiteral("⬇ 后顶低趴 240mm (四轮驱动)"), C_BLUE);
            setPoseState(QStringLiteral("后顶低趴 240"));
            setActState(QStringLiteral("站立"), C_GREEN);
            setGaitEnabled(false);
        }
        else if (l.startsWith(QStringLiteral("HIGH"), Qt::CaseInsensitive)) {
            // v6.12 HIGH = 高站姿 280; HIGH:高度 = 正常膝低站姿 200~280 (爬陡坡)
            int h = 280;
            const QStringList hp = l.split(QLatin1Char(':'));
            if (hp.size() > 1) h = hp.value(1).trimmed().toInt();
            if (h < 280) {
                appendLog(QStringLiteral("⬇ 低站姿 %1mm (正常膝, 站越低自稳可补偿坡度越大)").arg(h), C_BLUE);
                setPoseState(QStringLiteral("低站姿 %1").arg(h));
            } else {
                appendLog(QStringLiteral("⬆ 高站姿 280mm (狗姿态)"), C_BLUE);
                setPoseState(QStringLiteral("狗高 280"));
            }
            setActState(QStringLiteral("站立"), C_GREEN);
            setGaitEnabled(true);
        }
        else if (l.compare(QStringLiteral("KNEE"), Qt::CaseInsensitive) == 0) {
            appendLog(QStringLiteral("🦵 顶膝高站姿 280mm"), C_BLUE);
            setPoseState(QStringLiteral("前顶高 280"));
            setActState(QStringLiteral("站立"), C_GREEN);
            setGaitEnabled(true);
        }
        else if (l.compare(QStringLiteral("SIT"), Qt::CaseInsensitive) == 0) {
            appendLog(QStringLiteral("🐕 坐姿 (前腿竖直+后腿反折深蹲, 抬头~10°)"), C_BLUE);
            setPoseState(QStringLiteral("坐姿 240"));
            setActState(QStringLiteral("站立"), C_GREEN);
            setGaitEnabled(true);
        }
        else if (l.compare(QStringLiteral("LOW"), Qt::CaseInsensitive) == 0) {
            appendLog(QStringLiteral("⬇ 前顶低趴 191mm (四轮驱动, 勿发T)"), C_BLUE);
            setPoseState(QStringLiteral("前顶低趴 191"));
            setActState(QStringLiteral("站立"), C_GREEN);
            setGaitEnabled(false);
        }
        else if (l.compare(QStringLiteral("FLIP"), Qt::CaseInsensitive) == 0) {
            appendLog(QStringLiteral("🔄 翻膝机动开始 (~3s)"), C_ORANGE);
            setActState(QStringLiteral("翻膝机动中 (~3s)"), C_ORANGE);
            // 翻完升 280 狗姿态 (U 站起阶段B无最终回显, 约3s后解锁步态按钮)
            QTimer::singleShot(3200, this, [this]() { setGaitEnabled(true); });
        }
        else if (l.compare(QStringLiteral("ROLL"), Qt::CaseInsensitive) == 0) {
            appendLog(QStringLiteral("🛞 前进中 (30%)"), C_GREEN);
            setActState(QStringLiteral("轮式前进 30%"), C_GREEN);
            setDirState(QStringLiteral("前进 30%"), C_GREEN);
        }
        else if (l.compare(QStringLiteral("BACK"), Qt::CaseInsensitive) == 0) {
            appendLog(QStringLiteral("↩ 后退中 (30%)"), C_ORANGE);
            setActState(QStringLiteral("轮式后退 30%"), C_ORANGE);
            setDirState(QStringLiteral("后退 30%"), C_ORANGE);
        }
        else if (l.compare(QStringLiteral("CLIMB"), Qt::CaseInsensitive) == 0) {
            appendLog(QStringLiteral("🧗 越障模式: 车轮30% + 对角抬腿70mm (前伸50+回收推进)"), C_GREEN);
            setActState(QStringLiteral("越障模式"), C_GREEN);
            setDirState(QStringLiteral("越障前进"), C_GREEN);
        }
        else if (l.startsWith(QStringLiteral("SWAY"), Qt::CaseInsensitive)) {
            QString dir = l.contains(QLatin1String("-1")) ? QStringLiteral("逆时针") : QStringLiteral("顺时针");
            appendLog(QStringLiteral("🔄 转圈 %1").arg(dir), C_GREEN);
            setActState(QStringLiteral("转圈中"), C_GREEN);
            setDirState(QStringLiteral("%1转圈").arg(dir), C_GREEN);
        }
        else if (l.compare(QStringLiteral("TROT"), Qt::CaseInsensitive) == 0) {
            appendLog(QStringLiteral("🏃 连续行走 (步幅60 + 四轮10%助走)"), C_GREEN);
            setActState(QStringLiteral("连续行走"), C_GREEN);
            setDirState(QStringLiteral("行走前进"), C_GREEN);
        }
        else if (l.compare(QStringLiteral("TROTB"), Qt::CaseInsensitive) == 0) {
            appendLog(QStringLiteral("🔁 倒着走 (反向trot + 四轮10%反向助走)"), C_ORANGE);
            setActState(QStringLiteral("倒着走"), C_ORANGE);
            setDirState(QStringLiteral("行走后退"), C_ORANGE);
        }
        else if (l.startsWith(QStringLiteral("PH:"))) {
            appendLog(QStringLiteral("⏸ 单步调试 %1").arg(l), C_ORANGE);
            setActState(QStringLiteral("单步调试"), C_ORANGE);
        }
        else if (l.startsWith(QStringLiteral("SIDE:"))) {
            QString dir = l.mid(5).trimmed() == QStringLiteral("1") ? QStringLiteral("向右") : QStringLiteral("向左");
            appendLog(QStringLiteral("🦀 螃蟹步 %1").arg(dir), C_GREEN);
            setActState(QStringLiteral("横移走"), C_GREEN);
            setDirState(dir, C_GREEN);
        }
        else if (l.compare(QStringLiteral("STOP"), Qt::CaseInsensitive) == 0) {
            appendLog(QStringLiteral("🛑 四电机已停"), C_RED);
            setActState(QStringLiteral("停止"), C_RED);
            setDirState(QStringLiteral("停止"), C_RED);
        }
        else
            appendLog(QStringLiteral("📡 ") + l, C_DIM);
    });
    connect(m_bt, &BluetoothClient::errorOccurred, this, [this](const QString &m) {
        appendLog(QStringLiteral("ERROR: ") + m, C_RED); });
    onBtStateChanged(BluetoothClient::Idle);
}
MainWindow::~MainWindow() { m_bt->disconnect(); }

void MainWindow::appendLog(const QString &msg, const QString &c)
{
    if (++m_lineNum > 1000) { m_logView->clear(); m_lineNum = 1; }
    m_logView->append(QStringLiteral("<span style='color:%1;'>[%2 #%3] %4</span>")
        .arg(c, QDateTime::currentDateTime().toString(QStringLiteral("hh:mm:ss.zzz"))).arg(m_lineNum).arg(msg));
}

// ── 遥控页状态栏 (协议横屏方案: 回显驱动) ─────────────────────
void MainWindow::setPoseState(const QString &pose, const QString &color)
{
    if (!m_poseLabel) return;
    m_poseLabel->setText(QStringLiteral("姿态: %1").arg(pose));
    if (!color.isEmpty())
        m_poseLabel->setStyleSheet(QString("font-size:11px;font-weight:bold;color:%1;background:transparent;").arg(color));
}
void MainWindow::setActState(const QString &act, const QString &color)
{
    if (!m_actLabel) return;
    m_actLabel->setText(QStringLiteral("行为: %1").arg(act));
    if (!color.isEmpty())
        m_actLabel->setStyleSheet(QString("font-size:11px;font-weight:bold;color:%1;background:transparent;").arg(color));
}
void MainWindow::setDirState(const QString &dir, const QString &color)
{
    if (!m_dirLabel) return;
    m_dirLabel->setText(dir);
    if (!color.isEmpty())
        m_dirLabel->setStyleSheet(QString(
            "font-size:16px;font-weight:bold;color:%1;background:%2;border-radius:10px;padding:5px;")
            .arg(color, C_CARD));
}
void MainWindow::setGaitEnabled(bool on)
{
    // 低趴(LOW/REAR/PARK)置灰 T/W/E; STAND/HIGH/翻膝完成解锁
    for (auto *b : m_gaitBtns)
        if (b) b->setEnabled(on);
}

// ── 状态 ────────────────────────────────────────────────────
void MainWindow::onBtStateChanged(BluetoothClient::State s)
{
    QString t, i, c; bool on = false;
    switch (s) {
    case BluetoothClient::Idle:      t=QStringLiteral("未连接"); i=QStringLiteral("⚫"); c=C_DIM; break;
    case BluetoothClient::Scanning:  t=QStringLiteral("扫描中"); i=QStringLiteral("🔵"); c=C_BLUE; break;
    case BluetoothClient::Connecting:t=QStringLiteral("连接中"); i=QStringLiteral("🟡"); c=C_ORANGE; break;
    case BluetoothClient::Connected: t=QStringLiteral("已连接"); i=QStringLiteral("🟢"); c=C_GREEN; on=true; break;
    }
    m_statusIcon->setText(i); m_statusLabel->setText(t);
    m_statusLabel->setStyleSheet(QString("font-size:12px;font-weight:bold;color:%1;background:transparent;").arg(c));
    if (on) {
        // v6.10: 记录地址供一键重连 + 显示地址 + 自动跳遥控页
        QSettings().setValue(QStringLiteral("bt/lastAddr"), m_lastAddr);
        m_devAddrLabel->setText(m_lastAddr);
        m_btnReconnect->setVisible(false);
        if (m_stack->currentIndex() == 0) setPage(3);
        m_everConnected = true;
        m_userDisconnect = false;
        stopAutoReconnect();
    } else {
        m_btnReconnect->setVisible(!m_lastAddr.isEmpty());
        // v6.14 意外断开 (连过且非用户主动) → 自动重连 (固件看门狗复位后自动恢复)
        if (m_everConnected && !m_userDisconnect && !m_lastAddr.isEmpty())
            startAutoReconnect();
    }
    m_btnScan->setEnabled(s!=BluetoothClient::Connecting&&s!=BluetoothClient::Connected);
    m_btnConnect->setEnabled(s==BluetoothClient::Idle||s==BluetoothClient::Scanning);
    m_btnDisconnect->setEnabled(on);
    if (!on) {
        m_servoWidget->reset(); m_pidWidget->reset();
        // 自稳状态重置 (v6.14)
        m_lvOn = false;
        if (m_lvState) {
            m_lvState->setText(QStringLiteral("状态: 关闭"));
            m_lvState->setStyleSheet(QString("font-size:11px;font-weight:bold;color:%1;background:transparent;").arg(C_DIM));
        }
        if (m_btnLv) m_btnLv->setText(QStringLiteral("L:1 开"));
        // 云台回默认: 正对前方 90/120
        m_gimbalPanDeg = 90; m_gimbalTiltDeg = 120;
        m_gimbalDragging = false;
        if (m_gimbalTimer) m_gimbalTimer->stop();
        if (m_gimbalDial) m_gimbalDial->setValue(90, 120);
        if (m_gimbalStateLabel) {
            m_gimbalStateLabel->setText(QStringLiteral("✅ 水平 90° / 俯仰 120° (正对前方)"));
            m_gimbalStateLabel->setStyleSheet(QString(
                "font-size:13px;font-weight:bold;color:%1;background:%2;border-radius:10px;padding:8px;")
                .arg(C_TXT, C_CARD));
        }
    }
}
void MainWindow::onDeviceDiscovered(const QBluetoothDeviceInfo &d)
{
    QString n = d.name(), a = d.address().toString();
    bool tg = isTarget(n);
    QString l = n.isEmpty() ? a : (tg ? QStringLiteral("⭐ %1").arg(n) : QStringLiteral("   %1").arg(n));
    for (int i = 0; i < m_devList->count(); i++)
        if (m_devList->item(i)->data(Qt::UserRole).toString() == a) { m_devList->item(i)->setText(l); return; }
    auto *it = new QListWidgetItem(l); it->setData(Qt::UserRole, a); it->setSizeHint(QSize(0,34));
    if (tg) { it->setBackground(QColor(QStringLiteral("#152818"))); it->setForeground(QColor(C_GREEN));
        QFont f=it->font(); f.setBold(true); it->setFont(f); m_devList->insertItem(0,it); m_devList->setCurrentRow(0);
        appendLog(QStringLiteral("🎯 发现目标: %1").arg(n)); }
    else { it->setForeground(QColor(C_DIM)); m_devList->addItem(it); }
    if(m_devList->currentRow()<0&&m_devList->count()>0) m_devList->setCurrentRow(0);
}

// ── 舵机 ────────────────────────────────────────────────────
void MainWindow::onServoResponse(const ServoResponse &r) {
    switch (r.event) {
    case ServoResponse::SetPending:
        appendLog(QStringLiteral("⏳ SET:S%1:%2°?").arg(r.servoNum).arg(r.angle), C_ORANGE);
        m_servoWidget->onSetPending(r.servoNum, r.angle);
        break;
    case ServoResponse::Ok:
        appendLog(QStringLiteral("✅ OK:S%1:%2°").arg(r.servoNum).arg(r.angle), C_GREEN);
        m_servoWidget->onOk(r.servoNum, r.angle);
        break;
    case ServoResponse::Cancel:
        appendLog(QStringLiteral("❌ 已取消"), C_RED);
        m_servoWidget->onCancel();
        break;
    case ServoResponse::Switched:
        appendLog(QStringLiteral("🔀 已切换到舵机 %1").arg(r.servoNum), C_GREEN);
        m_servoWidget->onSwitched(r.servoNum, r.angle);
        break;
    default: break;
    }
}
void MainWindow::onServoAngleRequested(int srv, int a) { m_bt->sendServoAngle(srv, a); appendLog(QStringLiteral("📤 S%1:%2°").arg(srv).arg(a), C_BLUE); }

// ── PID ─────────────────────────────────────────────────────
void MainWindow::onPidMessage(const PidMessage &m)
{
    m_pidWidget->handleMessage(m);
    switch (m.type) {
    case PidWave: break;
    case PidKp:
        m_pidWidget->kpLabel()->setText(QString::number(m.params.kp));
        appendLog(QStringLiteral("Kp=%1").arg(m.params.kp)); break;
    case PidKi:
        m_pidWidget->kiLabel()->setText(QString::number(m.params.ki));
        appendLog(QStringLiteral("Ki=%1").arg(m.params.ki)); break;
    case PidKd:
        m_pidWidget->kdLabel()->setText(QString::number(m.params.kd / 10.0, 'f', 1));
        appendLog(QStringLiteral("Kd=%1").arg(m.params.kd / 10.0, 0, 'f', 1)); break;
    case PidTarget:
        m_pidWidget->tgtLabel()->setText(QString::number(m.params.targetRpm));
        appendLog(QStringLiteral("Target=%1 RPM").arg(m.params.targetRpm)); break;
    case PidGo: appendLog(QStringLiteral("▶ GO"), C_GREEN); break;
    case PidStop: appendLog(QStringLiteral("■ STOP"), C_RED); break;
    default: break;
    }
}
void MainWindow::onPidCommandRequested(const QString &c) { m_bt->sendPidCmd(c); appendLog(QStringLiteral("📤 %1").arg(c), C_BLUE); }

// ── 电机 ─────────────────────────────────────────────────────
void MainWindow::onMotorResponse(const MotorResponse &r)
{
    m_motorWidget->onMotorResponse(r);
    appendLog(QStringLiteral("⚙ 电机%1: %2% %3")
        .arg(QChar('A' + r.motorNum))
        .arg(r.speed)
        .arg(r.dir ? QStringLiteral("正转") : QStringLiteral("反转")), C_GREEN);
}
void MainWindow::onMotorCmdRequested(int motorNum, int speed, int dir)
{
    m_bt->sendMotorCmd(motorNum, speed, dir);
    appendLog(QStringLiteral("📤 M%1:%2:%3").arg(motorNum).arg(speed).arg(dir), C_BLUE);
}

// ── IMU 姿态 / 编码器转速 ─────────────────────────────────────
void MainWindow::onTelemetry(const TelemetryData &d)
{
    if (d.type == 1) {  // IMU: "R,x,P,y,Y,z,520" (v6.10: 只显示在遥控页数据区)
        if (m_joyImuLabel)
            m_joyImuLabel->setText(QStringLiteral("R: %1°  P: %2°  Y: %3°")
                .arg(d.imu.roll, 0, 'f', 1)
                .arg(d.imu.pitch, 0, 'f', 1)
                .arg(d.imu.yaw, 0, 'f', 1));
    } else if (d.type == 3 && m_encLabel) {  // v6.4 编码器: "E,e0,e1,e2,e3,520"
        m_encLabel->setText(QStringLiteral("A: %1   B: %2   C: %3   D: %4 RPM")
            .arg(d.encoder.motors[0].rpm)
            .arg(d.encoder.motors[1].rpm)
            .arg(d.encoder.motors[2].rpm)
            .arg(d.encoder.motors[3].rpm));
    }
}

// ── 按钮 ────────────────────────────────────────────────────
void MainWindow::onScanClicked()
{
    if(m_bt->state()==BluetoothClient::Scanning) { m_bt->stopScan(); m_btnScan->setText(QStringLiteral("扫描")); }
    else { m_devList->clear(); m_bt->startScan(); m_btnScan->setText(QStringLiteral("停止")); }
}
void MainWindow::onConnectClicked()
{
    auto *c=m_devList->currentItem(); if(!c){appendLog(QStringLiteral("⚠ 选择设备"),C_ORANGE);return;}
    if(m_bt->state()==BluetoothClient::Scanning) m_bt->stopScan();
    m_lastAddr = c->data(Qt::UserRole).toString();   // v6.10 记住供重连
    m_userDisconnect = false;   // v6.14 手动连接优先于自动重连
    m_bt->connectToAddress(m_lastAddr);
}
void MainWindow::onReconnectClicked()
{
    if (m_lastAddr.isEmpty()) return;
    appendLog(QStringLiteral("↻ 重连上次设备 %1").arg(m_lastAddr), C_ORANGE);
    m_userDisconnect = false;   // v6.14 手动连接优先于自动重连
    m_bt->connectToAddress(m_lastAddr);
}

// ── WiFi 摄像头 (v6.12) ──────────────────────────────────────
void MainWindow::onCamConnectClicked()
{
    if (!m_cameraWidget) return;
    m_cameraWidget->startStream(m_camIpEdit->text().trimmed());
}
void MainWindow::onCamDisconnectClicked()
{
    if (!m_cameraWidget) return;
    m_cameraWidget->stopStream();
    m_camStatusLabel->setText(QStringLiteral("状态: 已断开"));
    m_camStatusLabel->setStyleSheet(QString("font-size:11px;font-weight:bold;color:%1;background:transparent;").arg(C_DIM));
    m_btnCamConnect->setEnabled(true);
    m_btnCamDisconnect->setEnabled(false);
}
// Android 11+(API29+) 弹系统 WiFi 切换面板, 不离开 APP; 低版本跳 WiFi 设置页
void MainWindow::onOpenWifiSettings()
{
#if defined(Q_OS_ANDROID)
    QJniObject ctx = QNativeInterface::QAndroidApplication::context();
    const int sdk = QNativeInterface::QAndroidApplication::sdkVersion();
    if (sdk >= 29) {
        QJniObject action = QJniObject::getStaticObjectField(
            "android/provider/Settings$Panel", "ACTION_WIFI", "Ljava/lang/String;");
        QJniObject intent("android/content/Intent", "(Ljava/lang/String;)V", action.object());
        ctx.callMethod<void>("startActivity", "(Landroid/content/Intent;)V", intent.object());
    } else {
        QJniObject action = QJniObject::getStaticObjectField(
            "android/provider/Settings", "ACTION_WIFI_SETTINGS", "Ljava/lang/String;");
        QJniObject intent("android/content/Intent", "(Ljava/lang/String;)V", action.object());
        ctx.callMethod<void>("startActivity", "(Landroid/content/Intent;)V", intent.object());
    }
#endif
}

// 悬浮小窗开关: 显示 + 启动 10Hz 帧刷新; 关闭即停
void MainWindow::onMiniCamToggled(bool on)
{
    if (!m_miniCam) return;
    if (on) {
        m_miniCam->show();
        m_miniCam->raise();
        m_miniCamTimer->start();
    } else {
        m_miniCam->hide();
        m_miniCamTimer->stop();
    }
}
void MainWindow::onDisconnectClicked()
{
    m_userDisconnect = true;   // v6.14 主动断开不触发自动重连
    m_bt->disconnect();
    m_servoWidget->reset(); m_pidWidget->reset(); m_motorWidget->reset();
}

// v6.14 断线自动重连: 2.5s 间隔, 最多 20 次 (给固件复位/恢复留时间)
void MainWindow::startAutoReconnect()
{
    if (m_lastAddr.isEmpty()) return;
    if (!m_reconnectTimer) {
        m_reconnectTimer = new QTimer(this);
        m_reconnectTimer->setInterval(2500);
        connect(m_reconnectTimer, &QTimer::timeout, this, [this]() {
            if (m_bt->isConnected()) { stopAutoReconnect(); return; }
            if (m_bt->state() == BluetoothClient::Connecting) return;   // 上次尝试未结束, 跳过本轮
            if (++m_reconnectCount > 20) {
                stopAutoReconnect();
                appendLog(QStringLiteral("⛔ 自动重连 20 次仍失败 — 检查机器人供电/固件后手动重连"), C_RED);
                return;
            }
            appendLog(QStringLiteral("↻ 自动重连第 %1 次 %2").arg(m_reconnectCount).arg(m_lastAddr), C_ORANGE);
            m_bt->connectToAddress(m_lastAddr);
        });
    }
    if (!m_reconnectTimer->isActive()) {
        m_reconnectCount = 0;
        m_reconnectTimer->start();
        appendLog(QStringLiteral("🔁 连接断开, 2.5s 后自动重连 (20次上限)"), C_ORANGE);
    }
}
void MainWindow::stopAutoReconnect()
{
    if (m_reconnectTimer) m_reconnectTimer->stop();
    m_reconnectCount = 0;
}

// ── UI ──────────────────────────────────────────────────────
void MainWindow::setupUi()
{
    setWindowTitle(QStringLiteral("🔧 四足机器人调试助手 v6.16"));
    resize(800, 460);
    QPalette pal; pal.setColor(QPalette::Window, QColor(C_BG)); setPalette(pal); setAutoFillBackground(true);
    auto *cw=new QWidget; cw->setStyleSheet(QString("background:%1;").arg(C_BG));
    setCentralWidget(cw);
    auto *root=new QHBoxLayout(cw); root->setContentsMargins(8,6,8,6); root->setSpacing(8);

    // ── 左侧竖栏导航 (v6.10 横屏) ──
    auto *nav=new QWidget;
    nav->setFixedWidth(96);
    nav->setStyleSheet(QString("background:%1;border-radius:12px;").arg(C_CARD));
    auto *navL=new QVBoxLayout(nav); navL->setContentsMargins(6,8,6,8); navL->setSpacing(6);
    const QString tabNames[6] = { QStringLiteral("连接"), QStringLiteral("矫正"), QStringLiteral("运动"), QStringLiteral("遥控"), QStringLiteral("云台"), QStringLiteral("图传") };
    for (int i = 0; i < 6; i++) {
        m_tabBtns[i] = new QPushButton(tabNames[i]);
        m_tabBtns[i]->setMinimumHeight(38);
        int idx = i;
        connect(m_tabBtns[i], &QPushButton::clicked, this, [this, idx]() { setPage(idx); });
        navL->addWidget(m_tabBtns[i]);
    }
    navL->addStretch();
    root->addWidget(nav);

    // ── 右区: 顶部状态条 + 页面栈 ──
    auto *rightW=new QWidget;
    auto *rightL=new QVBoxLayout(rightW); rightL->setContentsMargins(0,0,0,0); rightL->setSpacing(6);

    // 顶部状态条 (v6.10: IMU 移入遥控页, 此处只留连接状态)
    auto *hdr=new QWidget;
    hdr->setStyleSheet(QString("background:%1;border-radius:12px;").arg(C_CARD));
    auto *hdrL=new QHBoxLayout(hdr); hdrL->setContentsMargins(12,6,12,6); hdrL->setSpacing(8);
    m_statusIcon=new QLabel(QStringLiteral("⚫")); m_statusIcon->setStyleSheet(QStringLiteral("font-size:16px;background:transparent;"));
    m_statusLabel=new QLabel(QStringLiteral("未连接")); m_statusLabel->setStyleSheet(QString("font-size:12px;font-weight:bold;color:%1;background:transparent;").arg(C_DIM));
    m_devAddrLabel=new QLabel; m_devAddrLabel->setStyleSheet(QString("font-size:11px;color:%1;background:transparent;").arg(C_DIM));
    hdrL->addWidget(m_statusIcon);
    hdrL->addWidget(m_statusLabel);
    hdrL->addWidget(m_devAddrLabel, 1);
    m_btnReconnect=mkB(QStringLiteral("↻ 重连"), C_ORANGE);
    m_btnReconnect->setFixedWidth(92);
    m_btnReconnect->setVisible(false);
    connect(m_btnReconnect,&QPushButton::clicked,this,&MainWindow::onReconnectClicked);
    hdrL->addWidget(m_btnReconnect);
    rightL->addWidget(hdr);

    m_btnScan=mkB(QStringLiteral("扫描"),C_ACCENT); m_btnConnect=mkB(QStringLiteral("连接"),C_BLUE); m_btnDisconnect=mkB(QStringLiteral("断开"),C_ORANGE);
    connect(m_btnScan,&QPushButton::clicked,this,&MainWindow::onScanClicked);
    connect(m_btnConnect,&QPushButton::clicked,this,&MainWindow::onConnectClicked);
    connect(m_btnDisconnect,&QPushButton::clicked,this,&MainWindow::onDisconnectClicked);
    m_devList=new QListWidget;
    m_devList->setStyleSheet(QString("QListWidget{font-size:12px;border:1px solid %1;border-radius:12px;background:%2;color:%3;padding:2px;}"
        "QListWidget::item{padding:6px 6px;margin:2px 0;border-radius:8px;}" "QListWidget::item:selected{background:%4;color:#fff;}")
        .arg(C_ACCENT,C_CARD,C_TXT,C_BLUE));
    connect(m_devList,&QListWidget::itemDoubleClicked,this,[this](){onConnectClicked();});

    // ── 舵机/PID (实例先建好, 信号连接) ──
    m_servoWidget = new ServoWidget;
    connect(m_servoWidget, &ServoWidget::servoAngleRequested, this, &MainWindow::onServoAngleRequested);
    connect(m_servoWidget, &ServoWidget::confirmYes, this, [this]() { m_bt->sendRawText(QStringLiteral("Y")); appendLog(QStringLiteral("📤 Y"), C_BLUE); });
    connect(m_servoWidget, &ServoWidget::confirmNo,  this, [this]() { m_bt->sendRawText(QStringLiteral("N")); appendLog(QStringLiteral("📤 N"), C_RED); });
    connect(m_servoWidget, &ServoWidget::switchServo,  this, [this](int srv) { m_bt->sendServoSwitch(srv); appendLog(QStringLiteral("📤 切换舵机 %1").arg(srv), C_BLUE); });

    m_pidWidget = new PidWidget;
    connect(m_pidWidget, &PidWidget::commandRequested, this, &MainWindow::onPidCommandRequested);

    // ── 电机控制 ──
    m_motorWidget = new MotorWidget;
    connect(m_motorWidget, &MotorWidget::motorCmdRequested, this, &MainWindow::onMotorCmdRequested);
    connect(m_motorWidget, &MotorWidget::rollAllRequested, this, [this]() { m_bt->sendRawText(QStringLiteral("R")); appendLog(QStringLiteral("📤 R 前进"), C_BLUE); });
    connect(m_motorWidget, &MotorWidget::backAllRequested, this, [this]() { m_bt->sendRawText(QStringLiteral("B")); appendLog(QStringLiteral("📤 B 后退"), C_BLUE); });
    connect(m_motorWidget, &MotorWidget::stopAllRequested, this, [this]() { m_bt->sendRawText(QStringLiteral("S")); appendLog(QStringLiteral("📤 S 全停"), C_RED); });

    // ════════════════════════════════════════════════════════
    //  页面栈 (连接 / 矫正 / 运动 / 遥控), 导航在左侧竖栏
    // ════════════════════════════════════════════════════════
    m_stack = new QStackedWidget;
    rightL->addWidget(m_stack, 1);
    root->addWidget(rightW, 1);

    // ── 页0: 连接 (左右分栏: 蓝牙+摄像头 | 日志) ──
    auto *connW = new QWidget;
    auto *connL = new QHBoxLayout(connW); connL->setContentsMargins(0, 0, 0, 0); connL->setSpacing(8);
    auto *leftCol = new QVBoxLayout; leftCol->setSpacing(8);

    auto *btG=new QGroupBox; sGrp(btG,QStringLiteral("蓝牙 (HC-05 115200bps)"));
    auto *btL=new QVBoxLayout(btG); auto *bR=new QHBoxLayout;
    bR->addWidget(m_btnScan); bR->addWidget(m_btnConnect); bR->addWidget(m_btnDisconnect);
    btL->addLayout(bR);
    btL->addWidget(m_devList, 1);
    leftCol->addWidget(btG, 3);

    // WiFi 摄像头连接组 (v6.12, 与蓝牙并列)
    auto *camG = new QGroupBox; sGrp(camG, QStringLiteral("WiFi 摄像头 (热点 Yahboom_ESP32_WIFI)"));
    auto *camL = new QVBoxLayout(camG); camL->setSpacing(6);
    m_camIpEdit = new QLineEdit(QStringLiteral("192.168.4.1"));
    m_camIpEdit->setStyleSheet(QString(
        "QLineEdit{font-size:11px;color:%1;background:%2;border:1px solid %3;"
        "border-radius:8px;padding:4px 8px;}").arg(C_TXT, C_BG, C_ACCENT));
    camL->addWidget(m_camIpEdit);
    auto *camRow = new QHBoxLayout; camRow->setSpacing(6);
    m_btnCamConnect = mkB(QStringLiteral("连接"), C_GREEN);
    m_btnCamDisconnect = mkB(QStringLiteral("断开"), C_RED);
    m_btnCamDisconnect->setEnabled(false);
    m_btnCamWifi = mkB(QStringLiteral("📶 切WiFi"), C_BLUE);
    connect(m_btnCamConnect, &QPushButton::clicked, this, &MainWindow::onCamConnectClicked);
    connect(m_btnCamDisconnect, &QPushButton::clicked, this, &MainWindow::onCamDisconnectClicked);
    connect(m_btnCamWifi, &QPushButton::clicked, this, &MainWindow::onOpenWifiSettings);
    camRow->addWidget(m_btnCamConnect); camRow->addWidget(m_btnCamDisconnect); camRow->addWidget(m_btnCamWifi);
    camL->addLayout(camRow);
    m_camStatusLabel = new QLabel(QStringLiteral("状态: 未连接"));
    m_camStatusLabel->setStyleSheet(QString("font-size:11px;font-weight:bold;color:%1;background:transparent;").arg(C_DIM));
    camL->addWidget(m_camStatusLabel);
    leftCol->addWidget(camG, 2);

    connL->addLayout(leftCol, 2);

    auto *lgG = new QGroupBox; sGrp(lgG, QStringLiteral("运行日志"));
    auto *lgL = new QVBoxLayout(lgG); lgL->setContentsMargins(2, 2, 2, 2);
    m_logView=new QTextEdit; m_logView->setReadOnly(true);
    m_logView->setStyleSheet(QString("QTextEdit{font-size:11px;font-family:Consolas,monospace;color:%1;background:%2;border:none;}").arg(C_GREEN,C_BG));
    lgL->addWidget(m_logView);
    connL->addWidget(lgG, 3);
    m_stack->addWidget(connW);  // index 0

    // ── 页1: 矫正 (可滚动) ──
    auto *calW = new QWidget;
    auto *calSc = new QScrollArea;
    calSc->setWidget(calW); calSc->setWidgetResizable(true);
    calSc->setFrameShape(QFrame::NoFrame);
    calSc->setStyleSheet(QString("QScrollArea{background:%1;border:none;}").arg(C_BG));
    auto *calL = new QVBoxLayout(calW); calL->setContentsMargins(0, 0, 0, 0); calL->setSpacing(6);
    auto *servoG = new QGroupBox; sGrp(servoG, QStringLiteral("舵机校准 (上电后、发 G/T 前使用)"));
    auto *servoL = new QVBoxLayout(servoG);
    servoL->setContentsMargins(2, 2, 2, 2);
    servoL->addWidget(m_servoWidget);
    calL->addWidget(servoG);
    calL->addStretch();
    m_stack->addWidget(calSc);  // index 1

    // ── 页2: 运动 (v6.10 三栏: 步态 | 修正+电机 | 坡度) ──
    auto *ctrlW = new QWidget;
    auto *ctrlSc = new QScrollArea;
    ctrlSc->setWidget(ctrlW); ctrlSc->setWidgetResizable(true);
    ctrlSc->setFrameShape(QFrame::NoFrame);
    ctrlSc->setStyleSheet(QString("QScrollArea{background:%1;border:none;}").arg(C_BG));
    auto *ctrlL = new QHBoxLayout(ctrlW); ctrlL->setContentsMargins(0, 0, 0, 0); ctrlL->setSpacing(8);

    // 步态
    auto *gaitG = new QGroupBox; sGrp(gaitG, QStringLiteral("步态控制"));
    auto *gaitGrid = new QGridLayout(gaitG); gaitGrid->setSpacing(6);
    auto mkGait = [&](const QString &t, const QString &bg, const QString &cmd, int r, int c) {
        auto *b = mkB(t, bg);
        b->setMinimumHeight(28);
        connect(b, &QPushButton::clicked, this, [this, cmd]() { m_bt->sendRawText(cmd); appendLog(QStringLiteral("📤 %1").arg(cmd), C_BLUE); });
        gaitGrid->addWidget(b, r, c);
    };
    mkGait(QStringLiteral("G 站姿"),   C_GREEN, QStringLiteral("G"), 0, 0);
    mkGait(QStringLiteral("H 高站姿"), QStringLiteral("#8b5cf6"), QStringLiteral("H"), 0, 1);
    mkGait(QStringLiteral("K 前顶"),   QStringLiteral("#06b6d4"), QStringLiteral("K"), 0, 2);
    mkGait(QStringLiteral("L 前顶趴"), QStringLiteral("#ec4899"), QStringLiteral("L"), 0, 3);
    mkGait(QStringLiteral("M 后顶趴"), QStringLiteral("#f59e0b"), QStringLiteral("M"), 1, 0);
    mkGait(QStringLiteral("P 后顶站"), QStringLiteral("#84cc16"), QStringLiteral("P"), 1, 1);
    mkGait(QStringLiteral("A 单步"),   C_ORANGE, QStringLiteral("A"), 1, 2);
    mkGait(QStringLiteral("T 行走"),   C_BLUE, QStringLiteral("T"), 1, 3);
    mkGait(QStringLiteral("E 顺转"),   QStringLiteral("#d946ef"), QStringLiteral("E"), 2, 0);
    mkGait(QStringLiteral("E 逆转"),   QStringLiteral("#c026d3"), QStringLiteral("E:0"), 2, 1);
    mkGait(QStringLiteral("W 越障"),   QStringLiteral("#14b8a6"), QStringLiteral("W"), 2, 2);
    mkGait(QStringLiteral("C 贴地趴"), QStringLiteral("#6366f1"), QStringLiteral("C"), 2, 3);
    mkGait(QStringLiteral("U 站起"),   QStringLiteral("#10b981"), QStringLiteral("U"), 3, 0);
    mkGait(QStringLiteral("O 坐姿"),   QStringLiteral("#8b5cf6"), QStringLiteral("O"), 3, 1);
    mkGait(QStringLiteral("T 倒走"),   QStringLiteral("#0ea5e9"), QStringLiteral("T:-1"), 3, 2);
    mkGait(QStringLiteral("H:230 蹲低"), QStringLiteral("#a78bfa"), QStringLiteral("H:230"), 3, 3);
    ctrlL->addWidget(gaitG, 2);

    // 中栏: 前倾修正 + 电机
    auto *midL = new QVBoxLayout; midL->setSpacing(8);

    // 前倾修正
    auto *shiftG = new QGroupBox; sGrp(shiftG, QStringLiteral("前倾修正 (足端前移)"));
    auto *shiftL = new QVBoxLayout(shiftG); shiftL->setSpacing(4);
    auto *shiftInfo = new QHBoxLayout;
    m_shiftLabel = new QLabel(QStringLiteral("当前: %1 mm").arg(m_footShift));
    m_shiftLabel->setStyleSheet(QString("font-size:11px;font-weight:bold;color:%1;background:transparent;").arg(C_BLUE));
    shiftInfo->addWidget(m_shiftLabel); shiftInfo->addStretch();
    shiftL->addLayout(shiftInfo);
    auto *shiftRow = new QHBoxLayout; shiftRow->setSpacing(6);
    auto *shiftSlider = new QSlider(Qt::Horizontal);
    shiftSlider->setRange(-30, 30);
    shiftSlider->setValue(m_footShift);
    shiftSlider->setStyleSheet(QString(
        "QSlider::groove:horizontal{height:6px;background:%1;border-radius:3px;}"
        "QSlider::handle:horizontal{width:24px;height:24px;margin:-9px 0;"
        "background:%2;border-radius:12px;}"
        "QSlider::sub-page:horizontal{background:%2;border-radius:3px;}")
        .arg(C_ACCENT, C_BLUE));
    connect(shiftSlider, &QSlider::valueChanged, this, [this](int v) {
        m_footShift = v;
        m_shiftLabel->setText(QStringLiteral("当前: %1 mm").arg(v));
    });
    shiftRow->addWidget(shiftSlider, 1);
    auto *shiftSend = mkB(QStringLiteral("发送"), C_BLUE);
    shiftSend->setFixedWidth(64);
    connect(shiftSend, &QPushButton::clicked, this, [this]() {
        m_bt->sendRawText(QStringLiteral("X:%1").arg(m_footShift));
        appendLog(QStringLiteral("📤 X:%1").arg(m_footShift), C_BLUE);
    });
    shiftRow->addWidget(shiftSend);
    shiftL->addLayout(shiftRow);
    midL->addWidget(shiftG);

    midL->addWidget(m_motorWidget);
    midL->addStretch();
    ctrlL->addLayout(midL, 2);

    // 站立自稳 (v6.12: Z 标定 + L:1/L:0 开关; 坡度自适应已随固件删除)
    auto *slopeG = new QGroupBox; sGrp(slopeG, QStringLiteral("站立自稳 (H/K 高站姿, 爬陡坡用)"));
    auto *slopeGL = new QVBoxLayout(slopeG); slopeGL->setSpacing(6);

    auto *slopeTop = new QHBoxLayout;
    m_slopeState = new QLabel(QStringLiteral("标定: 未标定"));
    m_slopeState->setStyleSheet(QString("font-size:11px;font-weight:bold;color:%1;background:transparent;").arg(C_DIM));
    slopeTop->addWidget(m_slopeState);
    auto *btnZ = mkB(QStringLiteral("Z 标定"), C_BLUE);
    btnZ->setFixedWidth(80);
    connect(btnZ, &QPushButton::clicked, this, [this]() {
        m_bt->sendRawText(QStringLiteral("Z"));
        appendLog(QStringLiteral("📤 Z (自稳标定)"), C_BLUE);
    });
    slopeTop->addStretch();
    slopeTop->addWidget(btnZ);
    slopeGL->addLayout(slopeTop);

    // 自稳开关 (v6.12: L:1/L:0, 先 Z 标定; 单独 L 仍是低趴姿态)
    auto *lvTop = new QHBoxLayout;
    m_lvState = new QLabel(QStringLiteral("自稳: 关闭"));
    m_lvState->setStyleSheet(QString("font-size:11px;font-weight:bold;color:%1;background:transparent;").arg(C_DIM));
    lvTop->addWidget(m_lvState);
    lvTop->addStretch();
    m_btnLv = mkB(QStringLiteral("L:1 开"), C_GREEN);
    m_btnLv->setFixedWidth(80);
    connect(m_btnLv, &QPushButton::clicked, this, [this]() {
        const QString cmd = m_lvOn ? QStringLiteral("L:0") : QStringLiteral("L:1");
        m_bt->sendRawText(cmd);
        appendLog(QStringLiteral("📤 %1 (自稳)").arg(cmd), C_BLUE);
    });
    lvTop->addWidget(m_btnLv);
    slopeGL->addLayout(lvTop);

    auto *lvHint = new QLabel(QStringLiteral(
        "流程: 平地站姿 → Z 标定(静止2s) → L:1 开自稳 · 爬陡坡: V模式1 接近坡底 → H:230 边开边蹲 → 回平地 H 站起\n"
        "单腿补偿上限随站高: 280≈25mm(7.5°) / 230≈75mm(23°) · 死区±1° · 行走/翻膝/收纳中不叠加"));
    lvHint->setWordWrap(true);
    lvHint->setStyleSheet(QString("font-size:10px;color:%1;background:transparent;").arg(C_DIM));
    slopeGL->addWidget(lvHint);
    ctrlL->addWidget(slopeG, 2);
    m_stack->addWidget(ctrlSc);  // index 2

    // ── 页3: 遥控 (v6.9 横屏方案: 状态栏+方向+摇杆+按钮区+姿态条) ──
    auto *joyW = new QWidget;
    auto *joySc = new QScrollArea;
    joySc->setWidget(joyW); joySc->setWidgetResizable(true);
    joySc->setFrameShape(QFrame::NoFrame);
    joySc->setStyleSheet(QString("QScrollArea{background:%1;border:none;}").arg(C_BG));
    auto *joyL = new QVBoxLayout(joyW); joyL->setContentsMargins(0, 0, 0, 0); joyL->setSpacing(6);

    // 状态栏 (回显驱动, 协议横屏遥控方案)
    auto *statBar = new QWidget;
    statBar->setStyleSheet(QString("background:%1;border-radius:8px;").arg(C_CARD));
    auto *statL = new QHBoxLayout(statBar); statL->setContentsMargins(10, 6, 10, 6); statL->setSpacing(8);
    m_poseLabel = new QLabel(QStringLiteral("姿态: 未同步"));
    m_poseLabel->setStyleSheet(QString("font-size:11px;font-weight:bold;color:%1;background:transparent;").arg(C_BLUE));
    m_actLabel = new QLabel(QStringLiteral("行为: 未同步"));
    m_actLabel->setStyleSheet(QString("font-size:11px;font-weight:bold;color:%1;background:transparent;").arg(C_DIM));
    statL->addWidget(m_poseLabel, 1);
    statL->addWidget(m_actLabel, 1);
    joyL->addWidget(statBar);

    // 方向指示 (APP 自算, 协议映射表)
    m_dirLabel = new QLabel(QStringLiteral("⏹ 停止"));
    m_dirLabel->setAlignment(Qt::AlignCenter);
    m_dirLabel->setStyleSheet(QString(
        "font-size:16px;font-weight:bold;color:%1;background:%2;border-radius:10px;padding:5px;").arg(C_DIM, C_CARD));
    joyL->addWidget(m_dirLabel);

    // 主体: 左摇杆 | 右按钮区
    auto *mainRow = new QHBoxLayout; mainRow->setSpacing(8);

    // 左: 虚拟摇杆
    auto *joyG = new QGroupBox; sGrp(joyG, QStringLiteral("摇杆驾驶 (V 命令, 松开回中)"));
    auto *joyGL = new QVBoxLayout(joyG); joyGL->setSpacing(4);
    m_joy = new JoystickWidget;
    m_joy->setMinimumSize(110, 110);
    joyGL->addWidget(m_joy, 0, Qt::AlignCenter);
    m_joyValLabel = new QLabel(QStringLiteral("V: 0 : 0   占空比 0% / 0%"));
    m_joyValLabel->setAlignment(Qt::AlignCenter);
    m_joyValLabel->setStyleSheet(QString(
        "font-size:12px;font-weight:bold;font-family:monospace;"
        "color:%1;background:transparent;").arg(C_BLUE));
    joyGL->addWidget(m_joyValLabel);
    auto *joyHint = new QLabel(QStringLiteral("↑↓ 前进/后退  ←→ 左转/右转\nV值=占空比%(1:1) 无回显"));
    joyHint->setAlignment(Qt::AlignCenter);
    joyHint->setStyleSheet(QString("font-size:11px;color:%1;background:transparent;").arg(C_DIM));
    joyGL->addWidget(joyHint);
    // v6.12 驾驶模式切换: 模式0 倾斜平衡 / 模式1 无倾斜+自稳保持 (爬陡坡轮盘)
    m_btnVMode = mkB(QStringLiteral("模式0 倾斜平衡"), C_ACCENT);
    m_btnVMode->setMinimumHeight(30);
    connect(m_btnVMode, &QPushButton::clicked, this, [this]() {
        m_vMode1 = !m_vMode1;
        m_btnVMode->setText(m_vMode1 ? QStringLiteral("模式1 自稳保持") : QStringLiteral("模式0 倾斜平衡"));
        m_btnVMode->setStyleSheet(mkStyleB(m_vMode1 ? C_ORANGE : C_ACCENT));
        appendLog(m_vMode1 ? QStringLiteral("🔄 驾驶模式 → 1 (无倾斜+自稳保持, 反折膝先翻膝)")
                           : QStringLiteral("🔄 驾驶模式 → 0 (倾斜平衡)"), C_ORANGE);
    });
    joyGL->addWidget(m_btnVMode);
    mainRow->addWidget(joyG, 2);
    connect(m_joy, &JoystickWidget::joystickMoved, this, [this](int sp, int st) {
        // v6.12 模式1 (无倾斜补偿+自稳保持): 带 :1 后缀; 模式0 倾斜平衡: 两参
        m_bt->sendRawText(m_vMode1 ? QStringLiteral("V:%1:%2:1").arg(sp).arg(st)
                                   : QStringLiteral("V:%1:%2").arg(sp).arg(st));  // 100ms 高频, 不刷日志
        // 1:1 映射: V 值即占空比% (协议 v6.4)
        m_joyValLabel->setText(QStringLiteral("V: %1 : %2%3   占空比 %4% / %5%")
            .arg(sp).arg(st)
            .arg(m_vMode1 ? QStringLiteral(" : 1") : QString())
            .arg(qAbs(sp)).arg(qAbs(st)));
        setActState(QStringLiteral("轮式驾驶"), C_BLUE);
        // 方向显示: 协议映射表 (APP 自算, 10Hz 同步刷新)
        QString d;
        if (sp > 0 && st == 0)      d = QStringLiteral("⬆ 前进");
        else if (sp < 0 && st == 0) d = QStringLiteral("⬇ 后退");
        else if (sp == 0 && st > 0) d = QStringLiteral("↻ 原地右转");
        else if (sp == 0 && st < 0) d = QStringLiteral("↺ 原地左转");
        else if (sp > 0 && st > 0)  d = QStringLiteral("↗ 右前");
        else if (sp > 0 && st < 0)  d = QStringLiteral("↖ 左前");
        else if (sp < 0 && st > 0)  d = QStringLiteral("↘ 右后");
        else                        d = QStringLiteral("↙ 左后");
        setDirState(d, C_BLUE);
    });
    connect(m_joy, &JoystickWidget::joystickCentered, this, [this]() {
        m_bt->sendRawText(QStringLiteral("V:0:0"));  // 回中 → 固件渐停+回平
        m_joyValLabel->setText(QStringLiteral("V: 0 : 0   (回中渐停)"));
        appendLog(QStringLiteral("🎮 摇杆回中 V:0:0"), C_DIM);
        setActState(QStringLiteral("站立"), C_GREEN);
        setDirState(QStringLiteral("⏹ 停止"), C_DIM);
    });

    // 右: 按钮区 (数据开关 + 步态 + 轮式 + 实时数据)
    auto *btnCol = new QVBoxLayout; btnCol->setSpacing(6);

    // 数据流开关 I/J
    auto *datG = new QGroupBox; sGrp(datG, QStringLiteral("数据流"));
    auto *datL = new QHBoxLayout(datG); datL->setSpacing(6);
    auto *btnImu = mkB(QStringLiteral("IMU I"), C_BLUE);
    auto *btnEnc = mkB(QStringLiteral("转速 J"), C_ORANGE);
    btnImu->setMinimumHeight(28); btnEnc->setMinimumHeight(28);
    connect(btnImu, &QPushButton::clicked, this, [this]() { m_bt->sendRawText(QStringLiteral("I")); });
    connect(btnEnc, &QPushButton::clicked, this, [this]() { m_bt->sendRawText(QStringLiteral("J")); });
    datL->addWidget(btnImu); datL->addWidget(btnEnc);
    btnCol->addWidget(datG);

    // 步态按钮 T/W/E顺/E逆 (低趴置灰, 协议交互提醒)
    auto *gaitG2 = new QGroupBox; sGrp(gaitG2, QStringLiteral("步态 (高站姿用)"));
    auto *gaitGrid2 = new QGridLayout(gaitG2); gaitGrid2->setSpacing(6);
    struct GaitDef { const QString t, bg, cmd; };
    const GaitDef gaits[4] = {
        { QStringLiteral("T 行走"), C_BLUE, QStringLiteral("T") },
        { QStringLiteral("W 越障"), QStringLiteral("#14b8a6"), QStringLiteral("W") },
        { QStringLiteral("E 顺转"), QStringLiteral("#d946ef"), QStringLiteral("E") },
        { QStringLiteral("E 逆转"), QStringLiteral("#c026d3"), QStringLiteral("E:0") },
    };
    for (int i = 0; i < 4; i++) {
        auto *b = mkB(gaits[i].t, gaits[i].bg);
        b->setMinimumHeight(28);
        const QString cmd = gaits[i].cmd;
        connect(b, &QPushButton::clicked, this, [this, cmd]() { m_bt->sendRawText(cmd); appendLog(QStringLiteral("📤 %1").arg(cmd), C_BLUE); });
        gaitGrid2->addWidget(b, i / 2, i % 2);
        m_gaitBtns[i] = b;
    }
    btnCol->addWidget(gaitG2);

    // 轮式按钮 R/B/S (S 急停永远可用)
    auto *wheelG = new QGroupBox; sGrp(wheelG, QStringLiteral("轮式"));
    auto *wheelL = new QHBoxLayout(wheelG); wheelL->setSpacing(6);
    auto mkWheel = [&](const QString &t, const QString &bg, const QString &cmd) {
        auto *b = mkB(t, bg);
        b->setMinimumHeight(28);
        connect(b, &QPushButton::clicked, this, [this, cmd]() { m_bt->sendRawText(cmd); appendLog(QStringLiteral("📤 %1").arg(cmd), C_BLUE); });
        wheelL->addWidget(b, 1);
    };
    mkWheel(QStringLiteral("R 前"), C_GREEN, QStringLiteral("R"));
    mkWheel(QStringLiteral("B 后"), C_ORANGE, QStringLiteral("B"));
    mkWheel(QStringLiteral("S 停"), C_RED, QStringLiteral("S"));
    btnCol->addWidget(wheelG);

    // 实时数据 (I/J 流)
    auto *dataG = new QGroupBox; sGrp(dataG, QStringLiteral("实时数据 (I/J 流)"));
    auto *dataL = new QVBoxLayout(dataG); dataL->setSpacing(4);
    m_joyImuLabel = new QLabel(QStringLiteral("R: --  P: --  Y: --"));
    m_joyImuLabel->setStyleSheet(QString("font-size:11px;font-weight:bold;color:%1;background:transparent;").arg(C_BLUE));
    dataL->addWidget(m_joyImuLabel);
    m_encLabel = new QLabel(QStringLiteral("A:-- B:-- C:-- D:-- RPM"));
    m_encLabel->setStyleSheet(QString("font-size:11px;font-weight:bold;color:%1;background:transparent;").arg(C_ORANGE));
    dataL->addWidget(m_encLabel);
    btnCol->addWidget(dataG);
    btnCol->addStretch();
    mainRow->addLayout(btnCol, 3);
    joyL->addLayout(mainRow);

    // 底部姿态条 G/H/K/P/L/M/C/U/O (协议横屏方案 + v6.11 坐姿)
    auto *poseG = new QGroupBox; sGrp(poseG, QStringLiteral("姿态"));
    auto *poseGrid = new QGridLayout(poseG); poseGrid->setSpacing(6);
    struct PoseDef { const QString t, bg, cmd; };
    const PoseDef poses[9] = {
        { QStringLiteral("G 站姿"),  C_GREEN, QStringLiteral("G") },
        { QStringLiteral("H 高站姿"), QStringLiteral("#8b5cf6"), QStringLiteral("H") },
        { QStringLiteral("K 前顶"),  QStringLiteral("#06b6d4"), QStringLiteral("K") },
        { QStringLiteral("P 后顶站"), QStringLiteral("#84cc16"), QStringLiteral("P") },
        { QStringLiteral("L 前顶趴"), QStringLiteral("#ec4899"), QStringLiteral("L") },
        { QStringLiteral("M 后顶趴"), QStringLiteral("#f59e0b"), QStringLiteral("M") },
        { QStringLiteral("C 贴地趴"), QStringLiteral("#6366f1"), QStringLiteral("C") },
        { QStringLiteral("U 站起"),   QStringLiteral("#10b981"), QStringLiteral("U") },
        { QStringLiteral("O 坐姿"),   QStringLiteral("#8b5cf6"), QStringLiteral("O") },
    };
    for (int i = 0; i < 9; i++) {
        auto *b = mkB(poses[i].t, poses[i].bg);
        b->setMinimumHeight(28);
        const QString cmd = poses[i].cmd;
        connect(b, &QPushButton::clicked, this, [this, cmd]() { m_bt->sendRawText(cmd); appendLog(QStringLiteral("📤 %1").arg(cmd), C_BLUE); });
        poseGrid->addWidget(b, i / 5, i % 5);
    }
    joyL->addWidget(poseG);
    joyL->addStretch();
    m_stack->addWidget(joySc);  // index 3

    // ── 页4: 云台 (v6.14 左摄像头画面 + 右单轮盘: 点哪云台去哪, 5Hz 实时发送) ──
    auto *gimW = new QWidget;
    auto *gimL = new QHBoxLayout(gimW); gimL->setContentsMargins(12, 8, 12, 8); gimL->setSpacing(10);

    // 左: 摄像头画面 (frameUpdated 信号驱动, 等比缩放)
    m_gimbalCamLabel = new QLabel(QStringLiteral("📷 未连接摄像头\n\n在连接页「WiFi 摄像头」点连接"));
    m_gimbalCamLabel->setAlignment(Qt::AlignCenter);
    m_gimbalCamLabel->setMinimumSize(280, 220);
    m_gimbalCamLabel->setStyleSheet(QString(
        "font-size:11px;color:%1;background:%2;border:1px solid %3;border-radius:12px;")
        .arg(C_DIM, C_BG, C_ACCENT));
    gimL->addWidget(m_gimbalCamLabel, 1);

    // 右: 状态卡 + 轮盘 + 预设 + 说明
    auto *gimRight = new QVBoxLayout; gimRight->setSpacing(6);

    // 状态卡 (GM 回显驱动)
    m_gimbalStateLabel = new QLabel(QStringLiteral("✅ 水平 90° / 俯仰 120° (正对前方)"));
    m_gimbalStateLabel->setAlignment(Qt::AlignCenter);
    m_gimbalStateLabel->setStyleSheet(QString(
        "font-size:13px;font-weight:bold;color:%1;background:%2;border-radius:10px;padding:8px;")
        .arg(C_TXT, C_CARD));
    gimRight->addWidget(m_gimbalStateLabel);

    // 单轮盘: 触摸点相对圆心 → 右拨=往右 / 下拨=往下 (盘内四向标记)
    auto *dialRow = new QHBoxLayout;
    dialRow->addStretch();
    m_gimbalDial = new GimbalDial;
    dialRow->addWidget(m_gimbalDial, 0, Qt::AlignCenter);
    dialRow->addStretch();
    gimRight->addLayout(dialRow, 1);

    // 拖动 5Hz 节流发送 (v6.14 从10Hz降到5Hz: 减半RFCOMM写频率, 云台舵机9°/档响应慢, 200ms足够跟手; quiet 不打日志)
    m_gimbalTimer = new QTimer(this);
    m_gimbalTimer->setInterval(200);
    connect(m_gimbalTimer, &QTimer::timeout, this, [this]() {
        if (m_gimbalDragging)
            sendGimbal(m_gimbalPanDeg, m_gimbalTiltDeg, true);
    });
    connect(m_gimbalDial, &GimbalDial::dragStarted, this, [this]() {
        m_gimbalDragging = true;
        m_gimbalTimer->start();
    });
    connect(m_gimbalDial, &GimbalDial::valuesChanged, this, [this](int pan, int tilt) {
        m_gimbalPanDeg = pan;
        m_gimbalTiltDeg = tilt;
        m_gimbalStateLabel->setText(QStringLiteral("🎯 水平 %1° / 俯仰 %2°").arg(pan).arg(tilt));
    });
    connect(m_gimbalDial, &GimbalDial::dragEnded, this, [this](int, int) {
        m_gimbalDragging = false;
        m_gimbalTimer->stop();
        sendGimbal(m_gimbalPanDeg, m_gimbalTiltDeg);   // 松手发最终值 (带日志)
    });

    // 底部: 正对前方预设 + 俯仰自稳开关 + 说明
    auto *gimBot = new QHBoxLayout; gimBot->setSpacing(8);
    auto *btnFront = mkB(QStringLiteral("🎯 正对前方 90/120"), C_ORANGE);
    btnFront->setMinimumHeight(34);
    connect(btnFront, &QPushButton::clicked, this, [this]() {
        m_gimbalPanDeg = 90; m_gimbalTiltDeg = 120;
        m_gimbalDial->setValue(90, 120);
        sendGimbal(90, 120);
    });
    gimBot->addWidget(btnFront);
    auto *gimHint = new QLabel(QStringLiteral(
        "点哪云台去哪 (5Hz) · 右拨=往右 下拨=往下 · 松手停住不回中 · 俯仰限位75° · 分辨率9°/档"));
    gimHint->setWordWrap(true);
    gimHint->setStyleSheet(QString("font-size:10px;color:%1;background:transparent;").arg(C_DIM));
    gimBot->addWidget(gimHint, 1);
    gimRight->addLayout(gimBot);
    gimL->addLayout(gimRight, 1);
    m_stack->addWidget(gimW);  // index 4

    // ── 页5: 图传 (v6.12 WiFi MJPEG, 蓝牙与 WiFi 独立并行) ──
    m_cameraWidget = new CameraWidget;
    connect(m_cameraWidget, &CameraWidget::statusChanged, this, [this](const QString &t) {
        m_camStatusLabel->setText(t);
        QString c = C_DIM;
        if (t.contains(QStringLiteral("已连接"))) c = C_GREEN;
        else if (t.contains(QStringLiteral("连接中"))) c = C_ORANGE;
        else if (t.contains(QStringLiteral("失败")) || t.contains(QStringLiteral("断开"))) c = C_RED;
        m_camStatusLabel->setStyleSheet(QString("font-size:11px;font-weight:bold;color:%1;background:transparent;").arg(c));
        m_btnCamConnect->setEnabled(!m_cameraWidget->isStreaming());
        m_btnCamDisconnect->setEnabled(m_cameraWidget->isStreaming());
    });
    connect(m_cameraWidget, &CameraWidget::miniWindowToggled, this, &MainWindow::onMiniCamToggled);
    // v6.14 云台页左侧画面: 流帧信号驱动 (仅云台页可见时更新, 省CPU)
    connect(m_cameraWidget, &CameraWidget::frameUpdated, this, [this](const QPixmap &pm) {
        if (m_gimbalCamLabel && m_stack->currentIndex() == 4)
            m_gimbalCamLabel->setPixmap(pm.scaled(m_gimbalCamLabel->size(),
                                                  Qt::KeepAspectRatio, Qt::SmoothTransformation));
    });
    m_stack->addWidget(m_cameraWidget);  // index 5

    // 画面悬浮小窗: parent=中央窗口, 浮于所有页面之上, 可拖动/双击缩放
    m_miniCam = new MiniCamWindow(cw);
    m_miniCam->move(600, 8);
    m_miniCam->hide();
    m_miniCamTimer = new QTimer(this);
    m_miniCamTimer->setInterval(100);   // 10Hz 拉取最新帧 (不拷贝流数据)
    connect(m_miniCamTimer, &QTimer::timeout, this, [this]() {
        if (m_miniCam && m_miniCam->isVisible())
            m_miniCam->setFrame(m_cameraWidget->currentFrame());
    });

    setPage(0);
    appendLog(QStringLiteral("🚀 调试助手 v6.16 已启动 — 横屏界面"));
}

// v6.14 云台: 发 G:pan:tilt (始终两轴完整; quiet=拖动 5Hz 高频不打日志)
void MainWindow::sendGimbal(int pan, int tilt, bool quiet)
{
    m_bt->sendRawText(QStringLiteral("G:%1:%2").arg(pan).arg(tilt));
    if (!quiet)
        appendLog(QStringLiteral("📤 G:%1:%2 (云台)").arg(pan).arg(tilt), C_BLUE);
}

void MainWindow::setPage(int idx)
{
    m_stack->setCurrentIndex(idx);
    // 切到云台页: 立即拉一帧最新画面 (流帧信号只在本页时更新)
    if (idx == 4 && m_cameraWidget && m_gimbalCamLabel) {
        const QPixmap pm = m_cameraWidget->currentFrame();
        if (pm.isNull())
            m_gimbalCamLabel->setText(QStringLiteral("📷 未连接摄像头\n\n在连接页「WiFi 摄像头」点连接"));
        else
            m_gimbalCamLabel->setPixmap(pm.scaled(m_gimbalCamLabel->size(),
                                                  Qt::KeepAspectRatio, Qt::SmoothTransformation));
    }
    for (int i = 0; i < 6; i++) {
        bool cur = (i == idx);
        m_tabBtns[i]->setStyleSheet(QString(
            "QPushButton{border:none;border-radius:12px;padding:6px;"
            "font-size:12px;font-weight:bold;color:%1;background:%2;}")
            .arg(cur ? QStringLiteral("#fff") : C_DIM,
                 cur ? C_ORANGE : QStringLiteral("transparent")));
    }
}

// v6.12 坡度参数 (Q:i:v) 已随固件删除, 相关能力由站立自稳 + V模式1 覆盖
