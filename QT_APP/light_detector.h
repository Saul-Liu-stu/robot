#ifndef LIGHT_DETECTOR_H
#define LIGHT_DETECTOR_H

#include <QObject>
#include <QImage>
#include <QPoint>
#include <QRect>

// 灯光/亮斑检测结果
struct LightResult {
    bool detected = false;
    QPoint center;      // 亮斑中心
    QRect region;       // 亮斑包围盒
    double area = 0;    // 亮斑像素数
    int maxLum = 0;     // 亮斑内最亮像素 (0~255)
    double avgLum = 0;  // 全画面平均亮度 (供状态显示)
};

// 纯 Qt 灯光/亮斑检测 (v6.15 搜救场景: 废墟暗环境找灯光/手电/火光)
// 算法与颜色识别同构: 灰度 → 自适应阈值 (画面均值+偏移) → 连通域 → 最大亮斑
// 暗环境 (均值低) 阈值下限 110 保证有效; 亮环境阈值自动抬高防全画面命中
class LightDetector : public QObject {
    Q_OBJECT

public:
    explicit LightDetector(QObject *parent = nullptr);

    // 灵敏度: 阈值 = max(110, 画面平均亮度 + offset), offset 30~120 默认 50
    // offset 越小越灵敏 (亮斑容易命中, 但弱光源/反光可能误报)
    void setOffset(int offset);

    // 最小亮斑面积 (过滤噪点)
    void setMinArea(int minArea);

    // 处理一帧, 返回标记图 (黄框+中心), result 为亮斑信息
    QImage detect(const QImage &input, LightResult &result);

private:
    int m_offset = 50;
    int m_minArea = 200;
};

#endif // LIGHT_DETECTOR_H
