#include "PingWorker.h"

#include <QAbstractSocket>
#include <QTcpServer>
#include <QTcpSocket>
#include <QUdpSocket>
#include <QStringList>
#include <cstdio>
#include <cstring>

PingWorker::PingWorker(QObject* parent)
    : QObject(parent) {
}

void PingWorker::setup() {
    tcpServer = new QTcpServer(this);
    tcpServer->listen(QHostAddress::Any, 8890);
    connect(tcpServer, &QTcpServer::newConnection, this, &PingWorker::handleTcpNewConnection);

    udpSocket = new QUdpSocket(this);
    udpSocket->bind(QHostAddress::Any, 8890, QUdpSocket::ShareAddress);
    connect(udpSocket, &QUdpSocket::readyRead, this, &PingWorker::handleUdpReadyRead);

    emit logToUi(">> [Latency Thread] Ping Sockets initialized on Port 8890.");
}

void PingWorker::handleTcpNewConnection() {
    if (tcpClient) tcpClient->deleteLater();
    tcpClient = tcpServer->nextPendingConnection();
    tcpClient->setSocketOption(QAbstractSocket::LowDelayOption, 1);
    connect(tcpClient, &QTcpSocket::readyRead, this, &PingWorker::handleTcpReadyRead);
    emit logToUi(">> [Latency Thread] TCP Probe joined Port 8890.");
}

void PingWorker::sendPing(int payloadSize, bool isUdp) {
    if (payloadSize != lastPayloadSize) {
        txBuffer.resize(payloadSize + 64);
        memset(txBuffer.data(), 'X', payloadSize);
        lastPayloadSize = payloadSize;
    }

    qint64 t1 = timer->nsecsElapsed();
    char tail[64];
    int tailLen = sprintf_s(tail, sizeof(tail), "PING|%lld|", t1);
    memcpy(txBuffer.data() + payloadSize, tail, tailLen);

    if (isUdp) {
        if (!udpTargetIp.isNull()) {
            udpSocket->writeDatagram(txBuffer.constData(), payloadSize + tailLen, udpTargetIp, udpTargetPort);
        }
    }
    else {
        if (tcpClient && tcpClient->isOpen()) {
            tcpClient->write(txBuffer.constData(), payloadSize + tailLen);
        }
    }
}

void PingWorker::handleTcpReadyRead() {
    qint64 t2 = timer->nsecsElapsed();
    if (tcpClient) processData(tcpClient->readAll(), t2);
}

void PingWorker::handleUdpReadyRead() {
    while (udpSocket && udpSocket->hasPendingDatagrams()) {
        qint64 t2 = timer->nsecsElapsed();
        QByteArray datagram;
        datagram.resize(int(udpSocket->pendingDatagramSize()));
        QHostAddress senderIp;
        quint16 senderPort;
        udpSocket->readDatagram(datagram.data(), datagram.size(), &senderIp, &senderPort);

        if (senderIp.protocol() == QAbstractSocket::IPv6Protocol) {
            senderIp = QHostAddress(senderIp.toIPv4Address());
        }

        QString cmd = QString::fromUtf8(datagram).trimmed();
        if (cmd == "PING_HOLE_PUNCH") {
            static QHostAddress lastPunchIp;
            static quint16 lastPunchPort = 0;
            if (lastPunchIp != senderIp || lastPunchPort != senderPort) {
                emit logToUi(QString(">> [Latency Thread] UDP Probe punched: %1:%2").arg(senderIp.toString()).arg(senderPort));
                lastPunchIp = senderIp;
                lastPunchPort = senderPort;
            }
            udpTargetIp = senderIp;
            udpTargetPort = senderPort;
        }
        else {
            processData(datagram, t2);
        }
    }
}

void PingWorker::processData(const QByteArray& data, qint64 t_recv) {
    int pongIdx = data.lastIndexOf("PONG|");
    if (pongIdx != -1) {
        QString pongPart = QString::fromUtf8(data.mid(pongIdx));
        QStringList parts = pongPart.split('|');

        if (parts.size() >= 3) {
            qint64 t_sent = parts[1].toLongLong();
            if (t_sent > 0) {
                double rttMs = (t_recv - t_sent) / 1000000.0;

                emit pongReceived();

                if (currentPayloadIndex == 0) {
                    emit resultReady(rttMs / 2.0);
                }
                else {
                    emit resultReady(rttMs);
                }
            }
        }
    }
}

void PingWorker::stopAll() {
    if (tcpServer) tcpServer->close();
    if (udpSocket) udpSocket->close();
    if (tcpClient && tcpClient->isOpen()) tcpClient->disconnectFromHost();
}
