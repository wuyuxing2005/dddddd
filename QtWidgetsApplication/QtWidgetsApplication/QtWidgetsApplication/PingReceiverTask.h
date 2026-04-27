#pragma once

#include <QObject>
#include <QString>

class QTcpSocket;
class QUdpSocket;
class QTimer;

class PingReceiverTask : public QObject {
    Q_OBJECT

public:
    explicit PingReceiverTask(QObject* parent = nullptr);

    QString ip;
    QTcpSocket* m_pingTcpSocket = nullptr;
    QUdpSocket* m_pingUdpSocket = nullptr;
    QTimer* m_pingUdpHeartbeatTimer = nullptr;

signals:
    void networkDisconnected();

public slots:
    void setupSockets();
    void stopSockets();
};
