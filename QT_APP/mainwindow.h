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
#include "joystickwidget.h"

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
    void onReconnectClicked();   // 一键重连上次设备

private:
    void setupUi();
    void appendLog(const QString &msg, const QString &color = QStringLiteral("#22c55e"));

    BluetoothClient *m_bt = nullptr;
    QLabel *m_statusIcon, *m_statusLabel;
    QLabel *m_devAddrLabel;             // 顶部状态条: 已连接设备地址
    QPushButton *m_btnScan, *m_btnConnect, *m_btnDisconnect;
    QPushButton *m_btnReconnect;        // 一键重连上次设备
    QString m_lastAddr;                 // QSettings 记忆的上次设备地址
    QListWidget *m_devList;
    ServoWidget *m_servoWidget;
    PidWidget *m_pidWidget;
    MotorWidget *m_motorWidget;
    QLabel *m_shiftLabel;   // 前倾修正当前值
    int m_footShift = 15;   // 固件最终默认 15mm

    // v6.9 遥控页 (横屏方案: 状态栏+摇杆+按钮区+姿态条)
    JoystickWidget *m_joy = nullptr;
    QLabel *m_joyValLabel = nullptr;  // 当前 V:sp:st 显示
    QLabel *m_encLabel = nullptr;     // 编码器转速显示
    QLabel *m_poseLabel = nullptr;    // 状态栏: 当前姿态 (回显驱动)
    QLabel *m_actLabel  = nullptr;    // 状态栏: 当前行为 (回显驱动)
    QLabel *m_dirLabel  = nullptr;    // 方向指示 (APP 自算)
    QLabel *m_joyImuLabel = nullptr;  // 遥控页 IMU 角度显示 (与页0共用数据源)
    QPushButton *m_gaitBtns[4] = { nullptr, nullptr, nullptr, nullptr };  // T/W/E顺/E逆
    void setPoseState(const QString &pose, const QString &color = QString());
    void setActState(const QString &act, const QString &color = QString());
    void setDirState(const QString &dir, const QString &color = QString());
    void setGaitEnabled(bool on);

    // 底部导航
    QStackedWidget *m_stack;
    QPushButton *m_tabBtns[4];
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
