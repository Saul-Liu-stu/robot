#include "light_detector.h"
#include <QPainter>
#include <QPen>
#include <QVector>

LightDetector::LightDetector(QObject *parent) : QObject(parent) {}

void LightDetector::setOffset(int offset) { m_offset = offset; }

void LightDetector::setMinArea(int minArea) { m_minArea = minArea; }

QImage LightDetector::detect(const QImage &input, LightResult &result)
{
    result.detected = false;
    if (input.isNull()) return input;

    const QImage img = input.convertToFormat(QImage::Format_RGB32);
    const int w = img.width(), h = img.height();

    // 灰度 + 全画面平均亮度
    QVector<uchar> gray(w * h);
    qint64 sum = 0;
    for (int y = 0; y < h; y++) {
        const QRgb *line = reinterpret_cast<const QRgb *>(img.constScanLine(y));
        for (int x = 0; x < w; x++) {
            const uchar lum = (uchar)((qRed(line[x]) * 299 + qGreen(line[x]) * 587 + qBlue(line[x]) * 114) / 1000);
            gray[y * w + x] = lum;
            sum += lum;
        }
    }
    result.avgLum = double(sum) / (w * h);

    // 自适应阈值: 画面均值+偏移; 下限 110 (暗环境手电/灯光仍能命中), 亮环境自动抬高防误报
    const int thr = qMax(110, int(result.avgLum + m_offset));

    // 亮度掩膜 → 连通域 (4邻域 BFS) → 最大亮斑 (与颜色识别同构)
    QVector<uchar> mask(w * h, 0);
    for (int i = 0; i < w * h; i++)
        if (gray[i] > thr) mask[i] = 1;

    QVector<uchar> visited(w * h, 0);
    QVector<int> stack;
    int bestArea = 0, bx = 0, by = 0, bw = 0, bh = 0;
    qint64 cx = 0, cy = 0;
    int maxLum = 0;
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
                if (x > 0 && mask[idx-1] && !visited[idx-1]) { visited[idx-1]=1; stack.append(idx-1); }
                if (x < w-1 && mask[idx+1] && !visited[idx+1]) { visited[idx+1]=1; stack.append(idx+1); }
                if (y > 0 && mask[idx-w] && !visited[idx-w]) { visited[idx-w]=1; stack.append(idx-w); }
                if (y < h-1 && mask[idx+w] && !visited[idx+w]) { visited[idx+w]=1; stack.append(idx+w); }
            }
            if (area > bestArea) {
                bestArea = area;
                bx = xmin; by = ymin; bw = xmax - xmin + 1; bh = ymax - ymin + 1;
                cx = sx; cy = sy;
            }
        }
    }

    QImage out = img;
    if (bestArea >= m_minArea) {
        result.detected = true;
        result.area = bestArea;
        result.center = QPoint(cx / bestArea, cy / bestArea);
        result.region = QRect(bx, by, bw, bh);
        // 亮斑内最亮值
        for (int y = by; y <= by + bh - 1 && y < h; y++)
            for (int x = bx; x <= bx + bw - 1 && x < w; x++)
                maxLum = qMax(maxLum, (int)gray[y * w + x]);
        result.maxLum = maxLum;

        // 标记: 黄框 + 中心点 (与颜色识别绿框区分)
        QPainter p(&out);
        p.setRenderHint(QPainter::Antialiasing, true);
        p.setPen(QPen(QColor(QStringLiteral("#f59e0b")), 3));
        p.setBrush(Qt::NoBrush);
        p.drawRect(result.region);
        p.setBrush(QColor(QStringLiteral("#f59e0b")));
        p.setPen(Qt::NoPen);
        p.drawEllipse(result.center, 5, 5);
        p.end();
    }
    return out;
}
