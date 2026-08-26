 #include <QApplication>
#include <QTimer>
#include <QJniObject>
#if defined(Q_OS_ANDROID)
#include <QtCore/qnativeinterface.h>
#endif
#include "mainwindow.h"
#include "splashscreen.h"

// v6.24 沉浸式全屏 (sticky): 触摸不再把系统栏弹出来挤压窗口
// 系统栏只在边缘滑动手势时短暂出现, 松手自动隐藏, 窗口尺寸全程不变
static void applyImmersiveFullscreen()
{
#if defined(Q_OS_ANDROID)
    QJniObject activity = QNativeInterface::QAndroidApplication::context();
    if (!activity.isValid()) return;
    QJniObject window = activity.callObjectMethod(
        "getWindow", "()Landroid/view/Window;");
    if (!window.isValid()) return;

    const int sdk = QNativeInterface::QAndroidApplication::sdkVersion();
    if (sdk >= 30) {
        // API30+: WindowInsetsController
        window.callMethod<void>("setDecorFitsSystemWindows", "(Z)V", false);
        QJniObject ctrl = window.callObjectMethod(
            "getInsetsController", "()Landroid/view/WindowInsetsController;");
        if (ctrl.isValid()) {
            const jint types = QJniObject::callStaticMethod<jint>(
                "android/view/WindowInsets$Type", "systemBars", "()I");
            ctrl.callMethod<void>("hide", "(I)V", types);
            // BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE = 1: 滑动短暂显示, 自动隐藏
            ctrl.callMethod<void>("setSystemBarsBehavior", "(I)V", 1);
        }
    } else {
        // API<30: 传统 immersive sticky 标志
        QJniObject decor = window.callObjectMethod(
            "getDecorView", "()Landroid/view/View;");
        if (decor.isValid())
            decor.callMethod<void>("setSystemUiVisibility", "(I)V", 0x1706);
    }
#endif
}

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
    applyImmersiveFullscreen();   // v6.24 防触摸弹出系统栏挤压页面

    // v6.17 启动画面: 主窗口先加载, 全屏 splash 覆盖 1.8s 后关闭
    SplashScreen splash;
    splash.showFullScreen();
    QTimer::singleShot(1800, &splash, &QWidget::close);

    return app.exec();
}
