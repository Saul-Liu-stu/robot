#ifndef MOTION_DETECTOR_H
#define MOTION_DETECTOR_H

#include <QObject>
#include <QImage>
#include <QRect>
#include <QPoint>
#include <QVector>

// 运动检测结果
struct MotionResult {
    bool detected = false;
    QRect region;      // 运动区域包围盒
    QPoint center;     // 运动区域中心
    double area = 0;   // 运动像素数
    double ratio = 0;  // 运动像素占画面比例
};

// 纯 Qt 运动检测 (v6.12 搜救场景): 帧差法
// |当前帧灰度 - 上一帧灰度| > 阈值 → 运动像素 → 连通域 → 最大运动块
// 废墟暗环境适用 (帧差对光照渐变不敏感)
class MotionDetector : public QObject {
    Q_OBJECT

public:
    explicit MotionDetector(QObject *parent = nullptr);

    // 处理一帧, 返回标记图 (运动区域红框), result 为运动信息
    QImage detect(const QImage &input, MotionResult &result);

    // 灵敏度: 帧差阈值 5~60, 默认 20 (越小越灵敏, 噪声也越多)
    void setSensitivity(int threshold);

    // 清空上一帧 (画面切换/流重连时调用)
    void reset();

private:
    QVector<uchar> m_prev;   // 上一帧灰度
    int m_w = 0, m_h = 0;
    int m_threshold = 20;
};

#endif // MOTION_DETECTOR_H
