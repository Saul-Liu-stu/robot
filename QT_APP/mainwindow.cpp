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

const QString MainWindow::C_BG     = QStringLiteral("#111318");
const QString MainWindow::C_CARD   = QStringLiteral("#181b22");
const QString MainWindow::C_ACCENT = QStringLiteral("#1e2433");
const QString MainWindow::C_BLUE   = QStringLiteral("#5b8def");
const QString MainWindow::C_GREEN  = QStringLiteral("#22c55e");
const QString MainWindow::C_ORANGE = QStringLiteral("#f59e0b");
const QString MainWindow::C_RED    = QStringLiteral("#ef4444");
const QString MainWindow::C_TXT    = QStringLiteral("#d0d4dc");
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
        "QGroupBox{font-size:13px;font-weight:bold;color:%1;"
        "border:1px solid %2;border-radius:8px;margin-top:12px;padding-top:16px;}"
        "QGroupBox::title{subcontrol-origin:margin;left:10px;padding:0 4px;color:%3;}")
        .arg(MainWindow::C_TXT, MainWindow::C_ACCENT, MainWindow::C_BLUE));
}
static QPushButton *mkB(const QString &t, const QString &bg)
{
    auto *b = new QPushButton(t); b->setMinimumHeight(34);
    b->setStyleSheet(QString(
        "QPushButton{border:none;border-radius:8px;padding:8px 16px;"
        "font-size:12px;font-weight:bold;color:#fff;background:%1;}"
        "QPushButton:disabled{background:#2a2a30;color:#555;}").arg(bg));
    return b;
}

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent) { setupUi();
    m_bt = new BluetoothClient(this);
    connect(m_bt, &BluetoothClient::stateChanged, this, &MainWindow::onBtStateChanged);
    connect(m_bt, &BluetoothClient::deviceDiscovered, this, &MainWindow::onDeviceDiscovered);
    connect(m_bt, &BluetoothClient::servoResponseReceived, this, &MainWindow::onServoResponse);
    connect(m_bt, &BluetoothClient::pidMessageReceived, this, &MainWindow::onPidMessage);
    connect(m_bt, &BluetoothClient::motorResponseReceived, this, &MainWindow::onMotorResponse);
    connect(m_bt, &BluetoothClient::rawLineReceived, this, [this](const QString &l) {
        if (l.startsWith(QStringLiteral("READY")))
            appendLog(QStringLiteral("🤝 READY — 固件就绪"), C_GREEN);
        else if (l.startsWith(QStringLiteral("UNK:")))
            appendLog(QStringLiteral("⚠ 固件不认识: %1").arg(l.mid(4)), C_ORANGE);
        else if (l.startsWith(QStringLiteral("STAND...")))
            appendLog(QStringLiteral("🧍 站立中..."), C_BLUE);
        else if (l.startsWith(QStringLiteral("XSH:"))) {
            m_footShift = l.mid(4).trimmed().toInt();
            m_shiftLabel->setText(QStringLiteral("当前: %1 mm").arg(m_footShift));
            appendLog(QStringLiteral("⚖ 前移修正 %1mm").arg(m_footShift), C_GREEN);
        }
        else if (l.compare(QStringLiteral("HIGH"), Qt::CaseInsensitive) == 0)
            appendLog(QStringLiteral("⬆ 高站姿 280mm"), C_BLUE);
        else if (l.compare(QStringLiteral("LOW"), Qt::CaseInsensitive) == 0)
            appendLog(QStringLiteral("⬇ 低站姿 240mm"), C_BLUE);
        else if (l.compare(QStringLiteral("ROLL"), Qt::CaseInsensitive) == 0)
            appendLog(QStringLiteral("🛞 滚动中 (30%)"), C_GREEN);
        else if (l.compare(QStringLiteral("STOP"), Qt::CaseInsensitive) == 0)
            appendLog(QStringLiteral("🛑 四电机已停"), C_RED);
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
    m_statusLabel->setStyleSheet(QString("font-size:15px;font-weight:bold;color:%1;background:transparent;").arg(c));
    m_btnScan->setEnabled(s!=BluetoothClient::Connecting&&s!=BluetoothClient::Connected);
    m_btnConnect->setEnabled(s==BluetoothClient::Idle||s==BluetoothClient::Scanning);
    m_btnDisconnect->setEnabled(on);
    if (!on) { m_servoWidget->reset(); m_pidWidget->reset(); }
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
    m_bt->connectToAddress(c->data(Qt::UserRole).toString());
}
void MainWindow::onDisconnectClicked() { m_bt->disconnect(); m_servoWidget->reset(); m_pidWidget->reset(); m_motorWidget->reset(); }

// ── UI ──────────────────────────────────────────────────────
void MainWindow::setupUi()
{
    setWindowTitle(QStringLiteral("🔧 四足机器人调试助手 v4.4"));
    resize(430, 740);
    QPalette pal; pal.setColor(QPalette::Window, QColor(C_BG)); setPalette(pal); setAutoFillBackground(true);
    auto *cw=new QWidget; cw->setStyleSheet(QString("background:%1;").arg(C_BG));
    auto *sc=new QScrollArea; sc->setWidget(cw); sc->setWidgetResizable(true);
    sc->setFrameShape(QFrame::NoFrame); sc->setStyleSheet(QString("QScrollArea{background:%1;border:none;}").arg(C_BG));
    setCentralWidget(sc);
    auto *rt=new QVBoxLayout(cw); rt->setContentsMargins(8,6,8,4); rt->setSpacing(6);

    // 状态 + 蓝牙
    auto *hdr=new QHBoxLayout;
    m_statusIcon=new QLabel(QStringLiteral("⚫")); m_statusIcon->setStyleSheet(QStringLiteral("font-size:20px;background:transparent;"));
    m_statusLabel=new QLabel(QStringLiteral("未连接")); m_statusLabel->setStyleSheet(QString("font-size:15px;font-weight:bold;color:%1;background:transparent;").arg(C_DIM));
    hdr->addWidget(m_statusIcon); hdr->addWidget(m_statusLabel,1); rt->addLayout(hdr);

    auto *btG=new QGroupBox; sGrp(btG,QStringLiteral("蓝牙 (HC-05 115200bps)"));
    auto *btL=new QVBoxLayout(btG); auto *bR=new QHBoxLayout;
    m_btnScan=mkB(QStringLiteral("扫描"),C_ACCENT); m_btnConnect=mkB(QStringLiteral("连接"),C_BLUE); m_btnDisconnect=mkB(QStringLiteral("断开"),C_ORANGE);
    bR->addWidget(m_btnScan); bR->addWidget(m_btnConnect); bR->addWidget(m_btnDisconnect); btL->addLayout(bR);
    m_devList=new QListWidget; m_devList->setMaximumHeight(90);
    m_devList->setStyleSheet(QString("QListWidget{font-size:12px;border:1px solid %1;border-radius:6px;background:%2;color:%3;padding:2px;}"
        "QListWidget::item{padding:6px 6px;margin:2px 0;border-radius:5px;}" "QListWidget::item:selected{background:%4;color:#fff;}")
        .arg(C_ACCENT,C_CARD,C_TXT,C_BLUE)); btL->addWidget(m_devList); rt->addWidget(btG);
    connect(m_btnScan,&QPushButton::clicked,this,&MainWindow::onScanClicked);
    connect(m_btnConnect,&QPushButton::clicked,this,&MainWindow::onConnectClicked);
    connect(m_btnDisconnect,&QPushButton::clicked,this,&MainWindow::onDisconnectClicked);
    connect(m_devList,&QListWidget::itemDoubleClicked,this,[this](){onConnectClicked();});

    // ── 舵机/PID (实例先建好, 信号连接; 舵机面板加在页面底部) ──
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
    connect(m_motorWidget, &MotorWidget::rollAllRequested, this, [this]() { m_bt->sendRawText(QStringLiteral("R")); appendLog(QStringLiteral("📤 R 滚动"), C_BLUE); });
    connect(m_motorWidget, &MotorWidget::stopAllRequested, this, [this]() { m_bt->sendRawText(QStringLiteral("S")); appendLog(QStringLiteral("📤 S 全停"), C_RED); });
    rt->addWidget(m_motorWidget, 3);

    // ── 步态控制 (2行×3列, 避免一行挤变形) ──
    auto *gaitG = new QGroupBox; sGrp(gaitG, QStringLiteral("步态控制"));
    auto *gaitGrid = new QGridLayout(gaitG); gaitGrid->setSpacing(6);
    auto mkGait = [&](const QString &t, const QString &bg, const QString &cmd, int r, int c) {
        auto *b = mkB(t, bg);
        b->setMinimumHeight(36);
        connect(b, &QPushButton::clicked, this, [this, cmd]() { m_bt->sendRawText(cmd); appendLog(QStringLiteral("📤 %1").arg(cmd), C_BLUE); });
        gaitGrid->addWidget(b, r, c);
    };
    mkGait(QStringLiteral("G 站姿"),   C_GREEN, QStringLiteral("G"), 0, 0);
    mkGait(QStringLiteral("H 高站姿"), QStringLiteral("#8b5cf6"), QStringLiteral("H"), 0, 1);
    mkGait(QStringLiteral("L 低站姿"), QStringLiteral("#ec4899"), QStringLiteral("L"), 0, 2);
    mkGait(QStringLiteral("A 单步"),   C_ORANGE, QStringLiteral("A"), 1, 0);
    mkGait(QStringLiteral("T 行走"),   C_BLUE, QStringLiteral("T"), 1, 1);
    rt->addWidget(gaitG);

    // ── 前倾修正 X:value (滑杆 + 发送) ──
    auto *shiftG = new QGroupBox; sGrp(shiftG, QStringLiteral("前倾修正 (足端前移)"));
    auto *shiftL = new QVBoxLayout(shiftG); shiftL->setSpacing(4);

    auto *shiftInfo = new QHBoxLayout;
    m_shiftLabel = new QLabel(QStringLiteral("当前: %1 mm").arg(m_footShift));
    m_shiftLabel->setStyleSheet(QString("font-size:13px;font-weight:bold;color:%1;background:transparent;").arg(C_BLUE));
    shiftInfo->addWidget(m_shiftLabel); shiftInfo->addStretch();
    shiftL->addLayout(shiftInfo);

    auto *shiftRow = new QHBoxLayout; shiftRow->setSpacing(6);
    auto *shiftSlider = new QSlider(Qt::Horizontal);
    shiftSlider->setRange(-30, 30);
    shiftSlider->setValue(m_footShift);  // 15mm, 与固件默认一致
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
    rt->addWidget(shiftG);

    // ── 舵机校准 (v4.4 重新启用, 放页面底部) ──
    auto *servoG = new QGroupBox; sGrp(servoG, QStringLiteral("舵机校准 (上电后、发 G/T 前使用)"));
    auto *servoL = new QVBoxLayout(servoG);
    servoL->setContentsMargins(2, 2, 2, 2);
    servoL->addWidget(m_servoWidget);
    rt->addWidget(servoG);

    // 日志
    auto *lgG=new QGroupBox; sGrp(lgG,QStringLiteral("运行日志"));
    auto *lgL=new QVBoxLayout(lgG); lgL->setContentsMargins(2,2,2,2);
    m_logView=new QTextEdit; m_logView->setReadOnly(true); m_logView->setMaximumHeight(130);
    m_logView->setStyleSheet(QString("QTextEdit{font-size:11px;font-family:Consolas,monospace;color:%1;background:%2;border:none;}").arg(C_GREEN,C_BG));
    lgL->addWidget(m_logView); rt->addWidget(lgG);

    appendLog(QStringLiteral("🚀 调试助手 v2.1 已启动 — 连接后自动识别固件"));
}
