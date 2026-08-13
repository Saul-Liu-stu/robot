#ifndef PIDWIDGET_H
#define PIDWIDGET_H

#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include "protocol_core.h"

class WaveChart : public QWidget
{
    Q_OBJECT
public:
    explicit WaveChart(QWidget *parent = nullptr);
    void addPoint(const WavePoint &pt);
    void clear();
protected:
    void paintEvent(QPaintEvent *) override;
private:
    QVector<WavePoint> m_data;
    int m_maxPoints = 100;
};

class PidWidget : public QWidget
{
    Q_OBJECT
public:
    explicit PidWidget(QWidget *parent = nullptr);
    void handleMessage(const PidMessage &m);
    void onReady();
    void reset();

    // 调试：暴露标签以便 mainwindow 直接写入
    QLabel *kpLabel()  { return m_kpVal; }
    QLabel *kiLabel()  { return m_kiVal; }
    QLabel *kdLabel()  { return m_kdVal; }
    QLabel *tgtLabel() { return m_tgtVal; }

signals:
    void commandRequested(const QString &cmd);

private:
    void setupUi();
    void updateParamDisplay();

    WaveChart *m_chart;
    QLabel *m_rpmLabel, *m_targetLabel;
    QLabel *m_kpVal, *m_kiVal, *m_kdVal, *m_tgtVal;
    QPushButton *m_btnGo, *m_btnStop;
    PidParams m_pid;
};
#endif
