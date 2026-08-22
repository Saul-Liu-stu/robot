#include "camerawidget.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QScrollArea>
#include <QFrame>
#include <QImage>
#include <QUrl>
#include <QDateTime>
#include <QStandardPaths>
#include <QDir>
#include <QDebug>
#include <QCoreApplication>
#include <QJniObject>
#include <QJniEnvironment>
#if defined(Q_OS_ANDROID)
#include <QtCore/qnativeinterface.h>
#endif

// 石墨极简风配色 (与 mainwindow 一致)
static const QString C_BG    = QStringLiteral("#141518");
static const QString C_CARD  = QStringLiteral("#1e2024");
static const QString C_TXT   = QStringLiteral("#e5e7eb");
static const QString C_DIM   = QStringLiteral("#6b7280");
static const QString C_BLUE  = QStringLiteral("#3b82f6");
static const QString C_GREEN = QStringLiteral("#22c55e");
static const QString C_RED   = QStringLiteral("#ef4444");
static const QString C_ORANGE= QStringLiteral("#f59e0b");
static const QString C_ACC   = QStringLiteral("#2a2d33");

static void sGrp(QGroupBox *g, const QString &t)
{
    g->setTitle(t);
    g->setStyleSheet(QString(
        "QGroupBox{font-size:11px;font-weight:bold;color:%1;"
        "border:1px solid %2;border-radius:10px;margin-top:10px;padding-top:14px;}"
        "QGroupBox::title{subcontrol-origin:margin;left:10px;padding:0 4px;color:%3;}")
        .arg(C_TXT, C_ACC, C_DIM));
}
static QString mkStyle(const QString &bg)
{
    return QString(
        "QPushButton{border:none;border-radius:10px;padding:6px 10px;"
        "font-size:12px;font-weight:bold;color:#fff;background:%1;}"
        "QPushButton:disabled{background:#2a2c31;color:#5a5f68;}").arg(bg);
}
static QPushButton *mkB(const QString &t, const QString &bg)
{
    auto *b = new QPushButton(t); b->setMinimumHeight(34);
    b->setStyleSheet(mkStyle(bg));
    return b;
}

// ══════════════ 画面悬浮小窗 (拖动/双击缩放/关闭) ══════════════
MiniCamWindow::MiniCamWindow(QWidget *parent) : QWidget(parent)
{
    setAttribute(Qt::WA_StyledBackground);
    setStyleSheet(QString("MiniCamWindow{background:#000;border:2px solid %1;border-radius:8px;}")
        .arg(C_ORANGE));
    auto *l = new QVBoxLayout(this);
    l->setContentsMargins(4, 4, 4, 4); l->setSpacing(2);
    m_view = new QLabel(QStringLiteral("📷 无画面"));
    m_view->setAlignment(Qt::AlignCenter);
    m_view->setStyleSheet(QString("QLabel{background:#000;color:%1;font-size:10px;border:none;}").arg(C_DIM));
    l->addWidget(m_view, 1);
    m_close = new QPushButton(QStringLiteral("×"), this);
    m_close->setFixedSize(18, 18);
    m_close->setStyleSheet(QStringLiteral(
        "QPushButton{background:#ef4444;color:#fff;border:none;border-radius:9px;"
        "font-size:11px;font-weight:bold;}"));
    connect(m_close, &QPushButton::clicked, this, &QWidget::hide);
    applySize();
}

void MiniCamWindow::applySize()
{
    resize(m_big ? QSize(340, 260) : QSize(180, 140));
}

void MiniCamWindow::setFrame(const QPixmap &pm)
{
    if (pm.isNull()) return;
    m_view->setPixmap(pm.scaled(m_view->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
}

void MiniCamWindow::mousePressEvent(QMouseEvent *e)
{
    if (e->button() == Qt::LeftButton) {
        m_dragging = true;
        m_dragOffset = e->globalPosition().toPoint() - frameGeometry().topLeft();
        raise();
    }
}

void MiniCamWindow::mouseMoveEvent(QMouseEvent *e)
{
    if (!m_dragging) return;
    QPoint np = e->globalPosition().toPoint() - m_dragOffset;
    if (QWidget *p = parentWidget()) {   // 限制在 APP 窗口内
        np.setX(qBound(0, np.x(), p->width() - width()));
        np.setY(qBound(0, np.y(), p->height() - height()));
    }
    move(np);
}

void MiniCamWindow::mouseReleaseEvent(QMouseEvent *) { m_dragging = false; }

void MiniCamWindow::mouseDoubleClickEvent(QMouseEvent *)
{
    m_big = !m_big;
    applySize();
    if (QWidget *p = parentWidget())   // 缩放后保持在窗口内
        move(qBound(0, x(), p->width() - width()), qBound(0, y(), p->height() - height()));
}

void MiniCamWindow::resizeEvent(QResizeEvent *)
{
    if (m_close) m_close->move(width() - 22, 4);
    // 双击缩放后, 下一帧 setFrame (100ms 内) 自动按新尺寸等比缩放
}

// ══════════════ CameraWidget ══════════════
CameraWidget::CameraWidget(QWidget *parent)
    : QWidget(parent), m_mgr(new QNetworkAccessManager(this))
{
    m_detector = new ColorDetector(this);
    m_motionDetector = new MotionDetector(this);
    setupUi();
}

CameraWidget::~CameraWidget() { stopStream(); }

void CameraWidget::setupUi()
{
    auto *l = new QHBoxLayout(this);
    l->setContentsMargins(0, 0, 0, 0); l->setSpacing(8);

    // ── 左: 视频区 (等比缩放显示) ──
    auto *vidG = new QGroupBox; sGrp(vidG, QStringLiteral("视频流 (MJPEG)"));
    auto *vidL = new QVBoxLayout(vidG); vidL->setContentsMargins(4, 4, 4, 4); vidL->setSpacing(4);
    m_videoLabel = new QLabel(QStringLiteral("未连接 — 去「连接」页连接 WiFi 摄像头"));
    m_videoLabel->setAlignment(Qt::AlignCenter);
    m_videoLabel->setMinimumSize(320, 240);
    m_videoLabel->setStyleSheet(QString(
        "QLabel{background:#000;border-radius:8px;color:%1;font-size:11px;}").arg(C_DIM));
    vidL->addWidget(m_videoLabel, 1);
    l->addWidget(vidG, 3);

    // ── 右: 控制区 (可上下滑动, 控件恢复舒适尺寸) ──
    auto *ctlW = new QWidget;
    auto *ctlSc = new QScrollArea;
    ctlSc->setWidget(ctlW); ctlSc->setWidgetResizable(true);
    ctlSc->setFrameShape(QFrame::NoFrame);
    ctlSc->setStyleSheet(QString("QScrollArea{background:%1;border:none;}").arg(C_BG));
    auto *ctl = new QVBoxLayout(ctlW); ctl->setContentsMargins(0, 0, 0, 0); ctl->setSpacing(8);

    // 画面参数
    auto *qualG = new QGroupBox; sGrp(qualG, QStringLiteral("画面参数 (control API)"));
    auto *qualL = new QVBoxLayout(qualG); qualL->setSpacing(8);

    auto *resRow = new QHBoxLayout; resRow->setSpacing(6);
    m_btnQvga = mkB(QStringLiteral("QVGA 流畅"), C_ACC);
    m_btnVga  = mkB(QStringLiteral("VGA 清晰"), C_BLUE);   // 默认 VGA 高亮
    connect(m_btnQvga, &QPushButton::clicked, this, [this]() {
        m_framesize = 4;
        sendControl(QStringLiteral("framesize"), 4);
        m_btnQvga->setStyleSheet(mkStyle(C_BLUE));
        m_btnVga->setStyleSheet(mkStyle(C_ACC));
    });
    connect(m_btnVga, &QPushButton::clicked, this, [this]() {
        m_framesize = 6;
        sendControl(QStringLiteral("framesize"), 6);
        m_btnVga->setStyleSheet(mkStyle(C_BLUE));
        m_btnQvga->setStyleSheet(mkStyle(C_ACC));
    });
    resRow->addWidget(m_btnQvga); resRow->addWidget(m_btnVga);
    qualL->addLayout(resRow);

    auto *qualRow = new QHBoxLayout; qualRow->setSpacing(6);
    m_qualityVal = new QLabel(QStringLiteral("画质 15"));
    m_qualityVal->setStyleSheet(QString("font-size:12px;font-weight:bold;color:%1;background:transparent;").arg(C_BLUE));
    m_qualitySlider = new QSlider(Qt::Horizontal);
    m_qualitySlider->setRange(4, 63);
    m_qualitySlider->setValue(15);
    m_qualitySlider->setStyleSheet(QString(
        "QSlider::groove:horizontal{height:6px;background:%1;border-radius:3px;}"
        "QSlider::handle:horizontal{width:22px;height:22px;margin:-8px 0;"
        "background:%2;border-radius:11px;}"
        "QSlider::sub-page:horizontal{background:%2;border-radius:3px;}")
        .arg(C_ACC, C_BLUE));
    connect(m_qualitySlider, &QSlider::valueChanged, this, [this](int v) {
        m_qualityVal->setText(QStringLiteral("画质 %1").arg(v));
    });
    connect(m_qualitySlider, &QSlider::sliderReleased, this, [this]() {
        sendControl(QStringLiteral("quality"), m_qualitySlider->value());
    });
    qualRow->addWidget(m_qualityVal);
    qualRow->addWidget(m_qualitySlider, 1);
    qualL->addLayout(qualRow);

    // 亮度/对比度 (暗光环境拉亮度提升人脸检测率)
    auto *brightRow = new QHBoxLayout; brightRow->setSpacing(6);
    auto *brightVal = new QLabel(QStringLiteral("亮度 0"));
    brightVal->setStyleSheet(QString("font-size:12px;font-weight:bold;color:%1;background:transparent;").arg(C_BLUE));
    auto *brightSlider = new QSlider(Qt::Horizontal);
    brightSlider->setRange(-2, 2);
    brightSlider->setValue(0);
    brightSlider->setStyleSheet(QString(
        "QSlider::groove:horizontal{height:6px;background:%1;border-radius:3px;}"
        "QSlider::handle:horizontal{width:22px;height:22px;margin:-8px 0;"
        "background:%2;border-radius:11px;}"
        "QSlider::sub-page:horizontal{background:%2;border-radius:3px;}")
        .arg(C_ACC, C_BLUE));
    connect(brightSlider, &QSlider::valueChanged, this, [brightVal](int v) {
        brightVal->setText(QStringLiteral("亮度 %1").arg(v));
    });
    connect(brightSlider, &QSlider::sliderReleased, this, [this, brightSlider]() {
        sendControl(QStringLiteral("brightness"), brightSlider->value());
    });
    brightRow->addWidget(brightVal);
    brightRow->addWidget(brightSlider, 1);
    qualL->addLayout(brightRow);

    auto *contrastRow = new QHBoxLayout; contrastRow->setSpacing(6);
    auto *contrastVal = new QLabel(QStringLiteral("对比度 0"));
    contrastVal->setStyleSheet(QString("font-size:12px;font-weight:bold;color:%1;background:transparent;").arg(C_BLUE));
    auto *contrastSlider = new QSlider(Qt::Horizontal);
    contrastSlider->setRange(-2, 2);
    contrastSlider->setValue(0);
    contrastSlider->setStyleSheet(brightSlider->styleSheet());
    connect(contrastSlider, &QSlider::valueChanged, this, [contrastVal](int v) {
        contrastVal->setText(QStringLiteral("对比度 %1").arg(v));
    });
    connect(contrastSlider, &QSlider::sliderReleased, this, [this, contrastSlider]() {
        sendControl(QStringLiteral("contrast"), contrastSlider->value());
    });
    contrastRow->addWidget(contrastVal);
    contrastRow->addWidget(contrastSlider, 1);
    qualL->addLayout(contrastRow);

    auto *flipRow = new QHBoxLayout; flipRow->setSpacing(6);
    m_btnHmirror = mkB(QStringLiteral("水平镜像 关"), C_ACC);
    m_btnVflip   = mkB(QStringLiteral("垂直翻转 关"), C_ACC);
    connect(m_btnHmirror, &QPushButton::clicked, this, [this]() {
        m_hmirror = !m_hmirror;
        sendControl(QStringLiteral("hmirror"), m_hmirror);
        m_btnHmirror->setText(m_hmirror ? QStringLiteral("水平镜像 开") : QStringLiteral("水平镜像 关"));
        m_btnHmirror->setStyleSheet(mkStyle(m_hmirror ? C_BLUE : C_ACC));
    });
    connect(m_btnVflip, &QPushButton::clicked, this, [this]() {
        m_vflip = !m_vflip;
        sendControl(QStringLiteral("vflip"), m_vflip);
        m_btnVflip->setText(m_vflip ? QStringLiteral("垂直翻转 开") : QStringLiteral("垂直翻转 关"));
        m_btnVflip->setStyleSheet(mkStyle(m_vflip ? C_BLUE : C_ACC));
    });
    flipRow->addWidget(m_btnHmirror); flipRow->addWidget(m_btnVflip);
    qualL->addLayout(flipRow);
    ctl->addWidget(qualG);

    // 拍照
    m_btnCapture = mkB(QStringLiteral("📷 拍照存本地"), C_ORANGE);
    m_btnCapture->setMinimumHeight(38);
    connect(m_btnCapture, &QPushButton::clicked, this, &CameraWidget::onCaptureClicked);
    ctl->addWidget(m_btnCapture);

    // 定时拍照 (搜救记录: 每20秒自动拍一张)
    auto *autoCapRow = new QHBoxLayout; autoCapRow->setSpacing(6);
    m_btnAutoCap = mkB(QStringLiteral("⏲ 定时拍照 关"), C_ACC);
    connect(m_btnAutoCap, &QPushButton::clicked, this, [this]() {
        m_autoCapOn = !m_autoCapOn;
        m_btnAutoCap->setText(m_autoCapOn ? QStringLiteral("⏲ 定时拍照 开") : QStringLiteral("⏲ 定时拍照 关"));
        m_btnAutoCap->setStyleSheet(mkStyle(m_autoCapOn ? C_ORANGE : C_ACC));
        if (m_autoCapOn) {
            m_autoCapCount = 0;
            m_autoCapTimer->start();
        } else {
            m_autoCapTimer->stop();
        }
    });
    autoCapRow->addWidget(m_btnAutoCap, 1);
    m_autoCapLabel = new QLabel(QStringLiteral("每20秒自动拍照"));
    m_autoCapLabel->setAlignment(Qt::AlignCenter);
    m_autoCapLabel->setStyleSheet(QString("font-size:11px;font-weight:bold;color:%1;background:transparent;").arg(C_DIM));
    autoCapRow->addWidget(m_autoCapLabel, 1);
    ctl->addLayout(autoCapRow);

    m_autoCapTimer = new QTimer(this);
    m_autoCapTimer->setInterval(20000);   // 20秒一张
    connect(m_autoCapTimer, &QTimer::timeout, this, [this]() {
        if (m_ip.isEmpty() || !isStreaming()) return;   // 未连流不拍
        onCaptureClicked();
        m_autoCapCount++;
        m_autoCapLabel->setText(QStringLiteral("已拍 %1 张").arg(m_autoCapCount));
        m_autoCapLabel->setStyleSheet(QString("font-size:11px;font-weight:bold;color:%1;background:transparent;").arg(C_GREEN));
    });

    // 悬浮小窗 (遥控行走时悬浮看画面)
    m_btnMini = mkB(QStringLiteral("🪟 悬浮小窗 关"), C_BLUE);
    m_btnMini->setMinimumHeight(38);
    connect(m_btnMini, &QPushButton::clicked, this, [this]() {
        m_miniOn = !m_miniOn;
        m_btnMini->setText(m_miniOn ? QStringLiteral("🪟 悬浮小窗 开") : QStringLiteral("🪟 悬浮小窗 关"));
        m_btnMini->setStyleSheet(mkStyle(m_miniOn ? C_ORANGE : C_BLUE));
        emit miniWindowToggled(m_miniOn);
    });
    ctl->addWidget(m_btnMini);

    // 颜色识别 (v6.12, 纯 Qt HSV 阈值 + 连通域)
    auto *detG = new QGroupBox; sGrp(detG, QStringLiteral("🎨 颜色识别 (追踪色标用)"));
    auto *detL = new QVBoxLayout(detG); detL->setSpacing(8);

    m_btnDetect = mkB(QStringLiteral("🎨 识别 关"), C_ACC);
    m_btnDetect->setMinimumHeight(38);
    connect(m_btnDetect, &QPushButton::clicked, this, [this]() {
        m_detectOn = !m_detectOn;
        m_btnDetect->setText(m_detectOn ? QStringLiteral("🎨 识别 开") : QStringLiteral("🎨 识别 关"));
        m_btnDetect->setStyleSheet(mkStyle(m_detectOn ? C_GREEN : C_ACC));
        if (!m_detectOn) {
            m_detectResultLabel->setText(QStringLiteral("🎯 识别未开启"));
            m_detectResultLabel->setStyleSheet(QString("font-size:11px;font-weight:bold;color:%1;background:transparent;").arg(C_DIM));
        }
    });
    detL->addWidget(m_btnDetect);

    // 6 色按钮 (红绿蓝黄橙紫)
    struct { ColorName c; const char *name; const char *bg; } defs[6] = {
        { ColorName::RED,    "红", "#ef4444" },
        { ColorName::GREEN,  "绿", "#22c55e" },
        { ColorName::BLUE,   "蓝", "#3b82f6" },
        { ColorName::YELLOW, "黄", "#eab308" },
        { ColorName::ORANGE, "橙", "#f59e0b" },
        { ColorName::PURPLE, "紫", "#a855f7" },
    };
    auto mkColorStyle = [](const QString &bg, bool sel) {
        return QString(
            "QPushButton{border:2px solid %1;border-radius:10px;padding:4px 0;"
            "font-size:12px;font-weight:bold;color:#fff;background:%2;}")
            .arg(sel ? QStringLiteral("#ffffff") : QStringLiteral("transparent"), bg);
    };
    auto *colorRow = new QHBoxLayout; colorRow->setSpacing(4);
    for (int i = 0; i < 6; i++) {
        m_colorBtns[i] = new QPushButton(QString::fromUtf8(defs[i].name));
        m_colorBtns[i]->setMinimumHeight(30);
        m_colorBtns[i]->setStyleSheet(mkColorStyle(QString::fromUtf8(defs[i].bg), i == 0));
        const ColorName cn = defs[i].c;
        // 按值捕获 defs (C++17 数组拷贝): 修复按引用捕获局部数组导致的悬垂引用闪退
        connect(m_colorBtns[i], &QPushButton::clicked, this, [this, i, cn, defs, mkColorStyle]() {
            m_detector->setTargetColor(cn);
            for (int k = 0; k < 6; k++) {
                const QString bg = QString::fromUtf8(defs[k].bg);
                m_colorBtns[k]->setStyleSheet(mkColorStyle(bg, k == i));
            }
        });
        colorRow->addWidget(m_colorBtns[i], 1);
    }
    detL->addLayout(colorRow);

    // minArea 滑杆
    auto *areaRow = new QHBoxLayout; areaRow->setSpacing(6);
    m_minAreaVal = new QLabel(QStringLiteral("过滤面积 300"));
    m_minAreaVal->setStyleSheet(QString("font-size:11px;font-weight:bold;color:%1;background:transparent;").arg(C_BLUE));
    m_minAreaSlider = new QSlider(Qt::Horizontal);
    m_minAreaSlider->setRange(100, 5000);
    m_minAreaSlider->setValue(300);   // 默认放宽: 小色标也能识别
    m_minAreaSlider->setStyleSheet(QString(
        "QSlider::groove:horizontal{height:6px;background:%1;border-radius:3px;}"
        "QSlider::handle:horizontal{width:22px;height:22px;margin:-8px 0;"
        "background:%2;border-radius:11px;}"
        "QSlider::sub-page:horizontal{background:%2;border-radius:3px;}")
        .arg(C_ACC, C_GREEN));
    connect(m_minAreaSlider, &QSlider::valueChanged, this, [this](int v) {
        m_minAreaVal->setText(QStringLiteral("过滤面积 %1").arg(v));
    });
    connect(m_minAreaSlider, &QSlider::sliderReleased, this, [this]() {
        m_detector->setMinArea(m_minAreaSlider->value());
    });
    areaRow->addWidget(m_minAreaVal);
    areaRow->addWidget(m_minAreaSlider, 1);
    detL->addLayout(areaRow);

    m_detectResultLabel = new QLabel(QStringLiteral("🎯 识别未开启"));
    m_detectResultLabel->setAlignment(Qt::AlignCenter);
    m_detectResultLabel->setStyleSheet(QString("font-size:11px;font-weight:bold;color:%1;background:transparent;").arg(C_DIM));
    detL->addWidget(m_detectResultLabel);

    // 掩膜预览: 黑白图, 白=命中像素 (调 HSV 阈值调试用)
    auto *btnMask = mkB(QStringLiteral("👁 掩膜预览 关"), C_ACC);
    btnMask->setMinimumHeight(30);
    bool maskOn = false;
    connect(btnMask, &QPushButton::clicked, this, [this, btnMask, maskOn]() mutable {
        maskOn = !maskOn;
        m_detector->setMaskPreview(maskOn);
        btnMask->setText(maskOn ? QStringLiteral("👁 掩膜预览 开") : QStringLiteral("👁 掩膜预览 关"));
        btnMask->setStyleSheet(mkStyle(maskOn ? C_BLUE : C_ACC));
    });
    detL->addWidget(btnMask);
    ctl->addWidget(detG);

    // 运动检测 (v6.12 帧差法, 搜救场景: 发现移动目标)
    auto *motG = new QGroupBox; sGrp(motG, QStringLiteral("📹 运动检测 (搜救: 发现移动目标)"));
    auto *motL = new QVBoxLayout(motG); motL->setSpacing(8);

    m_btnMotion = mkB(QStringLiteral("📹 运动检测 关"), C_ACC);
    m_btnMotion->setMinimumHeight(38);
    connect(m_btnMotion, &QPushButton::clicked, this, [this]() {
        m_motionOn = !m_motionOn;
        m_btnMotion->setText(m_motionOn ? QStringLiteral("📹 运动检测 开") : QStringLiteral("📹 运动检测 关"));
        m_btnMotion->setStyleSheet(mkStyle(m_motionOn ? C_GREEN : C_ACC));
        if (!m_motionOn) {
            m_motionDetector->reset();
            m_lastMotionText.clear();
            m_motionResultLabel->setText(QStringLiteral("📹 检测未开启"));
            m_motionResultLabel->setStyleSheet(QString("font-size:11px;font-weight:bold;color:%1;background:transparent;").arg(C_DIM));
        } else {
            m_lastMotionText.clear();
            m_motionResultLabel->setText(QStringLiteral("📹 建立参考帧..."));
            m_motionResultLabel->setStyleSheet(QString("font-size:11px;font-weight:bold;color:%1;background:transparent;").arg(C_ORANGE));
        }
    });
    motL->addWidget(m_btnMotion);

    // 灵敏度: 帧差阈值 5~100 (数值越小越灵敏/误报多, 越大越抗误报)
    auto *sensRow = new QHBoxLayout; sensRow->setSpacing(6);
    m_sensVal = new QLabel(QStringLiteral("灵敏度 20"));
    m_sensVal->setStyleSheet(QString("font-size:11px;font-weight:bold;color:%1;background:transparent;").arg(C_BLUE));
    m_sensSlider = new QSlider(Qt::Horizontal);
    m_sensSlider->setRange(5, 100);
    m_sensSlider->setValue(20);
    m_sensSlider->setStyleSheet(QString(
        "QSlider::groove:horizontal{height:6px;background:%1;border-radius:3px;}"
        "QSlider::handle:horizontal{width:22px;height:22px;margin:-8px 0;"
        "background:%2;border-radius:11px;}"
        "QSlider::sub-page:horizontal{background:%2;border-radius:3px;}")
        .arg(C_ACC, C_GREEN));
    connect(m_sensSlider, &QSlider::valueChanged, this, [this](int v) {
        m_sensVal->setText(QStringLiteral("灵敏度 %1").arg(v));
        m_motionDetector->setSensitivity(v);
    });
    sensRow->addWidget(m_sensVal);
    sensRow->addWidget(m_sensSlider, 1);
    motL->addLayout(sensRow);

    m_motionResultLabel = new QLabel(QStringLiteral("📹 检测未开启"));
    m_motionResultLabel->setAlignment(Qt::AlignCenter);
    m_motionResultLabel->setStyleSheet(QString("font-size:11px;font-weight:bold;color:%1;background:transparent;").arg(C_DIM));
    motL->addWidget(m_motionResultLabel);
    ctl->addWidget(motG);

    ctl->addStretch();
    l->addWidget(ctlSc, 2);
}

void CameraWidget::startStream(const QString &ip)
{
    if (m_reply) return;   // 已连接
    m_ip = ip.trimmed();
    if (m_ip.isEmpty()) { emit statusChanged(QStringLiteral("状态: IP 为空")); return; }
    const QString url = QStringLiteral("http://%1:81/stream").arg(m_ip);

    QNetworkRequest req((QUrl(url)));
    req.setRawHeader("User-Agent", "RobotBleApp/6.12");
    m_reply = m_mgr->get(req);
    m_buffer.clear();
    m_frame = QPixmap();
    m_motionDetector->reset();   // 流重连: 运动检测参考帧失效
    emit statusChanged(QStringLiteral("状态: 连接中 %1 ...").arg(url));
    connect(m_reply, &QNetworkReply::readyRead, this, &CameraWidget::onStreamData);
    connect(m_reply, &QNetworkReply::errorOccurred, this, [this](QNetworkReply::NetworkError) {
        emit statusChanged(QStringLiteral("状态: 连接失败 (检查热点/IP/端口)"));
        m_reply->deleteLater();
        m_reply = nullptr;
    });
    connect(m_reply, &QNetworkReply::finished, this, &CameraWidget::onStreamFinished);
}

void CameraWidget::stopStream()
{
    if (m_reply) {
        m_reply->abort();
        m_reply->deleteLater();
        m_reply = nullptr;
    }
    m_buffer.clear();
}

void CameraWidget::onStreamData()
{
    if (!m_reply) return;
    m_buffer += m_reply->readAll();
    parseMjpegFrame();
}

void CameraWidget::onStreamFinished()
{
    // 流结束 (断线/热点丢失)
    if (m_reply) { m_reply->deleteLater(); m_reply = nullptr; }
    m_buffer.clear();
    emit statusChanged(QStringLiteral("状态: 流已断开 — 点「连接」重连"));
}

// MJPEG: 每帧 JPEG 以 0xFFD8(SOI) 开头、0xFFD9(EOI) 结尾
// 缓冲区上限 1MB, 超出清空等下一帧
void CameraWidget::parseMjpegFrame()
{
    if (m_buffer.size() > 1024 * 1024) {
        m_buffer.clear();
        return;
    }
    while (m_buffer.size() > 0) {
        const int jpegStart = m_buffer.indexOf("\xff\xd8");
        if (jpegStart < 0) { m_buffer.clear(); return; }
        const int jpegEnd = m_buffer.indexOf("\xff\xd9", jpegStart + 2);
        if (jpegEnd < 0) {
            if (jpegStart > 0) m_buffer = m_buffer.mid(jpegStart);
            return;
        }
        const QByteArray jpegData = m_buffer.mid(jpegStart, jpegEnd + 2 - jpegStart);
        QImage img;
        if (img.loadFromData(jpegData, "JPG")) {
            if (m_detectOn) {   // v6.12 颜色识别 (结果文本变化才刷新标签)
                ColorDetectionResult r;
                img = m_detector->detect(img, r);
                const QString txt = r.detected
                    ? QStringLiteral("🎯 目标 (%1,%2) 面积 %3")
                          .arg(r.center.x()).arg(r.center.y()).arg((int)r.area)
                    : QStringLiteral("🎯 未检测到");
                if (txt != m_lastResultText) {
                    m_lastResultText = txt;
                    m_detectResultLabel->setText(txt);
                    m_detectResultLabel->setStyleSheet(QString(
                        "font-size:11px;font-weight:bold;color:%1;background:transparent;")
                        .arg(r.detected ? C_GREEN : C_DIM));
                }
            }
            if (m_motionOn) {   // v6.12 运动检测 (帧差法, 搜救场景)
                MotionResult mr;
                img = m_motionDetector->detect(img, mr);
                const QString txt = mr.detected
                    ? QStringLiteral("🚨 检测到运动 (%1,%2) 面积 %3")
                          .arg(mr.center.x()).arg(mr.center.y()).arg((int)mr.area)
                    : QStringLiteral("📹 无运动");
                if (txt != m_lastMotionText) {
                    m_lastMotionText = txt;
                    m_motionResultLabel->setText(txt);
                    m_motionResultLabel->setStyleSheet(QString(
                        "font-size:11px;font-weight:bold;color:%1;background:transparent;")
                        .arg(mr.detected ? C_RED : C_DIM));
                }
            }
            m_frame = QPixmap::fromImage(img);
            rescalePixmap();   // 等比缩放, 不拉伸畸变
            if (!m_reply || m_reply->property("reported").isNull()) {
                emit statusChanged(QStringLiteral("状态: 已连接 — 视频流中"));
                if (m_reply) {
                    m_reply->setProperty("reported", true);
                    // 首次连接自动初始化: QVGA 流畅档 + 画质4 (固件钳到10), 首屏不卡
                    sendControl(QStringLiteral("framesize"), 4);
                    sendControl(QStringLiteral("quality"), 4);
                    m_framesize = 4;
                    m_btnQvga->setStyleSheet(mkStyle(C_BLUE));
                    m_btnVga->setStyleSheet(mkStyle(C_ACC));
                    m_qualitySlider->setValue(4);   // 触发 valueChanged 同步显示
                }
            }
        }
        m_buffer = m_buffer.mid(jpegEnd + 2);
    }
}

// 等比缩放: 画面比例不变, 空白留黑边 (修复 setScaledContents 拉伸畸变)
void CameraWidget::rescalePixmap()
{
    if (m_frame.isNull() || m_videoLabel->width() <= 0) return;
    m_videoLabel->setPixmap(m_frame.scaled(m_videoLabel->size(),
        Qt::KeepAspectRatio, Qt::SmoothTransformation));
}

void CameraWidget::resizeEvent(QResizeEvent *e)
{
    QWidget::resizeEvent(e);
    rescalePixmap();
}

// 存系统相册 Pictures/RobotBleApp (MediaStore, 相册 APP 直接可见)
// 失败时回退 app 私有目录; 返回实际保存位置说明
static QString saveJpegToGallery(const QByteArray &jpeg, const QString &name)
{
#if defined(Q_OS_ANDROID)
    QJniObject ctx = QNativeInterface::QAndroidApplication::context();
    QJniObject resolver = ctx.callObjectMethod(
        "getContentResolver", "()Landroid/content/ContentResolver;");
    QJniEnvironment env;
    jclass mediaClass = env->FindClass("android/provider/MediaStore$Images$Media");
    QJniObject uriObj = QJniObject::getStaticObjectField(
        mediaClass, "EXTERNAL_CONTENT_URI", "Landroid/net/Uri;");
    QJniObject values("android/content/ContentValues");
    values.callMethod<void>("put", "(Ljava/lang/String;Ljava/lang/String;)V",
        QJniObject::fromString(QStringLiteral("_display_name")).object(),
        QJniObject::fromString(name).object());
    values.callMethod<void>("put", "(Ljava/lang/String;Ljava/lang/String;)V",
        QJniObject::fromString(QStringLiteral("mime_type")).object(),
        QJniObject::fromString(QStringLiteral("image/jpeg")).object());
    if (QNativeInterface::QAndroidApplication::sdkVersion() >= 29) {
        values.callMethod<void>("put", "(Ljava/lang/String;Ljava/lang/String;)V",
            QJniObject::fromString(QStringLiteral("relative_path")).object(),
            QJniObject::fromString(QStringLiteral("Pictures/RobotBleApp")).object());
    }
    QJniObject outUri = resolver.callObjectMethod(
        "insert", "(Landroid/net/Uri;Landroid/content/ContentValues;)Landroid/net/Uri;",
        uriObj.object(), values.object());
    if (outUri.isValid()) {
        QJniObject os = resolver.callObjectMethod(
            "openOutputStream", "(Landroid/net/Uri;)Ljava/io/OutputStream;", outUri.object());
        if (os.isValid()) {
            jbyteArray arr = env->NewByteArray(jpeg.size());
            env->SetByteArrayRegion(arr, 0, jpeg.size(),
                                    reinterpret_cast<const jbyte *>(jpeg.constData()));
            os.callMethod<void>("write", "([B)V", arr);
            os.callMethod<void>("close", "()V");
            return QStringLiteral("相册 Pictures/RobotBleApp/%1").arg(name);
        }
    }
#endif
    // 回退: app 私有目录
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dir);
    const QString path = dir + QLatin1Char('/') + name;
    QImage img;
    if (img.loadFromData(jpeg, "JPG") && img.save(path, "JPG", 95))
        return path;
    return QString();
}

void CameraWidget::onCaptureClicked()
{
    if (m_ip.isEmpty()) { emit statusChanged(QStringLiteral("状态: 未连接摄像头")); return; }
    QNetworkRequest req(QUrl(QStringLiteral("http://%1/capture").arg(m_ip)));
    QNetworkReply *reply = m_mgr->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        if (reply->error() == QNetworkReply::NoError) {
            const QByteArray jpegData = reply->readAll();
            QImage img;
            if (img.loadFromData(jpegData, "JPG")) {
                const QString name = QStringLiteral("capture_%1.jpg")
                    .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss")));
                const QString where = saveJpegToGallery(jpegData, name);
                if (!where.isEmpty())
                    emit statusChanged(QStringLiteral("状态: 已存 %1").arg(where));
                else
                    emit statusChanged(QStringLiteral("状态: 保存失败"));
            } else {
                emit statusChanged(QStringLiteral("状态: 拍照解码失败"));
            }
        } else {
            emit statusChanged(QStringLiteral("状态: 拍照失败 (模块未连接?)"));
        }
        reply->deleteLater();
        // ESP32 单核处理 /capture 时 stream 会吐出半帧: 清缓冲丢残帧, 防畸变帧
        m_buffer.clear();
    });
}

void CameraWidget::sendControl(const QString &var, int val)
{
    if (m_ip.isEmpty()) return;
    const QString url = QStringLiteral("http://%1/control?var=%2&val=%3").arg(m_ip, var).arg(val);
    QNetworkReply *reply = m_mgr->get(QNetworkRequest(QUrl(url)));
    connect(reply, &QNetworkReply::finished, reply, &QObject::deleteLater);
}
