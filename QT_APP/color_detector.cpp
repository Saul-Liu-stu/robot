#include "color_detector.h"
#include <QPainter>
#include <QPen>
#include <QVector>
#include <QDebug>
#include <QtMath>
#include <algorithm>

// HSV 说明 (与 OpenCV 约定一致): H 0-179, S 0-255, V 0-255
// 整数 RGB→HSV (H = 0..359, 除以2 = 0..179)
static inline void rgbToHsv(int r, int g, int b, int &h, int &s, int &v)
{
    const int max = qMax(r, qMax(g, b));
    const int min = qMin(r, qMin(g, b));
    v = max;
    const int d = max - min;
    if (d == 0) { h = 0; s = 0; return; }
    s = (255 * d) / max;
    int hh;
    if (max == r)      hh = 60 * (g - b) / d;
    else if (max == g) hh = 60 * (b - r) / d + 120;
    else               hh = 60 * (r - g) / d + 240;
    if (hh < 0) hh += 360;
    h = hh / 2;   // 0..179
}

static inline bool inRange(int h, int s, int v, int hMin, int hMax,
                           int sMin, int sMax, int vMin, int vMax)
{
    return h >= hMin && h <= hMax && s >= sMin && s <= sMax && v >= vMin && v <= vMax;
}

ColorDetector::ColorDetector(QObject *parent)
    : QObject(parent),
      m_hMin(0), m_hMax(10),
      m_sMin(100), m_sMax(255),
      m_vMin(100), m_vMax(255),
      m_minArea(500),
      m_currentColor(ColorName::RED)
{
}

void ColorDetector::setTargetColor(ColorName color) {
    m_currentColor = color;
    setHSVForColor(color);
}

void ColorDetector::setCustomHSVRange(int hMin, int hMax, int sMin, int sMax, int vMin, int vMax) {
    m_hMin = hMin;
    m_hMax = hMax;
    m_sMin = sMin;
    m_sMax = sMax;
    m_vMin = vMin;
    m_vMax = vMax;
    m_currentColor = ColorName::CUSTOM;
    m_redTwoEnds = false;
}

void ColorDetector::setMinArea(int minArea) {
    m_minArea = minArea;
}

void ColorDetector::setMaskPreview(bool on) {
    m_maskPreview = on;
}

void ColorDetector::setHSVForColor(ColorName color) {
    m_redTwoEnds = false;
    switch (color) {
    case ColorName::RED:
        // 红色在 HSV 圆环两端, 需两段合并 (0-10 与 170-179)
        // S 下限放宽到 80: 室内光照下红色饱和度常低于 100
        m_hMin = 0;   m_hMax = 10;
        m_sMin = 80;  m_sMax = 255;
        m_vMin = 70;  m_vMax = 255;
        m_redTwoEnds = true;
        break;
    case ColorName::GREEN:
        m_hMin = 35;  m_hMax = 77;
        m_sMin = 50;  m_sMax = 255;
        m_vMin = 50;  m_vMax = 255;
        break;
    case ColorName::BLUE:
        m_hMin = 100; m_hMax = 130;
        m_sMin = 60;  m_sMax = 255;
        m_vMin = 50;  m_vMax = 255;
        break;
    case ColorName::YELLOW:
        m_hMin = 20;  m_hMax = 35;
        m_sMin = 80;  m_sMax = 255;
        m_vMin = 80;  m_vMax = 255;
        break;
    case ColorName::ORANGE:
        m_hMin = 10;  m_hMax = 25;
        m_sMin = 100; m_sMax = 255;
        m_vMin = 80;  m_vMax = 255;
        break;
    case ColorName::PURPLE:
        m_hMin = 130; m_hMax = 160;
        m_sMin = 60;  m_sMax = 255;
        m_vMin = 50;  m_vMax = 255;
        break;
    default:
        break;
    }
}

QImage ColorDetector::detect(const QImage &input, ColorDetectionResult &result)
{
    result.detected = false;
    if (input.isNull()) return input;

    // 统一 RGB32, 逐像素建 mask
    const QImage img = input.convertToFormat(QImage::Format_RGB32);
    const int w = img.width(), h = img.height();
    QVector<uchar> mask(w * h, 0);
    for (int y = 0; y < h; y++) {
        const QRgb *line = reinterpret_cast<const QRgb *>(img.constScanLine(y));
        for (int x = 0; x < w; x++) {
            const int r = qRed(line[x]), g = qGreen(line[x]), b = qBlue(line[x]);
            int hv, sv, vv;
            rgbToHsv(r, g, b, hv, sv, vv);
            bool hit = inRange(hv, sv, vv, m_hMin, m_hMax, m_sMin, m_sMax, m_vMin, m_vMax);
            if (m_redTwoEnds && !hit)   // 红色第二段 170-179
                hit = inRange(hv, sv, vv, 170, 179, m_sMin, m_sMax, m_vMin, m_vMax);
            if (hit) mask[y * w + x] = 1;
        }
    }

    // 掩膜预览: 返回黑白 mask 图 (白=命中), 调阈值调试用
    if (m_maskPreview) {
        QImage maskImg(w, h, QImage::Format_RGB32);
        for (int y = 0; y < h; y++) {
            QRgb *line = reinterpret_cast<QRgb *>(maskImg.scanLine(y));
            for (int x = 0; x < w; x++)
                line[x] = mask[y * w + x] ? qRgb(255, 255, 255) : qRgb(0, 0, 0);
        }
        return maskImg;
    }

    // 连通域标记 (4邻域 BFS): 找面积 > minArea 的最大连通域
    QVector<uchar> visited(w * h, 0);
    int bestArea = 0, bx = 0, by = 0, bw = 0, bh = 0;
    qint64 cx = 0, cy = 0;   // 质心累计 (最大块)
    QVector<int> stack;
    for (int y0 = 0; y0 < h; y0++) {
        for (int x0 = 0; x0 < w; x0++) {
            const int idx0 = y0 * w + x0;
            if (!mask[idx0] || visited[idx0]) continue;
            stack.clear();
            stack.append(idx0);
            visited[idx0] = 1;
            int area = 0;
            int xmin = w, xmax = 0, ymin = h, ymax = 0;
            qint64 sx = 0, sy = 0;
            while (!stack.isEmpty()) {
                const int idx = stack.takeLast();
                const int x = idx % w, y = idx / w;
                area++;
                sx += x; sy += y;
                xmin = qMin(xmin, x); xmax = qMax(xmax, x);
                ymin = qMin(ymin, y); ymax = qMax(ymax, y);
                // 4邻域
                if (x > 0 && mask[idx-1] && !visited[idx-1]) { visited[idx-1]=1; stack.append(idx-1); }
                if (x < w-1 && mask[idx+1] && !visited[idx+1]) { visited[idx+1]=1; stack.append(idx+1); }
                if (y > 0 && mask[idx-w] && !visited[idx-w]) { visited[idx-w]=1; stack.append(idx-w); }
                if (y < h-1 && mask[idx+w] && !visited[idx+w]) { visited[idx+w]=1; stack.append(idx+w); }
            }
            if (area > m_minArea && area > bestArea) {
                bestArea = area;
                bx = xmin; by = ymin; bw = xmax - xmin + 1; bh = ymax - ymin + 1;
                cx = sx; cy = sy;
            }
        }
    }

    QImage out = img;
    if (bestArea > 0) {
        result.detected = true;
        result.area = bestArea;
        result.center = QPoint(cx / bestArea, cy / bestArea);
        result.size = QSize(bw, bh);
        result.dominantColor = img.pixel(result.center.x(), result.center.y());

        // 标记: 绿框 + 红中心点 + 十字线 + 文字
        QPainter p(&out);
        p.setRenderHint(QPainter::Antialiasing, true);
        QPen pen(QColor(QStringLiteral("#22c55e")), 3);
        p.setPen(pen);
        p.drawRect(bx, by, bw, bh);
        p.setPen(QPen(QColor(QStringLiteral("#ef4444")), 2));
        p.drawLine(result.center.x() - 15, result.center.y(),
                   result.center.x() + 15, result.center.y());
        p.drawLine(result.center.x(), result.center.y() - 15,
                   result.center.x(), result.center.y() + 15);
        p.setBrush(QColor(QStringLiteral("#ef4444")));
        p.setPen(Qt::NoPen);
        p.drawEllipse(result.center, 5, 5);
        p.setPen(QPen(QColor(QStringLiteral("#22c55e")), 2));
        QFont f = p.font(); f.setPixelSize(14); f.setBold(true); p.setFont(f);
        const QString label = QStringLiteral("Target (%1, %2)").arg(result.center.x()).arg(result.center.y());
        p.drawText(QPointF(bx, qMax(0, by - 6)), label);
        p.end();
    }
    return out;
}
