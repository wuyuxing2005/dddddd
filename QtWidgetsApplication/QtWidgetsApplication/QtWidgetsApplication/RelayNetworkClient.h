#pragma once

#include <QObject>
#include <QString>
#include <QMutex>
#include <QQueue>
#include <QThread>
#include <QTimer>
#include <QUdpSocket>
#include <atomic>

#include "RelayMoveDelta.h"

#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#pragma comment(lib, "ws2_32.lib")

class PingReceiverTask;
class RelaySenderThread;

class RelayNetworkClient : public QObject {
    Q_OBJECT

public:
    explicit RelayNetworkClient(QObject* parent = nullptr);
    ~RelayNetworkClient();

    void setTwinEnabled(bool enabled);
    bool isTwinEnabled() const;
    void connectToRelay(const QString& ip, int port);
    void disconnectFromRelay();
    bool isConnected() const;
    bool getRobotCurrentPos();

    void connectAndInitializeAsync(const QString& ip, int port = 8888);
    void refreshRobotBaseAsync();

    void connectTwin();
    void disconnectTwin();

public slots:
    void startRobotInitialization();
    void sendMoveCommand(double dx, double dy, double dz);
    void stopAll();

signals:
    void logMessage(const QString& msg);
    void connectionStatusChanged(bool connected);
    void initializationFinished(bool success);
    void robotPoseRefreshFinished(bool success);
    void twinDataReceived(double j1, double j2, double j3, double j4, double j5, double j6);

private slots:
    void onTwinUdpReadyRead();
    void sendTwinHeartbeat();

private:
    friend class RelaySenderThread;

    static constexpr int ENABLE_PORT = 29999;
    static constexpr int MOTION_PORT = 30003;
    static constexpr int STATUS_PORT = 8889;

    static constexpr float SpeedL = 100.0f;
    static constexpr float MIN_DELTA_THRESHOLD = 1.0f;
    static constexpr unsigned int CP_SMOOTH_RATIO = 100;
    static constexpr int FEEDBACK_TIMEOUT = 2000;
    static constexpr int MAX_RELAY_WAIT_TIME = 5000;
    static constexpr int MAX_QUEUE_SIZE = 5;
    static constexpr int RobotMode_CHECK_INTERVAL = 300;

    const char* ENABLE_CMD = "EnableRobot(0.5,0,0,0)";
    const char* DISABLE_CMD = "DisableRobot()";
    const char* CLEAR_ERROR_CMD = "ClearError()";
    const char* REALTIME_MOVE_CMD = "MovL(%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,0,SpeedL=%.0f)";

    SOCKET relaySocket = INVALID_SOCKET;
    mutable QMutex relaySocketMutex;
    mutable QMutex m_transactionMutex;

    bool clientRunning = false;
    bool isTcpConnected = false;
    bool isRobotInAlarm = false;
    bool isCpSet = false;
    bool isRobotBaseSet = false;
    bool isClosing = false;
    bool m_twinEnabled = false;

    double robotBaseX = 0.0, robotBaseY = 0.0, robotBaseZ = 0.0;
    double robotBaseRx = 0.0, robotBaseRy = 0.0, robotBaseRz = 0.0;

    char recvBuffer[1024] = { 0 };
    int recvBufferLen = 0;
    QMutex recvBufferMutex;

    QString m_relayIp;
    int m_relayPort = 8888;

    QQueue<RelayMoveDelta> sendQueue;
    QMutex queueMutex;
    RelaySenderThread* m_senderThread = nullptr;

    QUdpSocket* m_udpTwinSocket = nullptr;
    QTimer* m_twinHeartbeatTimer = nullptr;

    QThread* m_pingThread = nullptr;
    PingReceiverTask* m_pingTask = nullptr;

    std::atomic<bool> m_connectInitBusy{ false };
    std::atomic<bool> m_poseRefreshBusy{ false };

    bool connectRelay();
    void closeRelay();
    bool sendToRelay(int targetPort, const char* cmd);

    bool readFeedbackAndCheck(SOCKET sock, const char* portName, char* outBuf, int outBufLen);
    bool clearRobotAlarm();
    void updateAlarmStatus();
    bool sendCpCommand(unsigned int smoothRatio);
    bool sendCoordinates(double x, double y, double z);

    void appendLog(const QString& msg);
};
