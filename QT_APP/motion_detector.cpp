#include "motion_detector.h"
#include <QPainter>
#include <QPen>
#include <QDebug>

MotionDetector::MotionDetector(QObject *parent) : QObject(parent) {}

void MotionDetector::setSensitivity(int t) { m_threshold = t; }

void MotionDetector::reset() { m_prev.clear(); m_w = 0; m_h = 0; }

QImage MotionDetector::detect(const QImage &input, MotionResult &result)
{
    result.detected = false;
    if (input.isNull()) return input;

    const QImage img = input.convertToFormat(QImage::Format_RGB32);
    const int w = img.width(), h = img.height();

    // 当前帧灰度
    QVector<uchar> gray(w * h);
    for (int y = 0; y < h; y++) {
        const QRgb *line = reinterpret_cast<const QRgb *>(img.constScanLine(y));
        for (int x = 0; x < w; x++)
            gray[y * w + x] = (uchar)((qRed(line[x]) * 299 + qGreen(line[x]) * 587 + qBlue(line[x]) * 114) / 1000);
    }

    QImage out = img;
    if (m_prev.size() != w * h) {
        // 首帧: 只记参考帧, 不检测
        m_prev = gray;
        m_w = w; m_h = h;
        return out;
    }

    // 帧差 → 运动掩膜
    QVector<uchar> mask(w * h, 0);
    for (int i = 0; i < w * h; i++) {
        const int d = qAbs(int(gray[i]) - int(m_prev[i]));
        if (d > m_threshold) mask[i] = 1;
    }
    m_prev = gray;   // 更新参考帧

    // 连通域 (4邻域 BFS): 找最大运动块
    QVector<uchar> visited(w * h, 0);
    QVector<int> stack;
    int bestArea = 0, bx = 0, by = 0, bw = 0, bh = 0;
    qint64 cx = 0, cy = 0;
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

    // 最小运动块过滤: 小于画面 0.3% 视为噪声
    const int minPixels = int(w * h * 0.003);
    if (bestArea >= minPixels) {
        result.detected = true;
        result.area = bestArea;
        result.ratio = double(bestArea) / (w * h);
        result.center = QPoint(cx / bestArea, cy / bestArea);
        result.region = QRect(bx, by, bw, bh);

        // 标记: 红框 + 中心点
        QPainter p(&out);
        p.setRenderHint(QPainter::Antialiasing, true);
        p.setPen(QPen(QColor(QStringLiteral("#ef4444")), 3));
        p.setBrush(Qt::NoBrush);
        p.drawRect(result.region);
        p.setBrush(QColor(QStringLiteral("#ef4444")));
        p.setPen(Qt::NoPen);
        p.drawEllipse(result.center, 6, 6);
        p.end();
    }
    return out;
}
