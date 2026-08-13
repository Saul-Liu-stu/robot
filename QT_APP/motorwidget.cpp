#include "motorwidget.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFrame>
#include <QtMath>

// ── 配色 ────────────────────────────────────────────────────────
const QString MotorWidget::C_BG      = QStringLiteral("#111318");
const QString MotorWidget::C_CARD    = QStringLiteral("#181b22");
const QString MotorWidget::C_TXT     = QStringLiteral("#d0d4dc");
const QString MotorWidget::C_DIM     = QStringLiteral("#6b7280");
const QString MotorWidget::C_ACCENT  = QStringLiteral("#5b8def");
const QString MotorWidget::C_GREEN   = QStringLiteral("#22c55e");
const QString MotorWidget::C_YELLOW  = QStringLiteral("#f59e0b");
const QString MotorWidget::C_RED     = QStringLiteral("#ef4444");
const QString MotorWidget::MOTOR_NAMES[4] = {
    QStringLiteral("电机 A"),
    QStringLiteral("电机 B"),
    QStringLiteral("电机 C"),
    QStringLiteral("电机 D")
};

MotorWidget::MotorWidget(int motorIndex, QWidget *parent)
    : QWidget(parent), m_index(motorIndex)
{
    setupUi();
}

void MotorWidget::setupUi()
{
    setStyleSheet(QStringLiteral("background:transparent;"));

    auto *card = new QFrame;
    card->setStyleSheet(QString(
        "QFrame{background:%1;border:1px solid %2;border-radius:10px;}")
        .arg(C_CARD, QStringLiteral("#232833")));
    card->setMinimumHeight(145);

    auto *mainLayout = new QVBoxLayout(card);
    mainLayout->setContentsMargins(14, 10, 14, 10);
    mainLayout->setSpacing(6);

    // ── 标题 ──
    m_titleLabel = new QLabel(MOTOR_NAMES[m_index]);
    m_titleLabel->setStyleSheet(QString(
        "font-size:14px;font-weight:bold;color:%1;background:transparent;")
        .arg(C_ACCENT));
    mainLayout->addWidget(m_titleLabel);

    // 分隔线
    auto *sep = new QFrame;
    sep->setFrameShape(QFrame::HLine);
    sep->setStyleSheet(QStringLiteral("color:#232833;background:transparent;"));
    sep->setFixedHeight(1);
    mainLayout->addWidget(sep);

    // ── 编码器计数 ──
    auto *encRow = new QHBoxLayout;
    auto *encLbl = new QLabel(QStringLiteral("编码器:"));
    encLbl->setStyleSheet(QString("font-size:11px;color:%1;background:transparent;").arg(C_DIM));
    encRow->addWidget(encLbl);

    m_encoderLabel = new QLabel(QStringLiteral("--"));
    m_encoderLabel->setStyleSheet(QString(
        "font-size:20px;font-weight:bold;color:%1;background:transparent;")
        .arg(C_TXT));
    encRow->addWidget(m_encoderLabel, 1, Qt::AlignRight);
    mainLayout->addLayout(encRow);

    // ── 转速 ──
    auto *rpmRow = new QHBoxLayout;
    auto *rpmLbl = new QLabel(QStringLiteral("转速:"));
    rpmLbl->setStyleSheet(QString("font-size:11px;color:%1;background:transparent;").arg(C_DIM));
    rpmRow->addWidget(rpmLbl);

    m_rpmLabel = new QLabel(QStringLiteral("-- RPM"));
    m_rpmLabel->setStyleSheet(QString(
        "font-size:20px;font-weight:bold;color:%1;background:transparent;")
        .arg(C_GREEN));
    rpmRow->addWidget(m_rpmLabel, 1, Qt::AlignRight);
    mainLayout->addLayout(rpmRow);

    // ── RPM 进度条 ──
    m_rpmBar = new QProgressBar;
    m_rpmBar->setRange(0, 500);  // 最大 500 RPM (可自适应调整)
    m_rpmBar->setValue(0);
    m_rpmBar->setTextVisible(false);
    m_rpmBar->setFixedHeight(6);
    m_rpmBar->setStyleSheet(QString(
        "QProgressBar{background:%1;border:none;border-radius:3px;}"
        "QProgressBar::chunk{background:%2;border-radius:3px;}")
        .arg(QStringLiteral("#111318"), C_ACCENT));
    mainLayout->addWidget(m_rpmBar);

    auto *outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(0, 0, 0, 0);
    outerLayout->addWidget(card);
}

void MotorWidget::updateData(const MotorInfo &info)
{
    // 编码器计数 (千分位格式化)
    m_encoderLabel->setText(QString::number(info.encoderCount));

    // 转速
    int rpm = qAbs(info.rpm);
    QString rpmText = QString::number(info.rpm) + QStringLiteral(" RPM");

    // 根据转速着色: 绿色 (低) → 黄色 (中) → 红色 (高)
    const QString &rpmColor = rpm > 300 ? C_RED
                            : rpm > 150 ? C_YELLOW
                            : C_GREEN;

    m_rpmLabel->setText(rpmText);
    m_rpmLabel->setStyleSheet(QString(
        "font-size:20px;font-weight:bold;color:%1;background:transparent;")
        .arg(rpmColor));

    // 进度条: 自适应最大值
    if (rpm > m_rpmBar->maximum()) {
        m_rpmBar->setMaximum(rpm + 50);
    }
    m_rpmBar->setValue(rpm);

    // 进度条颜色跟随变化
    m_rpmBar->setStyleSheet(QString(
        "QProgressBar{background:%1;border:none;border-radius:3px;}"
        "QProgressBar::chunk{background:%2;border-radius:3px;}")
        .arg(QStringLiteral("#111318"), rpmColor));
}

void MotorWidget::reset()
{
    m_encoderLabel->setText(QStringLiteral("--"));
    m_encoderLabel->setStyleSheet(QString(
        "font-size:20px;font-weight:bold;color:%1;background:transparent;")
        .arg(C_TXT));

    m_rpmLabel->setText(QStringLiteral("-- RPM"));
    m_rpmLabel->setStyleSheet(QString(
        "font-size:20px;font-weight:bold;color:%1;background:transparent;")
        .arg(C_GREEN));

    m_rpmBar->setValue(0);
    m_rpmBar->setMaximum(500);
    m_rpmBar->setStyleSheet(QString(
        "QProgressBar{background:%1;border:none;border-radius:3px;}"
        "QProgressBar::chunk{background:%2;border-radius:3px;}")
        .arg(QStringLiteral("#111318"), C_ACCENT));
}
