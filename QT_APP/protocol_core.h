#ifndef PROTOCOL_CORE_H
#define PROTOCOL_CORE_H

#include <QString>
#include <QByteArray>

// ============================================================
// 蓝牙通信协议 v2.1 — 多阶段支持
// ============================================================

// ── 阶段一：舵机校准 ──────────────────────────────────────────

struct ServoResponse {
    enum Event { None, SetPending, Ok, Cancel, Switched } event = None;
    int servoNum = 0;  // 舵机号 1~12
    int angle = -1;
};

QByteArray buildServoCmd(int angle);
QByteArray buildServoCmd(int servoNum, int angle);
QByteArray buildServoSwitchCmd(int servoNum);
ServoResponse parseServoLine(const QString &line);

// ── 阶段二：PID 调参 ──────────────────────────────────────────

struct PidParams {
    int kp = 0, ki = 0, kd = 0, targetRpm = 0;
    bool running = false;
};

struct WavePoint {
    int actualRpm = 0;   // 实际转速
    int targetRpm = 0;   // 目标转速
    int pidOut = 0;      // PID 输出 PWM (-100~100)
};

enum PidMsgType {
    PidNone,
    PidKp, PidKi, PidKd, PidTarget, // "Kp=5" / "Target=100 RPM"
    PidGo, PidStop,                  // "GO" / "STOP"
    PidWave                         // "58,100,45"
};

struct PidMessage {
    PidMsgType type = PidNone;
    PidParams params;
    WavePoint wave;
};

QByteArray buildPidCmd(const QString &cmd);  // "P+\r\n" / "G\r\n" etc.
PidMessage parsePidLine(const QString &line);

// ── 阶段三：步态+IMU (保留) ────────────────────────────────────

#define FRAME_HEADER 0xA5
#define FRAME_FOOTER 0x5A
QByteArray buildCommand(uint8_t cmd);

struct ImuData { float roll=0, pitch=0, yaw=0; };
struct MotorInfo { int encoderCount=0, rpm=0; };
struct EncoderData { MotorInfo motors[4]; };
struct TelemetryData { int type=0; ImuData imu; EncoderData encoder; qint64 ts=0; };
int parseTelemetryLine(const QString &line, TelemetryData &out);

#endif
