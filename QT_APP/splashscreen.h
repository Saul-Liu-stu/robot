#ifndef SPLASHSCREEN_H
#define SPLASHSCREEN_H

#include <QWidget>

// 启动画面 (v6.17): 石墨深色底 + 中央徽章 + 项目名, 1.8s 后自动关闭
// 自绘无图片资源: 橙色圆环徽章 + 🐕 emoji + 标题
class SplashScreen : public QWidget
{
    Q_OBJECT
public:
    explicit SplashScreen(QWidget *parent = nullptr);

protected:
    void paintEvent(QPaintEvent *) override;
};

#endif // SPLASHSCREEN_H
