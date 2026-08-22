// FaceDetector 桌面验证程序: 加载模型 + 检测 lena.jpg
// 构建: cmake -G "MinGW Makefiles" -DCMAKE_PREFIX_PATH=D:/QT/QT/6.11.1/mingw_64 ..
#include <QCoreApplication>
#include <QImage>
#include <QElapsedTimer>
#include <QDebug>
#include <QFile>
#include <QTextStream>
#include <cstdio>
#include "face_detector.h"

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    // Windows GUI 程序: qDebug 重定向到文件
    freopen("D:/stm322025program/robot_four_leg/tools/facetest/stdout.txt", "w", stdout);
    freopen("D:/stm322025program/robot_four_leg/tools/facetest/stderr.txt", "w", stderr);
    setvbuf(stdout, nullptr, _IONBF, 0);
    setvbuf(stderr, nullptr, _IONBF, 0);

    FaceDetector det;
    const QString xml = QStringLiteral("D:/stm322025program/robot_four_leg/QT_APP/haarcascade_frontalface_default.xml");
    if (!det.init(xml)) { qWarning() << "模型加载失败!"; return 1; }
    qDebug() << "模型加载成功";

    QImage img(QStringLiteral("D:/stm322025program/robot_four_leg/tools/facetest/lena.jpg"));
    if (img.isNull()) { qWarning() << "图片加载失败!"; return 1; }
    qDebug() << "图片大小:" << img.size();

    QElapsedTimer t; t.start();
    FaceDetectionResult r = det.detect(img);
    const qint64 ms = t.elapsed();

    // Windows GUI 程序 qDebug 不进控制台, 结果写文件
    QFile out(QStringLiteral("D:/stm322025program/robot_four_leg/tools/facetest/result.txt"));
    out.open(QIODevice::WriteOnly | QIODevice::Text);
    QTextStream s(&out);
    s << "耗时: " << ms << " ms\n";
    s << "检测到 " << r.faceCount << " 张人脸\n";
    for (const FaceResult &f : r.faces)
        s << "  face: x=" << f.x << " y=" << f.y << " w=" << f.width << " h=" << f.height
          << " distanceCm=" << f.distanceCm << "\n";
    out.close();
    return r.detected ? 0 : 2;
}
