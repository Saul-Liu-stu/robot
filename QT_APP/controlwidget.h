#ifndef CONTROLWIDGET_H
#define CONTROLWIDGET_H

#include <QWidget>
#include <QPushButton>
#include <QLineEdit>

// ============================================================
// 命令发送控制面板
// 自定义十六进制命令输入
// ============================================================

class ControlWidget : public QWidget
{
    Q_OBJECT
public:
    explicit ControlWidget(QWidget *parent = nullptr);

signals:
    // cmd: 0x00 ~ 0xFF
    void commandRequested(uint8_t cmd);

private slots:
    void onCustomSend();

private:
    void setupUi();

    QLineEdit *m_customCmdEdit;
    QPushButton *m_customSendBtn;

    // 配色
    static const QString C_BG;
    static const QString C_CARD;
    static const QString C_BTN;
    static const QString C_ACCENT;
    static const QString C_TXT;
};

#endif // CONTROLWIDGET_H
