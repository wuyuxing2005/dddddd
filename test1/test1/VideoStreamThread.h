#pragma once

#include <QByteArray>
#include <QHostAddress>
#include <QThread>
#include <atomic>

class VideoStreamThread : public QThread {
    Q_OBJECT

public:
    explicit VideoStreamThread(QObject* parent = nullptr);

    void startStreaming(const QHostAddress& targetIp, int targetPort);
    void stopStreaming();

signals:
    void frameEncoded(const QByteArray& data, const QHostAddress& ip, int port, double latencyMs);
    void cameraStatusChanged(bool isOpen);

protected:
    void run() override;

private:
    std::atomic<bool> m_running{ false };
    QHostAddress m_targetIp;
    int m_targetPort = 8887;
};
