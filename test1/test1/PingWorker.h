#pragma once

#include <QObject>
#include <QByteArray>
#include <QElapsedTimer>
#include <QHostAddress>
#include <QString>

class QTcpServer;
class QTcpSocket;
class QUdpSocket;

class PingWorker : public QObject {
    Q_OBJECT

public:
    explicit PingWorker(QObject* parent = nullptr);

    QElapsedTimer* timer = nullptr;
    int currentPayloadIndex = 0;

signals:
    void resultReady(double latencyMs);
    void logToUi(const QString& msg);
    void pongReceived();

public slots:
    void setup();
    void stopAll();
    void sendPing(int payloadSize, bool isUdp);

private slots:
    void handleTcpNewConnection();
    void handleTcpReadyRead();
    void handleUdpReadyRead();
    void processData(const QByteArray& data, qint64 t_recv);

private:
    QTcpServer* tcpServer = nullptr;
    QTcpSocket* tcpClient = nullptr;
    QUdpSocket* udpSocket = nullptr;

    QHostAddress udpTargetIp;
    quint16 udpTargetPort = 0;

    QByteArray txBuffer;
    int lastPayloadSize = -1;
};
