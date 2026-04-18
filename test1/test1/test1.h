#pragma once
#include <QtWidgets/QMainWindow>
#include "ui_test1.h"
#include <QTcpServer> 
#include <QTcpSocket>
#include <QElapsedTimer>
#include "qcustomplot.h"
#include <cstdint>
#include <QTimer>
#include <QThread>
#include <QUdpSocket>
#include <QHostAddress>
#include <opencv2/opencv.hpp>
#include <atomic>

// =========================================================
// 🚀 独立测速后台线程类 (彻底剥离 UI 干扰)
// =========================================================
class PingWorker : public QObject {
    Q_OBJECT
public:
    explicit PingWorker(QObject* parent = nullptr) : QObject(parent) {}

    QElapsedTimer* timer;
    int currentPayloadIndex = 0; // 0:32B, 1:50KB, 2:3MB

signals:
    void resultReady(double latencyMs);
    void logToUi(const QString& msg);
    void pongReceived(); // 收到回响后通知 UI 解除发包锁定

public slots:
    void setup();
    void stopAll();
    void sendPing(int payloadSize, bool isUdp); // 接收 UI 的发包指令

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

// =========================================================
// 🚀 UDP Video Stream Thread
// =========================================================
class VideoStreamThread : public QThread {
    Q_OBJECT
public:
    explicit VideoStreamThread(QObject* parent = nullptr) : QThread(parent) {}

    void startStreaming(const QHostAddress& targetIp, int targetPort) {
        if (m_running && m_targetIp == targetIp && m_targetPort == targetPort) return;
        m_targetIp = targetIp;
        m_targetPort = targetPort;
        if (!m_running) {
            m_running = true;
            start();
        }
    }

    void stopStreaming() {
        m_running = false;
        wait();
    }

signals:
    // 🚀 核心更新：加入 latencyMs，将视频处理时延传给主线程画图
    void frameEncoded(const QByteArray& data, const QHostAddress& ip, int port, double latencyMs);

    // 🚀 新增这一行：用于向上级报告摄像头是否打开成功
    void cameraStatusChanged(bool isOpen);

protected:
    void run() override {
        cv::VideoCapture cap(0, cv::CAP_DSHOW);
        if (!cap.isOpened()) {
            qDebug() << "[Video Stream] Camera open failed!";

            // 🚀 失败时发信号
            emit cameraStatusChanged(false);
            return;
        }

        // 🚀 成功时发信号
        emit cameraStatusChanged(true);

        cap.set(cv::CAP_PROP_FRAME_WIDTH, 640);
        cap.set(cv::CAP_PROP_FRAME_HEIGHT, 480);

        cv::Mat frame;
        std::vector<uchar> buffer;
        std::vector<int> encodeParams = { cv::IMWRITE_JPEG_QUALITY, 60 };

        qDebug() << "[Video Stream] Started pushing to" << m_targetIp.toString() << ":" << m_targetPort;

        while (m_running) {
            // 🚀 核心更新：在这里给每一帧截图和压缩计死时延
            QElapsedTimer processTimer;
            processTimer.start();

            cap >> frame;
            if (frame.empty()) continue;

            cv::imencode(".jpg", frame, buffer, encodeParams);

            if (!buffer.empty() && buffer.size() < 65500) {
                double latency = processTimer.nsecsElapsed() / 1000000.0;
                QByteArray frameData(reinterpret_cast<const char*>(buffer.data()), buffer.size());

                // 把算出来的时延跟着帧一起发出去
                emit frameEncoded(frameData, m_targetIp, m_targetPort, latency);
            }
            QThread::msleep(33);
        }

        cap.release();
        qDebug() << "[Video Stream] Push thread safely exited";
    }

private:
    std::atomic<bool> m_running{ false };
    QHostAddress m_targetIp;
    int m_targetPort = 8887;
};

// =========================================================
// 🚀 Dobot V3 RealTime Data Dict (1440 Bytes)
// =========================================================
#pragma pack(push, 1)
struct DobotRealTimeData {
    uint8_t reserved1[432];
    double qActual[6];
    uint8_t reserved2[144];
    double toolVectorActual[6];
    uint8_t reserved3[768];
};
#pragma pack(pop)

static_assert(sizeof(DobotRealTimeData) == 1440, "DobotRealTimeData size must be exactly 1440 bytes");

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

    // 🚀 接收后台测速线程的结果
    void updateLatencyFromWorker(double latencyMs);

private:
    Ui::test1Class ui;

    // --- Control & Stream Sockets ---
    QTcpServer* m_server;
    QTcpSocket* m_client;
    QTcpSocket* m_robot_motion;
    QTcpSocket* m_robot_enable;

    QTcpSocket* m_robotStatusSocket;
    QByteArray m_statusBuffer;
    QTimer* m_statusTimeoutTimer;

    // --- 🚀 Dedicated Latency Test Core ---
    QThread* m_latencyThread = nullptr;
    PingWorker* m_worker = nullptr;

    void initRobotConnections(QString ip, int motionPort, int enablePort, int realtimePort);
    void initProcessLatencyPlot();
    void updateProcessLatencyUI(int channelIndex, double latencyMs);

    // 🚀 核心更新：将 Tactile 改为 Twin
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