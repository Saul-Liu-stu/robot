#include "imuwidget.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFrame>
#include <QtMath>

const QString ImuWidget::C_BG     = QStringLiteral("#111318");
const QString ImuWidget::C_CARD   = QStringLiteral("#181b22");
const QString ImuWidget::C_TXT    = QStringLiteral("#d0d4dc");
const QString ImuWidget::C_DIM    = QStringLiteral("#6b7280");
const QString ImuWidget::C_GREEN  = QStringLiteral("#22c55e");
const QString ImuWidget::C_YELLOW = QStringLiteral("#f59e0b");
const QString ImuWidget::C_RED    = QStringLiteral("#ef4444");
const QString ImuWidget::C_BLUE   = QStringLiteral("#5b8def");

ImuWidget::ImuWidget(QWidget *parent) : QWidget(parent)
{
    setupUi();
}

static QLabel *makeAxisLabel(const QString &text)
{
    auto *l = new QLabel(text);
    l->setFixedWidth(55);
    l->setStyleSheet(QString(
        "font-size:16px;font-weight:bold;color:%1;background:transparent;")
        .arg(ImuWidget::C_BLUE));
    return l;
}

static QLabel *makeAxisValue()
{
    auto *l = new QLabel(QStringLiteral("--"));
    l->setFixedWidth(90);
    l->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    l->setStyleSheet(QString(
        "font-size:22px;font-weight:bold;color:%1;background:transparent;")
        .arg(ImuWidget::C_TXT));
    return l;
}

static QWidget *makeBarBg()
{
    auto *w = new QWidget;
    w->setFixedHeight(8);
    w->setStyleSheet(QString(
        "background:%1;border-radius:4px;").arg(QStringLiteral("#232833")));
    return w;
}

static QWidget *makeBarFill()
{
    auto *w = new QWidget;
    w->setFixedHeight(8);
    w->setStyleSheet(QString(
        "background:%1;border-radius:4px;").arg(ImuWidget::C_GREEN));
    return w;
}

void ImuWidget::setupUi()
{
    setStyleSheet(QString("background:%1;border-radius:10px;").arg(C_CARD));

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(16, 12, 16, 12);
    layout->setSpacing(8);

    // ── 标题 ──
    auto *titleLabel = new QLabel(QStringLiteral("IMU 姿态角"));
    titleLabel->setStyleSheet(QString(
        "font-size:13px;font-weight:bold;color:%1;background:transparent;")
        .arg(C_BLUE));
    layout->addWidget(titleLabel);

    // ── 分隔线 ──
    auto *sep = new QFrame;
    sep->setFrameShape(QFrame::HLine);
    sep->setStyleSheet(QStringLiteral("color:#232833;"));
    sep->setFixedHeight(1);
    layout->addWidget(sep);

    // ── Roll / Pitch / Yaw 三行 ──────────────────────────────
    auto buildAxis = [&](const QString &name, AxisWidget &ax) {
        auto *row = new QHBoxLayout;
        row->setSpacing(8);

        ax.label = makeAxisLabel(name);
        ax.value = makeAxisValue();

        auto *barContainer = new QWidget;
        auto *barLayout = new QHBoxLayout(barContainer);
        barLayout->setContentsMargins(0, 0, 0, 0);

        ax.barBg = makeBarBg();
        ax.barFill = makeBarFill();
        ax.barFill->setParent(ax.barBg);
        ax.barFill->setFixedWidth(0);

        barLayout->addWidget(ax.barBg);

        row->addWidget(ax.label);
        row->addWidget(ax.value);
        row->addWidget(barContainer, 1);
        layout->addLayout(row);
    };

    buildAxis(QStringLiteral("Roll"),  m_roll);
    buildAxis(QStringLiteral("Pitch"), m_pitch);
    buildAxis(QStringLiteral("Yaw"),   m_yaw);

    // ── 编码器备用区 (折叠) ──
    m_encoderTitle = new QLabel;
    m_encoderTitle->setStyleSheet(QString(
        "font-size:10px;color:%1;background:transparent;").arg(C_DIM));
    m_encoderTitle->hide();
    layout->addWidget(m_encoderTitle);

    auto *encRow = new QHBoxLayout;
    encRow->setSpacing(4);
    for (int i = 0; i < 4; i++) {
        m_encoderVals[i] = new QLabel;
        m_encoderVals[i]->setStyleSheet(QString(
            "font-size:10px;color:%1;background:%2;border-radius:4px;"
            "padding:2px 6px;").arg(C_TXT, C_BG));
        m_encoderVals[i]->hide();
        encRow->addWidget(m_encoderVals[i]);
    }
    layout->addLayout(encRow);
}

void ImuWidget::setAxisVal(const AxisWidget &ax, float val, const QString &unit)
{
    ax.value->setText(QStringLiteral("%1%2").arg(val, 0, 'f', 1).arg(unit));

    // 颜色: 0±10 绿色, ±10~±30 黄色, >±30 红色
    float a = qAbs(val);
    const QString &clr = a > 30 ? C_RED : a > 10 ? C_YELLOW : C_GREEN;
    ax.value->setStyleSheet(QString(
        "font-size:22px;font-weight:bold;color:%1;background:transparent;").arg(clr));

    // 进度条: range -180~+180, center at 0
    int barW = ax.barBg->width();
    if (barW <= 0) barW = 150;
    float ratio = (val + 180.0f) / 360.0f;  // 0~1
    ratio = qBound(0.0f, ratio, 1.0f);
    ax.barFill->setFixedWidth(static_cast<int>(barW * ratio));
    ax.barFill->setStyleSheet(QString(
        "background:%1;border-radius:4px;").arg(clr));

    // 标记中心线
}

void ImuWidget::updateData(const ImuData &imu)
{
    m_frameCount++;
    setAxisVal(m_roll,  imu.roll,  QStringLiteral("°"));
    setAxisVal(m_pitch, imu.pitch, QStringLiteral("°"));
    setAxisVal(m_yaw,   imu.yaw,   QStringLiteral("°"));
}

void ImuWidget::updateEncoder(const EncoderData &enc)
{
    m_lastEnc = enc;
    m_encoderTitle->setText(QStringLiteral("编码器 (备用) — A:%1 RPM  B:%2 RPM  C:%3 RPM  D:%4 RPM")
        .arg(enc.motors[0].rpm).arg(enc.motors[1].rpm)
        .arg(enc.motors[2].rpm).arg(enc.motors[3].rpm));
    m_encoderTitle->show();

    static const QString names[4] = {
        QStringLiteral("A"), QStringLiteral("B"),
        QStringLiteral("C"), QStringLiteral("D")
    };
    for (int i = 0; i < 4; i++) {
        m_encoderVals[i]->setText(QStringLiteral("%1:%2/%3")
            .arg(names[i]).arg(enc.motors[i].encoderCount).arg(enc.motors[i].rpm));
        m_encoderVals[i]->show();
    }
}

void ImuWidget::reset()
{
    m_frameCount = 0;
    auto resetAxis = [](const AxisWidget &ax) {
        ax.value->setText(QStringLiteral("--"));
        ax.value->setStyleSheet(QString(
            "font-size:22px;font-weight:bold;color:%1;background:transparent;")
            .arg(C_TXT));
        ax.barFill->setFixedWidth(0);
    };
    resetAxis(m_roll);
    resetAxis(m_pitch);
    resetAxis(m_yaw);

    m_encoderTitle->hide();
    for (int i = 0; i < 4; i++) m_encoderVals[i]->hide();
}
