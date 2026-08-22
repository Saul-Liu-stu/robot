#ifndef COLOR_DETECTOR_H
#define COLOR_DETECTOR_H

#include <QObject>
#include <QImage>
#include <QColor>
#include <QPoint>
#include <QSize>

// 颜色名称枚举
enum class ColorName {
    RED,
    GREEN,
    BLUE,
    YELLOW,
    ORANGE,
    PURPLE,
    CUSTOM
};

// 检测结果
struct ColorDetectionResult {
    bool detected = false;
    QPoint center;        // 目标中心点坐标
    QSize size;           // 目标大小（宽高）
    double area = 0;      // 目标面积（像素数）
    QColor dominantColor; // 识别到的主色调
};

// 纯 Qt 颜色识别 (v6.12, 无 OpenCV 依赖)
// RGB→HSV 阈值 + 连通域标记 + 最大连通域质心/包围盒, QPainter 画标记
// 接口与《颜色识别使用说明》文档一致; 320x240 下每帧 <5ms
class ColorDetector : public QObject {
    Q_OBJECT

public:
    explicit ColorDetector(QObject *parent = nullptr);

    // 设置要检测的颜色
    void setTargetColor(ColorName color);
    void setCustomHSVRange(int hMin, int hMax, int sMin, int sMax, int vMin, int vMax);

    // 设置最小检测面积（过滤噪点）
    void setMinArea(int minArea);

    // 掩膜预览: 返回黑白 mask 图 (白=命中像素), 调 HSV 阈值调试用
    void setMaskPreview(bool on);

    // 对图片执行颜色识别，返回标记后的图片和检测结果
    QImage detect(const QImage &input, ColorDetectionResult &result);

private:
    int m_hMin, m_hMax;
    int m_sMin, m_sMax;
    int m_vMin, m_vMax;
    int m_minArea;
    ColorName m_currentColor;
    bool m_redTwoEnds = false;   // 红色在 HSV 两端 (0-10 与 170-179)
    bool m_maskPreview = false;  // 掩膜预览模式

    // 根据颜色名称设置HSV范围
    void setHSVForColor(ColorName color);
};

#endif // COLOR_DETECTOR_H
