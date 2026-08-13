#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QLabel>
#include <QPushButton>
#include <QListWidget>
#include <QTextEdit>
#include <QStackedWidget>
#include "bluetoothclient.h"
#include "servowidget.h"
#include "pidwidget.h"

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void onBtStateChanged(BluetoothClient::State s);
    void onDeviceDiscovered(const QBluetoothDeviceInfo &device);
    void onServoResponse(const ServoResponse &rsp);
    void onPidMessage(const PidMessage &m);
    void onServoAngleRequested(int srv, int angle);
    void onPidCommandRequested(const QString &cmd);
    void onScanClicked();
    void onConnectClicked();
    void onDisconnectClicked();

private:
    void setupUi();
    void appendLog(const QString &msg, const QString &color = QStringLiteral("#22c55e"));

    BluetoothClient *m_bt = nullptr;
    QLabel *m_statusIcon, *m_statusLabel;
    QPushButton *m_btnScan, *m_btnConnect, *m_btnDisconnect;
    QListWidget *m_devList;
    QStackedWidget *m_stack;
    ServoWidget *m_servoWidget;
    PidWidget *m_pidWidget;
    QTextEdit *m_logView;
    int m_lineNum = 0;

public:
    static const QString C_BG, C_CARD, C_ACCENT, C_BLUE;
    static const QString C_GREEN, C_ORANGE, C_RED, C_TXT, C_DIM;
};

#endif
