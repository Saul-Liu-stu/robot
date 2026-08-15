#ifndef MOTORWIDGET_H
#define MOTORWIDGET_H

#include <QWidget>
#include <QSlider>
#include <QLabel>
#include <QPushButton>
#include "protocol_core.h"

// 电机控制面板 — 4 电机 A/B/C/D, 速度+方向
class MotorWidget : public QWidget
{
    Q_OBJECT
public:
    explicit MotorWidget(QWidget *parent = nullptr);

    void onMotorResponse(const MotorResponse &r);
    void reset();

signals:
    void motorCmdRequested(int motorNum, int speed, int dir);

private:
    struct MotorRow {
        QLabel *name;
        QLabel *speedVal;
        QSlider *slider;
        QPushButton *btnDir;
        QPushButton *btnSend;
        QPushButton *btnStop;
        int dir = 1;
    };

    MotorRow m_rows[4];
    void setupUi();
    void updateRow(int i);
};

#endif
