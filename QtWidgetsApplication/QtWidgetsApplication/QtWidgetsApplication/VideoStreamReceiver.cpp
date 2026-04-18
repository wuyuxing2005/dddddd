#include "VideoStreamReceiver.h"
#include <QDebug>

VideoStreamReceiver::VideoStreamReceiver(VideoImageProvider* provider, QObject* parent)
    : QObject(parent), m_provider(provider)
{
    m_udpSocket = new QUdpSocket(this);
    // 绑定 8886 端口，同时用于接收图像和发送打洞包！
    m_udpSocket->bind(8886, QUdpSocket::ShareAddress);
    connect(m_udpSocket, &QUdpSocket::readyRead, this, &VideoStreamReceiver::readPendingDatagrams);

    m_heartbeatTimer = new QTimer(this);
    connect(m_heartbeatTimer, &QTimer::timeout, this, &VideoStreamReceiver::sendHeartbeat);
}

void VideoStreamReceiver::setRelayIp(const QString& ip) {
    m_relayIp = ip;
}

void VideoStreamReceiver::startCamera() {
    sendHeartbeat(); // 勾选瞬间立刻打洞
    m_heartbeatTimer->start(2000); // 核心网映射可能会过期，每2秒发一次保活
}

void VideoStreamReceiver::stopCamera() {
    m_heartbeatTimer->stop();
    if (!m_relayIp.isEmpty()) {
        QByteArray cmd = "VIDEO_STOP";
        m_udpSocket->writeDatagram(cmd, QHostAddress(m_relayIp), 8886);
    }
}

void VideoStreamReceiver::sendHeartbeat() {
    if (!m_relayIp.isEmpty()) {
        QByteArray cmd = "VIDEO_START";
        m_udpSocket->writeDatagram(cmd, QHostAddress(m_relayIp), 8886);
        // 💡 埋点 2：监控 Touch 到底有没有往外发包
       // qDebug() << "🎯 [UDP打洞] 发送心跳包 ->" << m_relayIp << ":8886";
    }
}

void VideoStreamReceiver::readPendingDatagrams() {
    while (m_udpSocket->hasPendingDatagrams()) {
        QByteArray datagram;
        datagram.resize(int(m_udpSocket->pendingDatagramSize()));
        m_udpSocket->readDatagram(datagram.data(), datagram.size());

        // 💡 埋点 3：监控 Touch 有没有收到视频！
        //qDebug() << "📦 [UDP接收] 收到视频帧，大小:" << datagram.size() << "bytes";

        QImage frame;
        if (frame.loadFromData(datagram, "JPG")) {
            m_provider->updateImage(frame);
            emit newFrameReceived();
        }
    }
}