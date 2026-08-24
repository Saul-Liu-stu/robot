#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QLabel>
#include <QPushButton>
#include <QListWidget>
#include <QTextEdit>
#include <QStackedWidget>
#include <QSlider>
#include <QLineEdit>
#include "bluetoothclient.h"
#include "servowidget.h"
#include "pidwidget.h"
#include "motorwidget.h"
#include "joystickwidget.h"
#include "camerawidget.h"
#include "gimbaldial.h"

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

    // v6.14 断线自动重连 (意外断开才重连; 用户主动断开/从未连过不重连)
    QTimer *m_reconnectTimer = nullptr;
    int m_reconnectCount = 0;
    bool m_everConnected = false;
    bool m_userDisconnect = false;
    void startAutoReconnect();
    void stopAutoReconnect();

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

    // 左侧竖栏导航 (6 页: 连接/矫正/运动/遥控/云台/图传)
    QStackedWidget *m_stack;
    QPushButton *m_tabBtns[6];
    CameraWidget *m_cameraWidget = nullptr;   // v6.12 第6页 WiFi 图传
    void setPage(int idx);

    // v6.14 云台操作页 (G:水平:俯仰, 单轮盘双轴拖动 10Hz 实时发送, 回显 GM:pan,tilt)
    QLabel *m_gimbalStateLabel = nullptr;   // 状态卡 (回显驱动)
    GimbalDial *m_gimbalDial = nullptr;     // 单盘: 点哪云台去哪 (右=往右 下=往下)
    QLabel *m_gimbalCamLabel = nullptr;     // 左侧摄像头画面 (frameUpdated 信号驱动)
    QTimer *m_gimbalTimer = nullptr;        // 拖动中 5Hz 节流发送
    int m_gimbalPanDeg = 90, m_gimbalTiltDeg = 120;   // 两轴记录值 (上电正对前方 90/120)
    bool m_gimbalDragging = false;          // 拖动中: 回显不同步轮盘 (防抢游标)
    void sendGimbal(int pan, int tilt, bool quiet = false);   // 发 G:pan:tilt (quiet=拖动高频不打日志)

    // v6.12 WiFi 摄像头连接组 (连接页, 与蓝牙并排)
    QLineEdit *m_camIpEdit;
    QPushButton *m_btnCamConnect, *m_btnCamDisconnect, *m_btnCamWifi;
    QLabel *m_camStatusLabel;
    void onCamConnectClicked();
    void onCamDisconnectClicked();
    void onOpenWifiSettings();

    // v6.12 画面悬浮小窗 (遥控时悬浮看摄像头画面)
    MiniCamWindow *m_miniCam = nullptr;
    QTimer *m_miniCamTimer = nullptr;
    void onMiniCamToggled(bool on);

    // v6.14 站立自稳 (Z 标定 + L:1/L:0 开关; 坡度自适应已随固件删除)
    QLabel *m_slopeState;   // 标定状态 (Z 标定中/已标定)
    QLabel *m_lvState = nullptr;
    QPushButton *m_btnLv = nullptr;
    bool m_lvOn = false;

    // v6.12 V 模式1 (无倾斜补偿+自稳保持轮盘): 摇杆发送带 :1 后缀
    bool m_vMode1 = false;
    QPushButton *m_btnVMode = nullptr;
    QTextEdit *m_logView;
    int m_lineNum = 0;

public:
    static const QString C_BG, C_CARD, C_ACCENT, C_BLUE;
    static const QString C_GREEN, C_ORANGE, C_RED, C_TXT, C_DIM;
};

#endif
