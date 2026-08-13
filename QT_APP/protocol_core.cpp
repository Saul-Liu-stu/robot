#include "protocol_core.h"
#include <QStringList>

// ============================================================
//  阶段一：舵机校准
// ============================================================

QByteArray buildServoCmd(int angle) { return QString::number(angle).toUtf8() + "\r\n"; }
QByteArray buildServoCmd(int servoNum, int angle) { return QStringLiteral("%1:%2\r\n").arg(servoNum).arg(angle).toUtf8(); }
QByteArray buildServoSwitchCmd(int servoNum) { return QString::number(servoNum).toUtf8() + "\r\n"; }

ServoResponse parseServoLine(const QString &line)
{
    ServoResponse rsp;
    if (line.isEmpty()) return rsp;
    QString t = line.trimmed();

    // "SET:S3:150?" — 待确认
    if (t.startsWith(QStringLiteral("SET:")) && t.endsWith(QLatin1Char('?'))) {
        QString inner = t.mid(4, t.size() - 5);  // "S3:150"
        int colon = inner.indexOf(QLatin1Char(':'));
        if (colon > 1) {
            rsp.servoNum = inner.mid(1, colon - 1).toInt();
            rsp.angle = inner.mid(colon + 1).toInt();
        }
        rsp.event = ServoResponse::SetPending;
        return rsp;
    }

    // "OK:S3:150" — 已执行, or "OK:STAND" — 步态
    if (t.startsWith(QStringLiteral("OK:"))) {
        QString inner = t.mid(3);
        // "OK:STAND" "OK:WALK" etc are gait responses, not servo
        if (inner.compare(QStringLiteral("STAND"), Qt::CaseInsensitive) == 0) return rsp;
        int colon = inner.indexOf(QLatin1Char(':'));
        if (colon > 1 && inner.size() > 2 && inner[0] == u'S') {
            rsp.servoNum = inner.mid(1, colon - 1).toInt();
            rsp.angle = inner.mid(colon + 1).toInt();
            rsp.event = ServoResponse::Ok;
        }
        return rsp;
    }

    // "SW:S5" — 已切换舵机 (v3.0 去掉角度)
    if (t.startsWith(QStringLiteral("SW:"))) {
        QString inner = t.mid(3);  // "S5"
        rsp.servoNum = (inner.size() > 1 && inner[0] == u'S') ? inner.mid(1).toInt() : inner.toInt();
        rsp.event = ServoResponse::Switched;
        return rsp;
    }

    // "CANCEL"
    if (t.compare(QStringLiteral("CANCEL"), Qt::CaseInsensitive) == 0) {
        rsp.event = ServoResponse::Cancel; return rsp;
    }
    return rsp;
}

// ============================================================
//  阶段二：PID 调参
// ============================================================

QByteArray buildPidCmd(const QString &cmd)
{
    return cmd.toUtf8() + "\r\n";
}

PidMessage parsePidLine(const QString &line)
{
    PidMessage m;
    if (line.isEmpty()) return m;
    QString t = line.trimmed();

    // 波形: 首字符为数字或负号
    if (!t.isEmpty() && (t[0].isDigit() || t[0] == u'-')) {
        QStringList p = t.split(QLatin1Char(','));
        if (p.size() == 3) {
            m.type = PidWave;
            m.wave.actualRpm = p[0].toInt();
            m.wave.targetRpm = p[1].toInt();
            m.wave.pidOut   = p[2].toInt();
        }
        return m;
    }

    // 单参数应答 Kp=5 / Ki=1 / Kd=3 / Target=100 RPM
    auto kv = t.split(QLatin1Char('='));
    if (kv.size() == 2) {
        // 大小写不敏感匹配
        if (kv[0].compare(QStringLiteral("Kp"), Qt::CaseInsensitive) == 0) {
            m.type = PidKp; m.params.kp = kv[1].toInt();
        } else if (kv[0].compare(QStringLiteral("Ki"), Qt::CaseInsensitive) == 0) {
            m.type = PidKi; m.params.ki = kv[1].toInt();
        } else if (kv[0].compare(QStringLiteral("Kd"), Qt::CaseInsensitive) == 0) {
            m.type = PidKd; m.params.kd = kv[1].toInt();
        } else if (kv[0].compare(QStringLiteral("Target"), Qt::CaseInsensitive) == 0) {
            // "Target=100 RPM" → 提取开头数字，兼容 "100RPM" "100"
            m.type = PidTarget;
            QString numStr;
            for (int i = 0; i < kv[1].size(); i++) {
                QChar c = kv[1][i];
                if (c.isDigit() || c == u'-') numStr.append(c); else break;
            }
            m.params.targetRpm = numStr.isEmpty() ? 0 : numStr.toInt();
        }
        return m;
    }

    // GO / STOP
    if (t.compare(QStringLiteral("GO"), Qt::CaseInsensitive) == 0)
        { m.type = PidGo; m.params.running = true; return m; }
    if (t.compare(QStringLiteral("STOP"), Qt::CaseInsensitive) == 0)
        { m.type = PidStop; m.params.running = false; return m; }

    return m;
}

// ============================================================
//  阶段三：步态 + IMU (保留)
// ============================================================

QByteArray buildCommand(uint8_t cmd)
{
    QByteArray f; f.append((char)FRAME_HEADER); f.append((char)cmd);
    f.append((char)cmd); f.append((char)FRAME_FOOTER); return f;
}

int parseTelemetryLine(const QString &line, TelemetryData &out)
{
    if (line.isEmpty()) return 0;
    if (line.startsWith(QLatin1Char('R'))) {
        QStringList p = line.split(QLatin1Char(' '), Qt::SkipEmptyParts);
        if (p.size() >= 3) {
            out.imu.roll=p[0].section(QLatin1Char(':'),1).toFloat();
            out.imu.pitch=p[1].section(QLatin1Char(':'),1).toFloat();
            out.imu.yaw=p[2].section(QLatin1Char(':'),1).toFloat();
            out.type=1; return 1;
        }
    }
    if (line.startsWith(QLatin1Char('A'))) {
        QStringList p=line.split(QLatin1Char(' '),Qt::SkipEmptyParts);
        if(p.size()>=4){ for(int i=0;i<4;i++){
            QStringList v=p[i].section(QLatin1Char(':'),1).split(QLatin1Char('/'));
            if(v.size()>=2){out.encoder.motors[i].encoderCount=v[0].toInt();
            out.encoder.motors[i].rpm=v[1].toInt();} } out.type=2; return 2; }
    }
    return 0;
}
