#include "pidwidget.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFrame>
#include <QPainter>

static const QString C_BG   = QStringLiteral("#141518");
static const QString C_CARD = QStringLiteral("#1e2024");
static const QString C_DIM  = QStringLiteral("#6b7280");
static const QString C_TXT  = QStringLiteral("#e5e7eb");
static const QString C_BLUE = QStringLiteral("#3b82f6");
static const QString C_GREEN= QStringLiteral("#22c55e");
static const QString C_YELLOW=QStringLiteral("#f59e0b");
static const QString C_RED  = QStringLiteral("#ef4444");
static const QString C_ACC  = QStringLiteral("#2a2d33");

// ════════════════════════════════════════════════════════════
//  WaveChart
// ════════════════════════════════════════════════════════════

WaveChart::WaveChart(QWidget *parent) : QWidget(parent)
{
    setMinimumHeight(160);
    setStyleSheet(QString("background:%1;border-radius:8px;").arg(C_BG));
}
void WaveChart::addPoint(const WavePoint &pt)
{
    m_data.append(pt);
    while (m_data.size() > m_maxPoints) m_data.removeFirst();
    update();
}
void WaveChart::clear() { m_data.clear(); update(); }

void WaveChart::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    int w = width(), h = height(), n = m_data.size();

    QFont f; f.setPixelSize(10); p.setFont(f);
    QFontMetrics fm(f);

    if (n < 2) {
        p.setPen(QColor(C_DIM));
        p.drawText(rect(), Qt::AlignCenter, QStringLiteral("等待波形数据..."));
        return;
    }

    int ml = 38, mr = 8, mt = 14, mb = 22;
    int pw = w - ml - mr, ph = h - mt - mb;
    int vMax = 300;

    // 网格
    p.setPen(QPen(QColor(QStringLiteral("#2a2d33")), 1));
    for (int i = 0; i <= 4; i++) {
        int y = mt + ph * i / 4;
        p.drawLine(ml, y, ml + pw, y);
    }
    for (int i = 0; i <= 4; i++) {
        int x = ml + pw * i / 4;
        p.drawLine(x, mt, x, mt + ph);
    }

    // Y 刻度
    for (int i = 0; i <= 4; i++) {
        int val = vMax * (4 - i) / 4;
        p.setPen(QColor(C_DIM));
        p.drawText(2, mt + ph * i / 4 - 6, ml - 6, 14,
                   Qt::AlignRight | Qt::AlignVCenter, QString::number(val));
    }

    // 目标线 (绿色虚线)
    int tgt = m_data.last().targetRpm;
    if (tgt > 0 && tgt <= vMax) {
        int ty = mt + ph - (tgt * ph / vMax);
        ty = qBound(mt, ty, mt + ph);
        QPen dash(QColor(C_GREEN), 1, Qt::DashLine);
        p.setPen(dash); p.drawLine(ml, ty, ml + pw, ty);
        p.setPen(QColor(C_GREEN));
        p.drawText(ml + pw + 2, ty - 6, mr, 12, Qt::AlignLeft, QString::number(tgt));
    }

    // 实际转速 (黄色曲线)
    p.setPen(QPen(QColor(C_YELLOW), 2.5));
    for (int i = 1; i < n; i++) {
        int x1 = ml + pw * (i - 1) / (n - 1);
        int x2 = ml + pw * i / (n - 1);
        int y1 = mt + ph - (m_data[i-1].actualRpm * ph / vMax);
        int y2 = mt + ph - (m_data[i].actualRpm * ph / vMax);
        p.drawLine(x1, qBound(mt, y1, mt+ph), x2, qBound(mt, y2, mt+ph));
    }

    // PID 输出 (蓝色半透明柱)
    QColor blue(QStringLiteral("#3b82f6")); blue.setAlpha(45);
    p.setPen(Qt::NoPen); p.setBrush(blue);
    for (int i = 0; i < n; i++) {
        int x = ml + pw * i / (n - 1);
        int barH = qAbs(m_data[i].pidOut) * ph / 250;
        int base = mt + ph - 4;
        p.drawRect(x - 1, base - barH, 3, barH);
    }

    // 图例
    f.setPixelSize(10); p.setFont(f);
    p.setPen(QColor(C_YELLOW)); p.drawText(ml, mt + ph + 15, QStringLiteral("实际"));
    p.setPen(QColor(C_GREEN));  p.drawText(ml + 50, mt + ph + 15, QStringLiteral("目标"));
    p.setPen(QColor(C_BLUE));   p.drawText(ml + 100, mt + ph + 15, QStringLiteral("PID Out"));
}

// ════════════════════════════════════════════════════════════
//  PidWidget
// ════════════════════════════════════════════════════════════

static QPushButton *mkB(const QString &t, const QString &bg, int fw = 0)
{
    auto *b = new QPushButton(t);
    b->setMinimumHeight(28);
    if (fw) b->setFixedWidth(fw);
    b->setStyleSheet(QString(
        "QPushButton{border:none;border-radius:6px;padding:4px 8px;"
        "font-size:11px;font-weight:bold;color:#fff;background:%1;}"
        "QPushButton:disabled{background:#2a2a30;color:#555;}").arg(bg));
    return b;
}

PidWidget::PidWidget(QWidget *parent) : QWidget(parent) { setupUi(); }

void PidWidget::setupUi()
{
    setStyleSheet(QString("background:%1;border-radius:10px;").arg(C_CARD));
    auto *l = new QVBoxLayout(this);
    l->setContentsMargins(10, 8, 10, 8); l->setSpacing(5);

    // 标题
    auto *title = new QLabel(QStringLiteral("PID 调参"));
    title->setStyleSheet(QString("font-size:13px;font-weight:bold;color:%1;background:transparent;").arg(C_BLUE));
    l->addWidget(title);
    auto *sep = new QFrame; sep->setFrameShape(QFrame::HLine);
    sep->setStyleSheet(QStringLiteral("color:#232833;")); sep->setFixedHeight(1);
    l->addWidget(sep);

    // 波形图
    m_chart = new WaveChart;
    l->addWidget(m_chart, 3);

    // 实时数值
    auto *vr = new QHBoxLayout; vr->setSpacing(12);
    m_rpmLabel = new QLabel(QStringLiteral("实际: 0 RPM"));
    m_rpmLabel->setStyleSheet(QString("font-size:13px;font-weight:bold;color:%1;background:transparent;").arg(C_YELLOW));
    m_targetLabel = new QLabel(QStringLiteral("目标: 0 RPM"));
    m_targetLabel->setStyleSheet(QString("font-size:13px;font-weight:bold;color:%1;background:transparent;").arg(C_GREEN));
    vr->addWidget(m_rpmLabel); vr->addWidget(m_targetLabel); vr->addStretch();
    l->addLayout(vr);

    // ── 参数 4 行 ──
    auto addRow = [&](const QString &name, QLabel *&val, const QString &cmdPrefix) {
        auto *row = new QHBoxLayout; row->setSpacing(4);
        auto *nm = new QLabel(name);
        nm->setFixedWidth(32);
        nm->setStyleSheet(QString("font-size:13px;font-weight:bold;color:%1;background:transparent;").arg(C_BLUE));
        row->addWidget(nm);
        val = new QLabel(QStringLiteral("0"));
        val->setFixedWidth(38);
        val->setStyleSheet(QString("font-size:13px;font-weight:bold;color:%1;background:transparent;").arg(C_TXT));
        row->addWidget(val);

        // 各参数粗微调步长
        int step1, step2;  // -- / ++ 的步长
        if (cmdPrefix == QStringLiteral("P"))      { step1 = 5; step2 = 5; }
        else if (cmdPrefix == QStringLiteral("I")) { step1 = 2; step2 = 2; }
        else if (cmdPrefix == QStringLiteral("D")) { step1 = 5; step2 = 5; }
        // Kd 存的是 10 倍值 (1=0.1), step 保持不变因为 MCU 相对模式 +1=+0.1
        else /* R */                               { step1 = 10; step2 = 10; }

        int *pidPtr = (cmdPrefix == QStringLiteral("P")) ? &m_pid.kp :
                      (cmdPrefix == QStringLiteral("I")) ? &m_pid.ki :
                      (cmdPrefix == QStringLiteral("D")) ? &m_pid.kd : &m_pid.targetRpm;

        auto mk = [&](const QString &t, int d) {
            auto *btn = mkB(t, C_ACC, 32);
            QObject::connect(btn, &QPushButton::clicked, this,
                [this, s = cmdPrefix + t.toLower(), d, pidPtr, val, cmdPrefix]() {
                *pidPtr = qMax(0, *pidPtr + d);
                // Kd 存 10 倍值, 显示除 10
                if (cmdPrefix == QStringLiteral("D"))
                    val->setText(QString::number(*pidPtr / 10.0, 'f', 1));
                else
                    val->setText(QString::number(*pidPtr));
                emit commandRequested(s);
            });
            row->addWidget(btn);
        };
        mk(QStringLiteral("--"), -step1);
        mk(QStringLiteral("-"),  -1);
        mk(QStringLiteral("+"),   1);
        mk(QStringLiteral("++"),  step2);
        row->addStretch();
        l->addLayout(row);
    };
    addRow(QStringLiteral("Kp"), m_kpVal, QStringLiteral("P"));
    addRow(QStringLiteral("Ki"), m_kiVal, QStringLiteral("I"));
    addRow(QStringLiteral("Kd"), m_kdVal, QStringLiteral("D"));
    addRow(QStringLiteral("Tgt"), m_tgtVal, QStringLiteral("R"));

    // GO / STOP
    auto *act = new QHBoxLayout; act->setSpacing(8);
    m_btnGo = mkB(QStringLiteral("▶ GO"), C_GREEN, 70);
    m_btnStop = mkB(QStringLiteral("■ STOP"), C_RED, 70);
    m_btnGo->setEnabled(true); m_btnStop->setEnabled(false);
    QObject::connect(m_btnGo, &QPushButton::clicked, this, [this]() { emit commandRequested(QStringLiteral("G")); });
    QObject::connect(m_btnStop, &QPushButton::clicked, this, [this]() { emit commandRequested(QStringLiteral("S")); });
    act->addWidget(m_btnGo); act->addWidget(m_btnStop); act->addStretch();
    l->addLayout(act);
}

void PidWidget::updateParamDisplay()
{
    m_kpVal->setText(QString::number(m_pid.kp));
    m_kiVal->setText(QString::number(m_pid.ki));
    m_kdVal->setText(QString::number(m_pid.kd / 10.0, 'f', 1));  /* Kd 显示除 10 */
    m_tgtVal->setText(QString::number(m_pid.targetRpm));
}

void PidWidget::handleMessage(const PidMessage &msg)
{
    switch (msg.type) {
    case PidWave:
        m_chart->addPoint(msg.wave);
        m_rpmLabel->setText(QStringLiteral("实际: %1 RPM").arg(msg.wave.actualRpm));
        m_targetLabel->setText(QStringLiteral("目标: %1 RPM").arg(msg.wave.targetRpm));
        break;
    case PidKp:     m_pid.kp = msg.params.kp; updateParamDisplay(); break;
    case PidKi:     m_pid.ki = msg.params.ki; updateParamDisplay(); break;
    case PidKd:     m_pid.kd = msg.params.kd; updateParamDisplay(); break;
    case PidTarget: m_pid.targetRpm = msg.params.targetRpm; updateParamDisplay(); break;
    case PidGo:
        m_pid.running = true;
        m_btnGo->setEnabled(false); m_btnStop->setEnabled(true);
        break;
    case PidStop:
        m_pid.running = false;
        m_btnGo->setEnabled(true); m_btnStop->setEnabled(false);
        break;
    default: break;
    }
}

void PidWidget::onReady()
{
    m_chart->clear();
    m_btnGo->setEnabled(true); m_btnStop->setEnabled(false);
}

void PidWidget::reset()
{
    m_chart->clear();
    m_pid = PidParams{};
    m_rpmLabel->setText(QStringLiteral("实际: 0 RPM"));
    m_targetLabel->setText(QStringLiteral("目标: 0 RPM"));
    m_kpVal->setText(QStringLiteral("0"));
    m_kiVal->setText(QStringLiteral("0"));
    m_kdVal->setText(QStringLiteral("0.0"));
    m_tgtVal->setText(QStringLiteral("0"));
    m_btnGo->setEnabled(true); m_btnStop->setEnabled(false);
}
