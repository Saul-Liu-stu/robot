#ifndef BLUETOOTHCLIENT_H
#define BLUETOOTHCLIENT_H

#include <QObject>
#include <QThread>
#include <QBluetoothDeviceDiscoveryAgent>
#include <QBluetoothDeviceInfo>
#include <QBluetoothLocalDevice>
#include <QBluetoothPermission>
#include <QLocationPermission>
#include <QByteArray>
#include <atomic>
#include "protocol_core.h"

// ============================================================
// RFCOMM 后台读取线程
// ============================================================

class RfcommReader : public QThread
{
    Q_OBJECT
public:
    RfcommReader(void *inputStream, QObject *parent = nullptr);
    void stop();

signals:
    void dataReceived(const QByteArray &data);
    void readError(const QString &msg);

protected:
    void run() override;

private:
    void *m_inputStream;
    std::atomic<bool> m_running{true};
};

// ============================================================
// 蓝牙客户端
// ============================================================

class BluetoothClient : public QObject
{
    Q_OBJECT
public:
    enum State { Idle, Scanning, Connecting, Connected };
    Q_ENUM(State)

    explicit BluetoothClient(QObject *parent = nullptr);
    ~BluetoothClient();

    State state() const { return m_state; }
    bool isConnected() const { return m_state == Connected; }

public slots:
    void startScan();
    void stopScan();
    void connectToDevice(const QBluetoothDeviceInfo &device);
    void connectToAddress(const QString &address);
    void disconnect();
    void sendCommand(uint8_t cmd);
    void sendServoAngle(int angle);
    void sendServoAngle(int srv, int angle);
    void sendServoSwitch(int srv);
    void sendPidCmd(const QString &cmd);
    void sendRawText(const QString &text);  // 发纯文本 + \r\n, 用于 Y/N 等
    void sendMotorCmd(int motorNum, int speed, int dir);

signals:
    void stateChanged(State state);
    void deviceDiscovered(const QBluetoothDeviceInfo &device);
    void scanFinished();
    void telemetryReceived(const TelemetryData &data);
    void servoResponseReceived(const ServoResponse &rsp);
    void pidMessageReceived(const PidMessage &m);
    void motorResponseReceived(const MotorResponse &r);
    void errorOccurred(const QString &message);
    void rawLineReceived(const QString &line);

private slots:
    void onDeviceDiscovered(const QBluetoothDeviceInfo &info);
    void onScanFinished();
    void onScanError(QBluetoothDeviceDiscoveryAgent::Error error);
    void onReaderData(const QByteArray &data);
    void onReaderError(const QString &msg);

private:
    void setState(State s);
    void requestPermAndScan();
    void doStartScan();
    void doConnect(const QString &address);
    bool connectRfcomm(const QString &address);
    void processBuffer();
    // v6.14 统一写入口: 写前清残留异常, 写失败返回 false (防静默失效)
    bool sendBytes(const QByteArray &data);
    // v6.14 资源清理 (意外断开/重连前必须清旧 socket, 否则 RFCOMM 通道被占导致重连失败)
    void cleanup();

    QBluetoothDeviceDiscoveryAgent *m_disco = nullptr;
    QBluetoothLocalDevice *m_local = nullptr;
    RfcommReader *m_reader = nullptr;
    State m_state = Idle;

    void *m_javaSocket = nullptr;
    void *m_outputStream = nullptr;

    QByteArray m_buffer;
    int m_frameCount = 0;
    int m_sendFailStreak = 0;   // v6.14 连续写失败计数 (≥3 判定链路死亡, 主动断开)

    static const QString SPP_UUID_STR;
};

#endif
