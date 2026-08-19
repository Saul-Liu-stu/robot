#ifndef JOYSTICKWIDGET_H
#define JOYSTICKWIDGET_H

#include <QWidget>
#include <QPointF>
#include <QTimer>

// ============================================================
// 虚拟摇杆 (v6.4 V 命令)
// 上下 = 前进/后退 (sp -50~+50)   左右 = 左转/右转 (st -50~+50)
// 按住期间每 100ms 发 joystickMoved; 松开回中发一次 joystickCentered
// ============================================================

class JoystickWidget : public QWidget
{
    Q_OBJECT
public:
    explicit JoystickWidget(QWidget *parent = nullptr);

    int sp() const { return m_sp; }   // -50~+50, 正=前进
    int st() const { return m_st; }   // -50~+50, 正=右转

signals:
    void joystickMoved(int sp, int st);   // 100ms 周期 (协议建议频率)
    void joystickCentered();              // 松开回中, 发一次 V:0:0

protected:
    void paintEvent(QPaintEvent *) override;
    void mousePressEvent(QMouseEvent *e) override;
    void mouseMoveEvent(QMouseEvent *e) override;
    void mouseReleaseEvent(QMouseEvent *e) override;
    void resizeEvent(QResizeEvent *) override;

private:
    void updateFromPos(const QPoint &pos);

    QPointF m_knobPos;      // 摇杆头中心 (widget 坐标)
    double m_radius = 80;   // 可动半径
    double m_knobR = 26;    // 摇杆头半径
    int m_sp = 0, m_st = 0;
    bool m_tracking = false;  // 按住中
    bool m_moved = false;     // 本次按住是否移动过
    QTimer *m_timer = nullptr;
};

#endif
