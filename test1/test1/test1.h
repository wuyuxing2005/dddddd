#pragma once

#include <QtWidgets/QMainWindow>
#include "ui_test1.h"

#include "qcustomplot.h"

#include <QByteArray>
#include <QElapsedTimer>
#include <QHostAddress>
#include <QList>
#include <QString>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>
#include <QUdpSocket>

class PingWorker;
class QThread;
class VideoStreamThread;

class test1 : public QMainWindow
{
    Q_OBJECT

public:
    test1(QWidget* parent = nullptr);
    ~test1();

private slots:
    void on_btn_start_clicked();
    void on_btn_stop_clicked();

    void onRobotStatusReadyRead();
    void onUdpCommandReadyRead();
    void onUdpTwinReadyRead();

    void updateLatencyFromWorker(double latencyMs);

private:
    Ui::test1Class ui;

    QTcpServer* m_server;
    QTcpSocket* m_client;
    QTcpSocket* m_robot_motion;
    QTcpSocket* m_robot_enable;

    QTcpSocket* m_robotStatusSocket;
    QByteArray m_statusBuffer;
    QTimer* m_statusTimeoutTimer;

    QThread* m_latencyThread = nullptr;
    PingWorker* m_worker = nullptr;

    void initRobotConnections(QString ip, int motionPort, int enablePort, int realtimePort);
    void initProcessLatencyPlot();
    void updateProcessLatencyUI(int channelIndex, double latencyMs);

    int m_packetCountControl = 0;
    int m_packetCountTwin = 0;
    int m_packetCountVideo = 0;
    int m_packetCountFeedback = 0;

    QList<double> m_latencyHistory[4];
    const int m_maxWindowSize = 20;

    bool m_isWaitingPONG = false;
    QElapsedTimer m_netTestTimer;
    QList<double> m_networkLatencyHistory;
    void updateNetworkLatencyUI(double netLatency);

    QTimer* m_netTimer = nullptr;
    int m_packetCountNetwork = 0;

    VideoStreamThread* m_videoThread = nullptr;

    QUdpSocket* m_udpCommandSocket;
    QTimer* m_videoHeartbeatTimer;

    QUdpSocket* m_udpTwinSocket;
    QTimer* m_twinHeartbeatTimer;
    QHostAddress m_twinTargetIp;
    quint16 m_twinTargetPort = 0;
};
