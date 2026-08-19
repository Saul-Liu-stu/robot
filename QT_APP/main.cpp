 #include <QApplication>
#include "mainwindow.h"

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
    w.show();

    return app.exec();
}
