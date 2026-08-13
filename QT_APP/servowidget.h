#ifndef SERVOWIDGET_H
#define SERVOWIDGET_H

#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <QSlider>
#include "protocol_core.h"

class ServoWidget : public QWidget
{
    Q_OBJECT
public:
    explicit ServoWidget(QWidget *parent = nullptr);

    void onSetPending(int srv, int angle);
    void onOk(int srv, int angle);
    void onCancel();
    void onSwitched(int srv, int angle = -1);
    void reset();

signals:
    void angleRequested(int angle);           // 当前舵机设角度
    void servoAngleRequested(int srv, int angle); // 指定舵机设角度
    void confirmYes();
    void confirmNo();
    void switchServo(int srv);                // 切换舵机

private slots:
    void onSetClicked();
    void onSliderChanged(int val);
    void onQuickAdjust(int delta);
    void onSlotClicked(int index);
    void onExportCsv();
    void onClearAll();

private:
    void setupUi();
    void updateAngleDisplay(int angle);
    void updateServoHighlight();

    QLabel *m_angleLabel;
    QLabel *m_statusLabel;
    QLabel *m_servoLabel;
    QSlider *m_slider;
    QPushButton *m_btnY, *m_btnN;
    QPushButton *m_btnSet;
    QPushButton *m_servoBtns[12];  // 舵机选择按钮
    QPushButton *m_slotBtns[12];   // 记录槽位
    int m_savedAngles[12];

    int m_currentServo = 1;
    int m_currentAngle = 90;
    int m_pendingSrv = 0;
    int m_pendingAngle = -1;
};

#endif
