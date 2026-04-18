#pragma once
#include <QObject>
#include <QUdpSocket>
#include <QImage>
#include <QTimer>
#include "VideoImageProvider.h"

class VideoStreamReceiver : public QObject {
    Q_OBJECT
public:
    // 构造函数声明
    explicit VideoStreamReceiver(VideoImageProvider* provider, QObject* parent = nullptr);

    // 🚀 新增：控制 UDP 打洞的接口
    void setRelayIp(const QString& ip);
    void startCamera();
    void stopCamera();

signals:
    // 信号声明
    void newFrameReceived();

private slots:
    // 槽函数声明
    void readPendingDatagrams();
    void sendHeartbeat(); // 发送打洞保活包

private:
    QUdpSocket* m_udpSocket;
    VideoImageProvider* m_provider;
    QString m_relayIp;
    QTimer* m_heartbeatTimer;
};