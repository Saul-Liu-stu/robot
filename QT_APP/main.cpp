 #include <QApplication>
#include <QTimer>
#include "mainwindow.h"
#include "splashscreen.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("RobotBleApp"));
    app.setApplicationVersion(QStringLiteral("1.0"));

    QFont font = app.font();
    font.setFamily(QStringLiteral("Microsoft YaHei"));
    font.setPointSize(9);
    app.setFont(font);

    MainWindow w;
    w.showFullScreen();   // v6.17 进入即全屏 (隐藏状态栏/导航栏)

    // v6.17 启动画面: 主窗口先加载, 全屏 splash 覆盖 1.8s 后关闭
    SplashScreen splash;
    splash.showFullScreen();
    QTimer::singleShot(1800, &splash, &QWidget::close);

    return app.exec();
}
