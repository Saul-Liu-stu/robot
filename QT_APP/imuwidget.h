#ifndef IMUWIDGET_H
#define IMUWIDGET_H

#include <QWidget>
#include <QLabel>
#include "protocol_core.h"

// ============================================================
// IMU 姿态角显示面板
// Roll / Pitch / Yaw 三行 + 可视化进度条
// ============================================================

class ImuWidget : public QWidget
{
    Q_OBJECT
public:
    explicit ImuWidget(QWidget *parent = nullptr);

    void updateData(const ImuData &imu);
    void updateEncoder(const EncoderData &enc);

    void reset();

private:
    void setupUi();

    struct AxisWidget {
        QLabel *label;
        QLabel *value;
        QWidget *barBg;
        QWidget *barFill;
    };

    AxisWidget m_roll, m_pitch, m_yaw;
    QLabel *m_encoderTitle;
    QLabel *m_encoderVals[4];
    int m_frameCount = 0;

    void setAxisVal(const AxisWidget &ax, float val, const QString &unit);
    void updateEncoderUi();

    EncoderData m_lastEnc;

public:
    static const QString C_BG, C_CARD, C_TXT, C_DIM;
    static const QString C_GREEN, C_YELLOW, C_RED, C_BLUE;
};

#endif
