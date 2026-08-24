#ifndef CAMERAWIDGET_H
#define CAMERAWIDGET_H

#include <QWidget>
#include <QByteArray>
#include <QLabel>
#include <QPushButton>
#include <QSlider>
#include <QPixmap>
#include <QResizeEvent>
#include <QMouseEvent>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QTimer>
#include "color_detector.h"
#include "motion_detector.h"
#include "light_detector.h"

// 画面悬浮小窗 (v6.12): 悬浮在 APP 内所有页面之上
// 拖动定位 / 双击切换大(340x260)小(180x140) / 右上角×关闭
class MiniCamWindow : public QWidget
{
    Q_OBJECT
public:
    explicit MiniCamWindow(QWidget *parent);
    void setFrame(const QPixmap &pm);

protected:
    void mousePressEvent(QMouseEvent *e) override;
    void mouseMoveEvent(QMouseEvent *e) override;
    void mouseReleaseEvent(QMouseEvent *e) override;
    void mouseDoubleClickEvent(QMouseEvent *e) override;
    void resizeEvent(QResizeEvent *e) override;

private:
    QLabel *m_view;
    QPushButton *m_close;
    QPoint m_dragOffset;
    bool m_dragging = false;
    bool m_big = false;
    void applySize();
};

// WiFi 图传控件 (v6.12): 视频显示 + MJPEG 流 + 画面参数 + 拍照
// 连接 UI (IP/连接/断开/切WiFi) 在 mainwindow 连接页, 通过 startStream 驱动
// API: GET /stream (MJPEG, 端口81)  /capture (单帧)  /control?var=&val=
class CameraWidget : public QWidget
{
    Q_OBJECT
public:
    explicit CameraWidget(QWidget *parent = nullptr);
    ~CameraWidget() override;

    void startStream(const QString &ip);   // 连接 http://ip:81/stream
    void stopStream();
    bool isStreaming() const { return m_reply != nullptr; }
    QString ip() const { return m_ip; }
    QPixmap currentFrame() const { return m_frame; }   // 悬浮窗拉取最新帧

signals:
    void statusChanged(const QString &text);   // 连接页状态标签用
    void miniWindowToggled(bool on);           // 悬浮小窗开关 (mainwindow 管理悬浮窗)
    void frameUpdated(const QPixmap &pm);      // v6.14 每解析出一帧发信号 (云台页左侧画面用)

protected:
    void resizeEvent(QResizeEvent *e) override;   // 等比缩放保持 (防畸变)

private slots:
    void onStreamData();
    void onStreamFinished();
    void onCaptureClicked();

private:
    void setupUi();
    void parseMjpegFrame();
    void rescalePixmap();   // 按当前 label 尺寸等比缩放当前帧
    void sendControl(const QString &var, int val);

    QNetworkAccessManager *m_mgr;
    QNetworkReply *m_reply = nullptr;
    QByteArray m_buffer;
    QString m_ip;
    QPixmap m_frame;        // 最近一帧原图 (resize 时重新缩放)

    QLabel *m_videoLabel;
    QSlider *m_qualitySlider;
    QLabel *m_qualityVal;
    QPushButton *m_btnQvga, *m_btnVga;
    QPushButton *m_btnHmirror, *m_btnVflip;
    QPushButton *m_btnCapture;
    QPushButton *m_btnMini;
    int m_framesize = 6;    // 默认 VGA 640x480
    int m_hmirror = 0, m_vflip = 0;
    bool m_miniOn = false;

    // v6.12 颜色识别 (纯 Qt, 无 OpenCV)
    ColorDetector *m_detector;
    bool m_detectOn = false;
    QPushButton *m_btnDetect;
    QPushButton *m_colorBtns[6];
    QSlider *m_minAreaSlider;
    QLabel *m_minAreaVal;
    QLabel *m_detectResultLabel;
    QString m_lastResultText;   // 结果文本变化检测 (避免每帧 setText)

    // v6.12 定时拍照 (搜救记录: 每20秒自动存相册)
    QTimer *m_autoCapTimer = nullptr;
    bool m_autoCapOn = false;
    QPushButton *m_btnAutoCap = nullptr;
    QLabel *m_autoCapLabel = nullptr;
    int m_autoCapCount = 0;

    // v6.12 运动检测 (帧差法, 搜救场景: 发现移动目标)
    MotionDetector *m_motionDetector;
    bool m_motionOn = false;
    QPushButton *m_btnMotion;
    QSlider *m_sensSlider;
    QLabel *m_sensVal;
    QLabel *m_motionResultLabel;
    QString m_lastMotionText;

    // v6.15 灯光/亮斑检测 (搜救场景: 废墟暗环境找灯光/手电/火光)
    LightDetector *m_lightDetector;
    bool m_lightOn = false;
    QPushButton *m_btnLight;
    QSlider *m_lightSensSlider;
    QLabel *m_lightSensVal;
    QLabel *m_lightResultLabel;
    QString m_lastLightText;

};

#endif // CAMERAWIDGET_H
