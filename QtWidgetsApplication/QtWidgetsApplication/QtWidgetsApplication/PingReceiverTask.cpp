#include "PingReceiverTask.h"

#include <QByteArray>
#include <QHostAddress>
#include <QNetworkProxy>
#include <QTcpSocket>
#include <QTimer>
#include <QUdpSocket>
#include <cstring>

PingReceiverTask::PingReceiverTask(QObject* parent)
    : QObject(parent) {
}

void PingReceiverTask::setupSockets() {
    stopSockets();

    m_pingTcpSocket = new QTcpSocket(this);
    m_pingTcpSocket->setProxy(QNetworkProxy::NoProxy);
    m_pingTcpSocket->setSocketOption(QAbstractSocket::LowDelayOption, 1);

    connect(m_pingTcpSocket, &QTcpSocket::disconnected, this, [this]() {
        emit networkDisconnected();
        });

    connect(m_pingTcpSocket, &QTcpSocket::readyRead, this, [this]() {
        static QByteArray tcpPingBuffer;
        tcpPingBuffer.append(m_pingTcpSocket->readAll());

        if (tcpPingBuffer.size() > 8192) {
            tcpPingBuffer.remove(0, tcpPingBuffer.size() - 8192);
        }

        int idx = tcpPingBuffer.lastIndexOf("PING|");
        if (idx != -1) {
            int endIdx = tcpPingBuffer.indexOf('|', idx + 5);
            if (endIdx != -1) {
                QByteArray pongData = tcpPingBuffer.mid(idx, endIdx - idx + 1);
                memcpy(pongData.data(), "PONG", 4);
                m_pingTcpSocket->write(pongData);
                m_pingTcpSocket->flush();
                tcpPingBuffer.clear();
            }
        }
        });

    m_pingUdpSocket = new QUdpSocket(this);
    if (m_pingUdpSocket->state() != QAbstractSocket::BoundState) {
        m_pingUdpSocket->bind(QHostAddress::Any, 0);
    }

    connect(m_pingUdpSocket, &QUdpSocket::readyRead, this, [this]() {
        while (m_pingUdpSocket->hasPendingDatagrams()) {
            QByteArray data;
            data.resize(int(m_pingUdpSocket->pendingDatagramSize()));
            QHostAddress senderIp;
            quint16 senderPort;
            m_pingUdpSocket->readDatagram(data.data(), data.size(), &senderIp, &senderPort);

            int idx = data.lastIndexOf("PING|");
            if (idx != -1) {
                int endIdx = data.indexOf('|', idx + 5);
                if (endIdx != -1) {
                    QByteArray pongData = data.mid(idx, endIdx - idx + 1);
                    memcpy(pongData.data(), "PONG", 4);
                    m_pingUdpSocket->writeDatagram(pongData, senderIp, senderPort);
                }
            }
        }
        });

    m_pingUdpHeartbeatTimer = new QTimer(this);
    connect(m_pingUdpHeartbeatTimer, &QTimer::timeout, this, [this]() {
        if (m_pingUdpSocket) {
            m_pingUdpSocket->writeDatagram("PING_HOLE_PUNCH", QHostAddress(ip), 8890);
        }
        });

    m_pingTcpSocket->connectToHost(ip, 8890);
    m_pingUdpHeartbeatTimer->start(2000);
    m_pingUdpSocket->writeDatagram("PING_HOLE_PUNCH", QHostAddress(ip), 8890);
}

void PingReceiverTask::stopSockets() {
    if (m_pingUdpHeartbeatTimer) {
        m_pingUdpHeartbeatTimer->stop();
        delete m_pingUdpHeartbeatTimer;
        m_pingUdpHeartbeatTimer = nullptr;
    }
    if (m_pingTcpSocket) {
        m_pingTcpSocket->abort();
        delete m_pingTcpSocket;
        m_pingTcpSocket = nullptr;
    }
    if (m_pingUdpSocket) {
        m_pingUdpSocket->abort();
        delete m_pingUdpSocket;
        m_pingUdpSocket = nullptr;
    }
}
