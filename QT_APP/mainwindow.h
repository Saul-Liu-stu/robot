#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QLabel>
#include <QPushButton>
#include <QListWidget>
#include <QTextEdit>
#include <QStackedWidget>
#include <QSlider>
#include "bluetoothclient.h"
#include "servowidget.h"
#include "pidwidget.h"
#include "motorwidget.h"

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void onBtStateChanged(BluetoothClient::State s);
    void onDeviceDiscovered(const QBluetoothDeviceInfo &device);
    void onServoResponse(const ServoResponse &rsp);
    void onPidMessage(const PidMessage &m);
    void onMotorResponse(const MotorResponse &r);
    void onMotorCmdRequested(int motorNum, int speed, int dir);
    void onTelemetry(const TelemetryData &d);
    void onServoAngleRequested(int srv, int angle);
    void onPidCommandRequested(const QString &cmd);
    void onScanClicked();
    void onConnectClicked();
    void onDisconnectClicked();

private:
    void setupUi();
    void appendLog(const QString &msg, const QString &color = QStringLiteral("#22c55e"));

    BluetoothClient *m_bt = nullptr;
    QLabel *m_statusIcon, *m_statusLabel;
    QPushButton *m_btnScan, *m_btnConnect, *m_btnDisconnect;
    QListWidget *m_devList;
    ServoWidget *m_servoWidget;
    PidWidget *m_pidWidget;
    MotorWidget *m_motorWidget;
    QLabel *m_shiftLabel;   // 前倾修正当前值
    int m_footShift = 15;   // 固件最终默认 15mm
    QLabel *m_imuLabel;     // IMU 姿态 R/P/Y 显示

    // 底部导航
    QStackedWidget *m_stack;
    QPushButton *m_tabBtns[3];
    void setPage(int idx);

    // 坡度自适应
    QLabel *m_slopeState;
    struct SlopeRow { QLabel *name; QSlider *slider; QLabel *val; };
    SlopeRow m_slopeRows[5];
    void sendSlopeParam(int idx);
    QTextEdit *m_logView;
    int m_lineNum = 0;

public:
    static const QString C_BG, C_CARD, C_ACCENT, C_BLUE;
    static const QString C_GREEN, C_ORANGE, C_RED, C_TXT, C_DIM;
};

#endif
