#include "gimbaldial.h"
#include <QPainter>
#include <QMouseEvent>

GimbalDial::GimbalDial(QWidget *parent) : QWidget(parent)
{
    setFixedSize(200, 200);   // 紧凑居中, 手指从圆心到边缘仅约90px, 好拨
}

double GimbalDial::trackR() const
{
    const QRectF r = QRectF(rect()).adjusted(12, 12, -12, -12);
    return r.width() / 2.0;
}

void GimbalDial::setValue(int pan, int tilt)
{
    m_pan = qBound(0, pan, 180);
    m_tilt = qBound(75, tilt, 180);
    update();
}

// 两轴值→游标点: 圆心=正前(90/120); 右=pan0, 左=pan180, 上=tilt180, 下=tilt75
QPointF GimbalDial::posForValues() const
{
    const QPointF c = QRectF(rect()).center();
    const double R = trackR();
    const double dx = (90.0 - m_pan) / 90.0 * R;                        // pan<90 右移 (往右)
    const double dy = m_tilt <= 120 ? (120.0 - m_tilt) / 45.0 * R        // tilt<120 下移 (往下)
                                    : -(m_tilt - 120.0) / 60.0 * R;      // tilt>120 上移 (往上)
    return c + QPointF(dx, dy);
}

// 位置→两轴值 (与 posForValues 互逆; 上下分段线性: 上段60°跨度/下段45°跨度)
void GimbalDial::updateFromPos(const QPointF &p)
{
    const QPointF c = QRectF(rect()).center();
    const double R = trackR();
    const double dx = qBound(-R, p.x() - c.x(), R);
    const double dy = qBound(-R, p.y() - c.y(), R);
    const int pan  = qRound(90.0 - dx / R * 90.0);
    const int tilt = dy < 0 ? qRound(120.0 - dy / R * 60.0)
                            : qRound(120.0 - dy / R * 45.0);
    if (pan == m_pan && tilt == m_tilt) return;
    m_pan = pan; m_tilt = tilt;
    update();
    emit valuesChanged(pan, tilt);
}

void GimbalDial::mousePressEvent(QMouseEvent *e)
{
    m_dragging = true;
    emit dragStarted();
    updateFromPos(e->position());
}

void GimbalDial::mouseMoveEvent(QMouseEvent *e)
{
    if (m_dragging)
        updateFromPos(e->position());
}

void GimbalDial::mouseReleaseEvent(QMouseEvent *e)
{
    Q_UNUSED(e);
    if (!m_dragging) return;
    m_dragging = false;
    updateFromPos(e->position());
    emit dragEnded(m_pan, m_tilt);
}

void GimbalDial::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    const QRectF r = QRectF(rect()).adjusted(3, 3, -3, -3);
    const QPointF c = r.center();
    const double R = trackR();

    // 底盘
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(QStringLiteral("#1e2024")));
    p.drawEllipse(r);

    // 静态十字参考线 (正前=中心, 四向指示)
    QPen guidePen(QColor(QStringLiteral("#2a2d33")), 2.0, Qt::DashLine);
    p.setPen(guidePen);
    p.drawLine(QPointF(c.x() - R, c.y()), QPointF(c.x() + R, c.y()));
    p.drawLine(QPointF(c.x(), c.y() - R), QPointF(c.x(), c.y() + R));
    // 外环
    QPen ringPen(QColor(QStringLiteral("#2a2d33")), 3.0);
    p.setPen(ringPen);
    p.drawEllipse(r);

    // 四向文字标记
    QFont f = font();
    f.setPixelSize(12);
    f.setBold(true);
    p.setFont(f);
    p.setPen(QColor(QStringLiteral("#6b7280")));
    const double rt = R - 14.0;
    p.drawText(QRectF(c.x() - rt, c.y() - rt, rt * 2, 16), Qt::AlignHCenter | Qt::AlignTop,  QStringLiteral("▲ 抬"));
    p.drawText(QRectF(c.x() - rt, c.y() + rt - 16, rt * 2, 16), Qt::AlignHCenter | Qt::AlignBottom, QStringLiteral("▼ 压"));
    p.drawText(QRectF(c.x() - rt, c.y() - 10, 28, 20), Qt::AlignLeft | Qt::AlignVCenter, QStringLiteral("◀左"));
    p.drawText(QRectF(c.x() + rt - 28, c.y() - 10, 28, 20), Qt::AlignRight | Qt::AlignVCenter, QStringLiteral("右▶"));

    // 游标点 (蓝点白边, 指示当前两轴姿态)
    const QPointF dot = posForValues();
    p.setPen(QPen(QColor(QStringLiteral("#e5e7eb")), 2.5));
    p.setBrush(QColor(QStringLiteral("#3b82f6")));
    p.drawEllipse(dot, 9, 9);

    // 中心下方两行: 当前角度
    f.setPixelSize(11);
    p.setFont(f);
    p.setPen(QColor(QStringLiteral("#e5e7eb")));
    p.drawText(QRectF(c.x() - 70, c.y() + 8, 140, 30),
               Qt::AlignHCenter | Qt::AlignTop,
               QStringLiteral("水平 %1°\n俯仰 %2°").arg(m_pan).arg(m_tilt));
}
