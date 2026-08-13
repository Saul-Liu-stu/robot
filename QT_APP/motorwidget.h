#ifndef MOTORWIDGET_H
#define MOTORWIDGET_H

#include <QWidget>
#include <QLabel>
#include <QProgressBar>
#include "protocol_core.h"

// ============================================================
// 单电机数据显示面板
// 用于展示一个电机 (A/B/C/D) 的编码器计数和转速
// ============================================================

class MotorWidget : public QWidget
{
    Q_OBJECT
public:
    explicit MotorWidget(int motorIndex, QWidget *parent = nullptr);

    // 更新显示数据
    void updateData(const MotorInfo &info);

    // 重置到默认状态
    void reset();

private:
    void setupUi();

    int m_index;
    QLabel *m_titleLabel;
    QLabel *m_encoderLabel;
    QLabel *m_rpmLabel;
    QProgressBar *m_rpmBar;

    // 配色常量
    static const QString C_BG;
    static const QString C_CARD;
    static const QString C_TXT;
    static const QString C_DIM;
    static const QString C_ACCENT;
    static const QString C_GREEN;
    static const QString C_YELLOW;
    static const QString C_RED;
    static const QString MOTOR_NAMES[4];
};

#endif // MOTORWIDGET_H
