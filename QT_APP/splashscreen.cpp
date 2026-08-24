#include "splashscreen.h"
#include <QPainter>
#include <QFont>
#include <QPixmap>

SplashScreen::SplashScreen(QWidget *parent) : QWidget(parent)
{
    // 全屏无边框, 与 APP 同款石墨深色背景
    setWindowFlags(Qt::Window | Qt::FramelessWindowHint);
    setStyleSheet(QStringLiteral("background:#141518;"));
}

void SplashScreen::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::SmoothPixmapTransform, true);
    const QPointF c(rect().center());

    // logo 图 (圆形裁剪资源): 直径 = 屏幕短边 38%, 居中偏上
    const QPixmap logo(QStringLiteral(":/assets/logo.png"));
    const double D = qMin(width(), height()) * 0.38;
    const QRectF logoR(c.x() - D / 2, c.y() - D * 0.66, D, D);
    if (!logo.isNull())
        p.drawPixmap(logoR, logo, QRectF(logo.rect()));

    // 标题 (logo 下方, 紧凑排布防重叠)
    QFont f = font();
    const double titleY = logoR.bottom() + 6.0;
    f.setPixelSize(qMax(18, (int)(D * 0.20)));
    f.setBold(true);
    p.setFont(f);
    p.setPen(QColor(QStringLiteral("#e5e7eb")));
    p.drawText(QRectF(c.x() - D * 1.6, titleY, D * 3.2, D * 0.30),
               Qt::AlignHCenter | Qt::AlignTop, QStringLiteral("四足搜救机器人"));

    // 英文副标题
    f.setPixelSize(qMax(10, (int)(D * 0.11)));
    f.setBold(false);
    p.setFont(f);
    p.setPen(QColor(QStringLiteral("#6b7280")));
    p.drawText(QRectF(c.x() - D * 1.6, titleY + D * 0.30, D * 3.2, D * 0.20),
               Qt::AlignHCenter | Qt::AlignTop, QStringLiteral("RESCUE QUADRUPED ROBOT"));

    // 底部版本 + 加载文字 (贴底, 远离副标题)
    f.setPixelSize(qMax(10, (int)(D * 0.10)));
    p.setFont(f);
    p.setPen(QColor(QStringLiteral("#4b5563")));
    p.drawText(QRectF(0, height() * 0.90, width(), 26),
               Qt::AlignHCenter | Qt::AlignTop,
               QStringLiteral("v6.17 · 正在初始化..."));
}
