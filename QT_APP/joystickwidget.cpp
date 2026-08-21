#include "joystickwidget.h"
#include <QPainter>
#include <QMouseEvent>
#include <QtMath>

JoystickWidget::JoystickWidget(QWidget *parent) : QWidget(parent)
{
    setMinimumSize(200, 200);
    setMaximumSize(240, 240);
    m_timer = new QTimer(this);
    m_timer->setInterval(100);  // 协议: 握住摇杆时每 50~100ms 发一次
    connect(m_timer, &QTimer::timeout, this, [this]() {
        emit joystickMoved(m_sp, m_st);
    });
}

void JoystickWidget::resizeEvent(QResizeEvent *)
{
    updateFromPos(QPoint(width() / 2, height() / 2));
}

void JoystickWidget::updateFromPos(const QPoint &pos)
{
    QPointF c(width() / 2.0, height() / 2.0);
    m_radius = qMin(width(), height()) / 2.0 - m_knobR - 6;
    QPointF d = pos - c;
    double len = qSqrt(d.x() * d.x() + d.y() * d.y());
    if (len > m_radius && len > 0.5) d *= m_radius / len;   // 限幅在圆内
    if (len < 10) d = QPointF(0, 0);                        // 死区

    m_knobPos = c + d;
    // 上=前进(sp正) 右=右转(st正), 满幅 ±50
    m_sp = qRound(-d.y() / m_radius * 50);
    m_st = qRound(d.x() / m_radius * 50);
    update();
}

void JoystickWidget::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    QPointF c(width() / 2.0, height() / 2.0);

    // 底座外圈
    p.setPen(QPen(QColor(QStringLiteral("#3b82f6")), 2));
    p.setBrush(QColor(QStringLiteral("#2a2d33")));
    p.drawEllipse(c, m_radius, m_radius);

    // 内圈 + 十字线
    p.setPen(QPen(QColor(QStringLiteral("#3a3f47")), 1));
    p.setBrush(Qt::NoBrush);
    p.drawEllipse(c, m_radius * 0.66, m_radius * 0.66);
    p.drawLine(QPointF(c.x() - m_radius, c.y()), QPointF(c.x() + m_radius, c.y()));
    p.drawLine(QPointF(c.x(), c.y() - m_radius), QPointF(c.x(), c.y() + m_radius));

    // 摇杆头
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(QStringLiteral("#3b82f6")));
    p.drawEllipse(m_knobPos, m_knobR, m_knobR);
    p.setBrush(QColor(255, 255, 255, 50));
    p.drawEllipse(m_knobPos - QPointF(m_knobR * 0.3, m_knobR * 0.3), m_knobR * 0.55, m_knobR * 0.55);
}

void JoystickWidget::mousePressEvent(QMouseEvent *e)
{
    if (e->button() != Qt::LeftButton) return;
    m_tracking = true;
    m_moved = false;
    updateFromPos(e->position().toPoint());
    m_timer->start();
}

void JoystickWidget::mouseMoveEvent(QMouseEvent *e)
{
    if (!m_tracking) return;
    updateFromPos(e->position().toPoint());
    m_moved = true;
}

void JoystickWidget::mouseReleaseEvent(QMouseEvent *e)
{
    if (e->button() != Qt::LeftButton || !m_tracking) return;
    m_tracking = false;
    m_timer->stop();
    bool wasMoved = m_moved;
    updateFromPos(QPoint(width() / 2, height() / 2));  // 回中
    if (wasMoved) emit joystickCentered();             // 松开 → V:0:0
}
