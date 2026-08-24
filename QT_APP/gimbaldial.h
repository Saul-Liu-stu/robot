#ifndef GIMBALDIAL_H
#define GIMBALDIAL_H

#include <QWidget>

// 云台轮盘控件 (v6.14): 单盘双轴 — 手指点哪云台去哪
// 触摸点相对圆心: 右拨=水平往右(pan减小) 左拨=水平往左(pan增大)
//                 下拨=俯仰往下(tilt减小) 上拨=俯仰往上(tilt增大)
// 松手停住不回中; 蓝色游标点指示当前两轴姿态
// pan 0~180 (90=正前), tilt 75~180 (120=正前, 75=机械下限)
class GimbalDial : public QWidget
{
    Q_OBJECT
public:
    explicit GimbalDial(QWidget *parent = nullptr);

    void setValue(int pan, int tilt);       // 回显同步 (不发射信号)
    int panValue() const { return m_pan; }
    int tiltValue() const { return m_tilt; }

signals:
    void dragStarted();
    void valuesChanged(int pan, int tilt);  // 拖动中连续发射
    void dragEnded(int pan, int tilt);      // 松手最终值

protected:
    void paintEvent(QPaintEvent *) override;
    void mousePressEvent(QMouseEvent *e) override;
    void mouseMoveEvent(QMouseEvent *e) override;
    void mouseReleaseEvent(QMouseEvent *e) override;

private:
    int m_pan = 90, m_tilt = 120;
    bool m_dragging = false;

    double trackR() const;                  // 有效轨道半径
    QPointF posForValues() const;           // 两轴值→游标点 (绘图)
    void updateFromPos(const QPointF &p);   // 位置→两轴值 (与 posForValues 互逆)
};

#endif // GIMBALDIAL_H
