#include "servowidget.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QFrame>
#include <QFileDialog>
#include <QMessageBox>
#include <QFile>
#include <QTextStream>

static const QString C_BG    = QStringLiteral("#111318");
static const QString C_CARD  = QStringLiteral("#181b22");
static const QString C_TXT   = QStringLiteral("#d0d4dc");
static const QString C_DIM   = QStringLiteral("#6b7280");
static const QString C_BLUE  = QStringLiteral("#5b8def");
static const QString C_GREEN = QStringLiteral("#22c55e");
static const QString C_RED   = QStringLiteral("#ef4444");
static const QString C_ORANGE= QStringLiteral("#f59e0b");
static const QString C_ACC   = QStringLiteral("#1e2433");

static QPushButton *mkB(const QString &t, const QString &bg, int h = 30, int fw = 0)
{
    auto *b = new QPushButton(t);
    b->setMinimumHeight(h);
    if (fw) b->setFixedWidth(fw);
    b->setStyleSheet(QString(
        "QPushButton{border:none;border-radius:6px;padding:4px 8px;"
        "font-size:12px;font-weight:bold;color:#fff;background:%1;}"
        "QPushButton:disabled{background:#2a2a30;color:#555;}").arg(bg));
    return b;
}

ServoWidget::ServoWidget(QWidget *parent) : QWidget(parent)
{
    for (int i = 0; i < 12; i++) m_savedAngles[i] = -1;
    setupUi();
}

void ServoWidget::setupUi()
{
    setStyleSheet(QString("background:%1;border-radius:10px;").arg(C_CARD));
    auto *l = new QVBoxLayout(this);
    l->setContentsMargins(12, 8, 12, 8); l->setSpacing(5);

    auto *title = new QLabel(QStringLiteral("舵机校准 — 多舵机 + 两步确认"));
    title->setStyleSheet(QString("font-size:13px;font-weight:bold;color:%1;background:transparent;").arg(C_BLUE));
    l->addWidget(title);
    auto *sep = new QFrame; sep->setFrameShape(QFrame::HLine);
    sep->setStyleSheet(QStringLiteral("color:#232833;")); sep->setFixedHeight(1);
    l->addWidget(sep);

    // ── 舵机选择 ──
    auto *selRow = new QHBoxLayout; selRow->setSpacing(4);
    m_servoLabel = new QLabel(QStringLiteral("当前: 舵机 1"));
    m_servoLabel->setStyleSheet(QString("font-size:13px;font-weight:bold;color:%1;background:transparent;").arg(C_BLUE));
    selRow->addWidget(m_servoLabel); selRow->addStretch();
    l->addLayout(selRow);

    auto *grid = new QGridLayout; grid->setSpacing(3);
    for (int i = 0; i < 12; i++) {
        m_servoBtns[i] = new QPushButton(QString::number(i + 1));
        m_servoBtns[i]->setMinimumSize(42, 30);
        m_servoBtns[i]->setStyleSheet(QString(
            "QPushButton{font-size:11px;font-weight:bold;color:%1;background:%2;"
            "border:1px solid #2a3145;border-radius:5px;}")
            .arg(C_DIM, C_BG));
        int srv = i + 1;
        connect(m_servoBtns[i], &QPushButton::clicked, this, [this, srv]() {
            // 立即切换本地高亮
            m_currentServo = srv;
            updateServoHighlight();
            m_statusLabel->setText(QStringLiteral("已切换到舵机 %1 (等待确认...)").arg(srv));
            m_statusLabel->setStyleSheet(QString("font-size:11px;color:%1;background:transparent;").arg(C_DIM));
            emit switchServo(srv);
        });
        grid->addWidget(m_servoBtns[i], i / 4, i % 4);
    }
    l->addLayout(grid);
    updateServoHighlight();

    auto *sep2 = new QFrame; sep2->setFrameShape(QFrame::HLine);
    sep2->setStyleSheet(QStringLiteral("color:#232833;")); sep2->setFixedHeight(1);
    l->addWidget(sep2);

    // ── 角度 ──
    m_angleLabel = new QLabel(QStringLiteral("90°"));
    m_angleLabel->setAlignment(Qt::AlignCenter);
    m_angleLabel->setStyleSheet(QString("font-size:40px;font-weight:bold;color:%1;background:transparent;").arg(C_TXT));
    l->addWidget(m_angleLabel);

    // ── 滑块 0~270 ──
    m_slider = new QSlider(Qt::Horizontal);
    m_slider->setRange(0, 270); m_slider->setValue(90);
    m_slider->setStyleSheet(QString(
        "QSlider::groove:horizontal{height:8px;background:%1;border-radius:4px;}"
        "QSlider::handle:horizontal{width:32px;height:32px;margin:-12px 0;"
        "background:%2;border-radius:16px;}"
        "QSlider::sub-page:horizontal{background:%2;border-radius:4px;}")
        .arg(C_ACC, C_BLUE));
    connect(m_slider, &QSlider::valueChanged, this, &ServoWidget::onSliderChanged);
    l->addWidget(m_slider);

    // 刻度
    auto *ticks = new QHBoxLayout;
    for (int v : {0, 45, 90, 135, 180, 225, 270}) {
        auto *lb = new QLabel(QString::number(v));
        lb->setStyleSheet(QString("font-size:9px;color:%1;background:transparent;").arg(C_DIM));
        lb->setAlignment(v == 0 ? Qt::AlignLeft : v == 270 ? Qt::AlignRight : Qt::AlignCenter);
        ticks->addWidget(lb);
    }
    l->addLayout(ticks);

    // ── 快捷调整 + 设置 ──
    auto *adjRow = new QHBoxLayout; adjRow->setSpacing(4);
    auto *btnM10 = mkB(QStringLiteral("-10"), C_ACC, 30, 38);
    auto *btnM1  = mkB(QStringLiteral("-1"),  C_ACC, 30, 38);
    m_btnSet     = mkB(QStringLiteral("设置"), C_BLUE, 36);
    auto *btnP1  = mkB(QStringLiteral("+1"),  C_ACC, 30, 38);
    auto *btnP10 = mkB(QStringLiteral("+10"), C_ACC, 30, 38);
    connect(btnM10, &QPushButton::clicked, this, [this]() { onQuickAdjust(-10); });
    connect(btnM1,  &QPushButton::clicked, this, [this]() { onQuickAdjust(-1); });
    connect(btnP1,  &QPushButton::clicked, this, [this]() { onQuickAdjust(1); });
    connect(btnP10, &QPushButton::clicked, this, [this]() { onQuickAdjust(10); });
    connect(m_btnSet, &QPushButton::clicked, this, &ServoWidget::onSetClicked);
    adjRow->addWidget(btnM10); adjRow->addWidget(btnM1);
    adjRow->addWidget(m_btnSet, 1);
    adjRow->addWidget(btnP1); adjRow->addWidget(btnP10);
    l->addLayout(adjRow);

    // ── 状态 ──
    m_statusLabel = new QLabel(QStringLiteral("选舵机 → 拖滑块 → 点「设置」"));
    m_statusLabel->setAlignment(Qt::AlignCenter);
    m_statusLabel->setStyleSheet(QString("font-size:11px;color:%1;background:transparent;").arg(C_DIM));
    l->addWidget(m_statusLabel);

    // ── 确认栏 ──
    auto *cfRow = new QHBoxLayout; cfRow->setSpacing(8);
    m_btnY = mkB(QStringLiteral("✅ 确认 Y"), C_GREEN, 36);
    m_btnN = mkB(QStringLiteral("❌ 取消 N"), C_RED, 36);
    m_btnY->setEnabled(false); m_btnN->setEnabled(false);
    connect(m_btnY, &QPushButton::clicked, this, [this]() { emit confirmYes(); });
    connect(m_btnN, &QPushButton::clicked, this, [this]() { emit confirmNo(); });
    cfRow->addWidget(m_btnY); cfRow->addWidget(m_btnN);
    l->addLayout(cfRow);

    // ── 已记录 ──
    auto *slotTitle = new QLabel(QStringLiteral("已记录 — 点槽位保存当前角度"));
    slotTitle->setStyleSheet(QString("font-size:10px;color:%1;background:transparent;").arg(C_DIM));
    l->addWidget(slotTitle);

    auto *sg = new QGridLayout; sg->setSpacing(3);
    for (int i = 0; i < 12; i++) {
        m_slotBtns[i] = new QPushButton(QStringLiteral("S%1\n--").arg(i + 1));
        m_slotBtns[i]->setMinimumSize(64, 40);
        m_slotBtns[i]->setStyleSheet(QString(
            "QPushButton{font-size:11px;font-weight:bold;color:%1;background:%2;"
            "border:1px solid #2a3145;border-radius:6px;}")
            .arg(C_DIM, C_BG));
        int idx = i;
        connect(m_slotBtns[i], &QPushButton::clicked, this, [this, idx]() { onSlotClicked(idx); });
        sg->addWidget(m_slotBtns[i], i / 4, i % 4);
    }
    l->addLayout(sg);

    // ── 导出/清空 ──
    auto *ar = new QHBoxLayout; ar->setSpacing(6);
    auto *btnE = mkB(QStringLiteral("导出 CSV"), C_GREEN);
    auto *btnC = mkB(QStringLiteral("清空"), C_RED);
    connect(btnE, &QPushButton::clicked, this, &ServoWidget::onExportCsv);
    connect(btnC, &QPushButton::clicked, this, &ServoWidget::onClearAll);
    ar->addWidget(btnE); ar->addWidget(btnC);
    l->addLayout(ar);
}

void ServoWidget::updateServoHighlight()
{
    for (int i = 0; i < 12; i++) {
        bool cur = (i + 1 == m_currentServo);
        m_servoBtns[i]->setStyleSheet(QString(
            "QPushButton{font-size:11px;font-weight:bold;color:%1;background:%2;"
            "border:2px solid %3;border-radius:5px;}")
            .arg(cur ? C_TXT : C_DIM, cur ? C_BLUE : C_BG, cur ? C_BLUE : QStringLiteral("#2a3145")));
    }
    m_servoLabel->setText(QStringLiteral("当前: 舵机 %1").arg(m_currentServo));
}

void ServoWidget::updateAngleDisplay(int a)
{
    m_currentAngle = a;
    m_angleLabel->setText(QStringLiteral("%1°").arg(a));
    const QString &c = (a >= 0 && a <= 270) ? C_GREEN : C_RED;
    m_angleLabel->setStyleSheet(QString("font-size:40px;font-weight:bold;color:%1;background:transparent;").arg(c));
}

void ServoWidget::onSliderChanged(int val) { updateAngleDisplay(val); }
void ServoWidget::onQuickAdjust(int d) { int v = qBound(0, m_currentAngle + d, 270); m_slider->setValue(v); }
void ServoWidget::onSetClicked()
{
    m_statusLabel->setText(QStringLiteral("📤 已发送 S%1:%2° — 等待确认...").arg(m_currentServo).arg(m_currentAngle));
    m_statusLabel->setStyleSheet(QString("font-size:11px;font-weight:bold;color:%1;background:transparent;").arg(C_BLUE));
    emit servoAngleRequested(m_currentServo, m_currentAngle);
}

void ServoWidget::onSetPending(int srv, int angle)
{
    m_pendingSrv = srv; m_pendingAngle = angle;
    m_statusLabel->setText(QStringLiteral("⏳ S%1 → %2°? — 确认或取消").arg(srv).arg(angle));
    m_statusLabel->setStyleSheet(QString("font-size:11px;font-weight:bold;color:%1;background:transparent;").arg(C_ORANGE));
    m_btnY->setEnabled(true); m_btnN->setEnabled(true);
    m_btnSet->setEnabled(false);
}

void ServoWidget::onOk(int srv, int angle)
{
    m_pendingAngle = -1; m_pendingSrv = 0;
    if (m_currentServo == srv) { updateAngleDisplay(angle); m_slider->setValue(angle); }
    m_statusLabel->setText(QStringLiteral("✅ S%1 = %2° 已到位").arg(srv).arg(angle));
    m_statusLabel->setStyleSheet(QString("font-size:11px;font-weight:bold;color:%1;background:transparent;").arg(C_GREEN));
    m_btnY->setEnabled(false); m_btnN->setEnabled(false);
    m_btnSet->setEnabled(true);
}

void ServoWidget::onCancel()
{
    m_pendingAngle = -1; m_pendingSrv = 0;
    m_statusLabel->setText(QStringLiteral("❌ 已取消"));
    m_statusLabel->setStyleSheet(QString("font-size:11px;font-weight:bold;color:%1;background:transparent;").arg(C_RED));
    m_btnY->setEnabled(false); m_btnN->setEnabled(false);
    m_btnSet->setEnabled(true);
}

void ServoWidget::onSwitched(int srv, int angle)
{
    m_currentServo = srv;
    m_pendingAngle = -1; m_pendingSrv = 0;
    updateServoHighlight();
    m_statusLabel->setText(QStringLiteral("已切换到 舵机 %1").arg(srv));
    m_statusLabel->setStyleSheet(QString("font-size:11px;color:%1;background:transparent;").arg(C_DIM));
    m_btnY->setEnabled(false); m_btnN->setEnabled(false);
    m_btnSet->setEnabled(true);
}

void ServoWidget::onSlotClicked(int idx)
{
    if (m_currentAngle < 0 || m_currentAngle > 270) return;
    m_savedAngles[idx] = m_currentAngle;
    m_slotBtns[idx]->setText(QStringLiteral("S%1\n%2°").arg(idx + 1).arg(m_currentAngle));
    m_slotBtns[idx]->setStyleSheet(QString(
        "QPushButton{font-size:11px;font-weight:bold;color:%1;background:%2;border-radius:6px;}")
        .arg(C_TXT, C_BLUE));
}

void ServoWidget::onExportCsv()
{
    QString path = QFileDialog::getSaveFileName(this,
        QStringLiteral("导出 CSV"), QStringLiteral("servo_calib.csv"), QStringLiteral("CSV (*.csv)"));
    if (path.isEmpty()) return;
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, QStringLiteral("错误"), QStringLiteral("无法写入")); return;
    }
    QTextStream out(&f);
    out << "Servo,Angle\n";
    for (int i = 0; i < 12; i++)
        out << QStringLiteral("S%1,%2\n").arg(i + 1)
            .arg(m_savedAngles[i] >= 0 ? QString::number(m_savedAngles[i]) : QString());
    f.close();
}

void ServoWidget::onClearAll()
{
    for (int i = 0; i < 12; i++) {
        m_savedAngles[i] = -1;
        m_slotBtns[i]->setText(QStringLiteral("S%1\n--").arg(i + 1));
        m_slotBtns[i]->setStyleSheet(QString(
            "QPushButton{font-size:11px;font-weight:bold;color:%1;background:%2;border:1px solid #2a3145;border-radius:6px;}")
            .arg(C_DIM, C_BG));
    }
}

void ServoWidget::reset()
{
    m_currentServo = 1;
    m_pendingAngle = -1; m_pendingSrv = 0;
    updateAngleDisplay(90);
    m_slider->setValue(90);
    updateServoHighlight();
    m_statusLabel->setText(QStringLiteral("选舵机 → 拖滑块 → 点「设置」"));
    m_statusLabel->setStyleSheet(QString("font-size:11px;color:%1;background:transparent;").arg(C_DIM));
    m_btnY->setEnabled(false); m_btnN->setEnabled(false);
    m_btnSet->setEnabled(true);
}
