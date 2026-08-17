#include "motorwidget.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFrame>

static const QString C_BG    = QStringLiteral("#111318");
static const QString C_CARD  = QStringLiteral("#181b22");
static const QString C_TXT   = QStringLiteral("#d0d4dc");
static const QString C_DIM   = QStringLiteral("#6b7280");
static const QString C_BLUE  = QStringLiteral("#5b8def");
static const QString C_GREEN = QStringLiteral("#22c55e");
static const QString C_RED   = QStringLiteral("#ef4444");
static const QString C_ACC   = QStringLiteral("#1e2433");
static const QString MOTOR_NAMES[4] = {
    QStringLiteral("A"), QStringLiteral("B"), QStringLiteral("C"), QStringLiteral("D")
};

static QPushButton *mkB(const QString &t, const QString &bg, int fw = 0)
{
    auto *b = new QPushButton(t);
    b->setMinimumHeight(30);
    if (fw) b->setFixedWidth(fw);
    b->setStyleSheet(QString(
        "QPushButton{border:none;border-radius:6px;padding:4px 8px;"
        "font-size:12px;font-weight:bold;color:#fff;background:%1;}"
        "QPushButton:disabled{background:#2a2a30;color:#555;}").arg(bg));
    return b;
}

MotorWidget::MotorWidget(QWidget *parent) : QWidget(parent) { setupUi(); }

void MotorWidget::setupUi()
{
    setStyleSheet(QString("background:%1;border-radius:10px;").arg(C_CARD));
    auto *l = new QVBoxLayout(this);
    l->setContentsMargins(12, 8, 12, 8); l->setSpacing(4);

    auto *title = new QLabel(QStringLiteral("电机控制"));
    title->setStyleSheet(QString("font-size:13px;font-weight:bold;color:%1;background:transparent;").arg(C_BLUE));
    l->addWidget(title);
    auto *sep = new QFrame; sep->setFrameShape(QFrame::HLine);
    sep->setStyleSheet(QStringLiteral("color:#232833;")); sep->setFixedHeight(1);
    l->addWidget(sep);

    for (int i = 0; i < 4; i++) {
        auto &row = m_rows[i];
        auto *rl = new QHBoxLayout; rl->setSpacing(6);

        row.name = new QLabel(QStringLiteral("电机 %1").arg(MOTOR_NAMES[i]));
        row.name->setFixedWidth(52);
        row.name->setStyleSheet(QString("font-size:13px;font-weight:bold;color:%1;background:transparent;").arg(C_BLUE));
        rl->addWidget(row.name);

        row.speedVal = new QLabel(QStringLiteral("0%"));
        row.speedVal->setFixedWidth(40);
        row.speedVal->setStyleSheet(QString("font-size:13px;font-weight:bold;color:%1;background:transparent;").arg(C_GREEN));
        rl->addWidget(row.speedVal);

        row.slider = new QSlider(Qt::Horizontal);
        row.slider->setRange(0, 100);
        row.slider->setValue(0);
        row.slider->setStyleSheet(QString(
            "QSlider::groove:horizontal{height:6px;background:%1;border-radius:3px;}"
            "QSlider::handle:horizontal{width:24px;height:24px;margin:-9px 0;"
            "background:%2;border-radius:12px;}"
            "QSlider::sub-page:horizontal{background:%2;border-radius:3px;}")
            .arg(C_ACC, C_BLUE));
        rl->addWidget(row.slider, 1);

        row.btnDir = mkB(QStringLiteral("正转"), C_GREEN, 48);
        row.btnSend = mkB(QStringLiteral("发送"), C_BLUE, 48);
        row.btnStop = mkB(QStringLiteral("停止"), C_RED, 48);

        int idx = i;
        // 滑块只改显示, 不发送
        connect(row.slider, &QSlider::valueChanged, this, [this, idx](int v) {
            m_rows[idx].speedVal->setText(QStringLiteral("%1%").arg(v));
        });
        // 方向只切本地状态, 不发送
        connect(row.btnDir, &QPushButton::clicked, this, [this, idx]() {
            auto &r = m_rows[idx];
            r.dir = !r.dir;
            r.btnDir->setText(r.dir ? QStringLiteral("正转") : QStringLiteral("反转"));
            r.btnDir->setStyleSheet(QString(
                "QPushButton{border:none;border-radius:6px;padding:4px 8px;"
                "font-size:12px;font-weight:bold;color:#fff;background:%1;}")
                .arg(r.dir ? C_GREEN : QStringLiteral("#f59e0b")));
        });
        // 发送: 确认速度+方向
        connect(row.btnSend, &QPushButton::clicked, this, [this, idx]() {
            emit motorCmdRequested(idx, m_rows[idx].slider->value(), m_rows[idx].dir);
        });
        // 停止: 归零 + 发送
        connect(row.btnStop, &QPushButton::clicked, this, [this, idx]() {
            m_rows[idx].slider->setValue(0);
            emit motorCmdRequested(idx, 0, m_rows[idx].dir);
        });

        rl->addWidget(row.btnDir);
        rl->addWidget(row.btnSend);
        rl->addWidget(row.btnStop);
        l->addLayout(rl);
    }

    // ── 一键操作 ──
    auto *sep2 = new QFrame; sep2->setFrameShape(QFrame::HLine);
    sep2->setStyleSheet(QStringLiteral("color:#232833;")); sep2->setFixedHeight(1);
    l->addWidget(sep2);

    auto *oneRow = new QHBoxLayout; oneRow->setSpacing(8);
    auto *btnRoll = mkB(QStringLiteral("R 前进"), C_GREEN);
    auto *btnBack = mkB(QStringLiteral("B 后退"), QStringLiteral("#f59e0b"));
    auto *btnStopAll = mkB(QStringLiteral("S 停"), C_RED);
    connect(btnRoll, &QPushButton::clicked, this, [this]() { emit rollAllRequested(); });
    connect(btnBack, &QPushButton::clicked, this, [this]() { emit backAllRequested(); });
    connect(btnStopAll, &QPushButton::clicked, this, [this]() {
        for (int i = 0; i < 4; i++) {
            m_rows[i].slider->setValue(0);
            m_rows[i].speedVal->setText(QStringLiteral("0%"));
        }
        emit stopAllRequested();
    });
    oneRow->addWidget(btnRoll);
    oneRow->addWidget(btnBack);
    oneRow->addWidget(btnStopAll);
    l->addLayout(oneRow);
}

void MotorWidget::updateRow(int i)
{
    auto &r = m_rows[i];
    r.speedVal->setText(QStringLiteral("%1%").arg(r.slider->value()));
}

void MotorWidget::onMotorResponse(const MotorResponse &m)
{
    if (!m.valid || m.motorNum < 0 || m.motorNum > 3) return;
    auto &r = m_rows[m.motorNum];
    r.slider->setValue(m.speed);
    r.dir = m.dir;
    r.btnDir->setText(m.dir ? QStringLiteral("正转") : QStringLiteral("反转"));
    r.btnDir->setStyleSheet(QString(
        "QPushButton{border:none;border-radius:6px;padding:4px 8px;"
        "font-size:12px;font-weight:bold;color:#fff;background:%1;}")
        .arg(m.dir ? C_GREEN : QStringLiteral("#f59e0b")));
    updateRow(m.motorNum);
}

void MotorWidget::reset()
{
    for (int i = 0; i < 4; i++) {
        m_rows[i].slider->setValue(0);
        m_rows[i].dir = 1;
        m_rows[i].btnDir->setText(QStringLiteral("正转"));
        m_rows[i].btnDir->setStyleSheet(QString(
            "QPushButton{border:none;border-radius:6px;padding:4px 8px;"
            "font-size:12px;font-weight:bold;color:#fff;background:%1;}").arg(C_GREEN));
        updateRow(i);
    }
}
