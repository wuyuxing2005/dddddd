#include "RelayNetworkClient.h"
#include <QDateTime>
#include <QThread>
#include <QMutexLocker>
#include <QByteArray>
#include <cstring>
#include <thread>
#include <QNetworkProxy>

// =========================================================
// 🚀 独立测速线程任务类的实现
// =========================================================
void PingReceiverTask::setupSockets() {
    // 确保清理旧的遗留对象
    stopSockets();

    // 1. TCP 测速专线初始化 (禁用 Nagle)
    m_pingTcpSocket = new QTcpSocket(this);
    m_pingTcpSocket->setProxy(QNetworkProxy::NoProxy);
    m_pingTcpSocket->setSocketOption(QAbstractSocket::LowDelayOption, 1);

    // 🚀 新增：当 TCP 连接被中转站主动掐断时，立刻发出预警信号
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

    // 2. UDP 测速专线初始化
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

    // 3. UDP 保活心跳定时器
    m_pingUdpHeartbeatTimer = new QTimer(this);
    connect(m_pingUdpHeartbeatTimer, &QTimer::timeout, this, [this]() {
        if (m_pingUdpSocket) {
            m_pingUdpSocket->writeDatagram("PING_HOLE_PUNCH", QHostAddress(ip), 8890);
        }
        });

    // 4. 启动连接与定时器
    m_pingTcpSocket->connectToHost(ip, 8890);
    m_pingUdpHeartbeatTimer->start(2000);
    m_pingUdpSocket->writeDatagram("PING_HOLE_PUNCH", QHostAddress(ip), 8890);
}

void PingReceiverTask::stopSockets() {
    // 🚀 核心修复：彻底在正确的线程销毁对象，避免跨线程强拆引发的 Crash！
    if (m_pingUdpHeartbeatTimer) {
        m_pingUdpHeartbeatTimer->stop();
        delete m_pingUdpHeartbeatTimer;
        m_pingUdpHeartbeatTimer = nullptr;
    }
    if (m_pingTcpSocket) {
        m_pingTcpSocket->abort(); // 立即掐断网络，不等待
        delete m_pingTcpSocket;
        m_pingTcpSocket = nullptr;
    }
    if (m_pingUdpSocket) {
        m_pingUdpSocket->abort();
        delete m_pingUdpSocket;
        m_pingUdpSocket = nullptr;
    }
}


// =========================================================
// 主体网络类实现
// =========================================================
RelaySenderThread::RelaySenderThread(RelayNetworkClient* owner)
    : m_owner(owner), m_running(true) {
}

void RelaySenderThread::stop() {
    m_running = false;
}

void RelaySenderThread::run() {
    const int IDLE_SLEEP_MS = 1;
    m_owner->appendLog("[Sender Thread] Started successfully");

    while (m_running) {
        SendData data;
        bool hasData = false;

        {
            QMutexLocker locker(&m_owner->queueMutex);
            if (!m_owner->sendQueue.isEmpty()) {
                data = m_owner->sendQueue.dequeue();
                hasData = true;
            }
        }

        if (hasData) {
            m_owner->sendCoordinates(data.deltaX, data.deltaY, data.deltaZ);
        }
        else {
            Sleep(IDLE_SLEEP_MS);
        }
    }
    m_owner->appendLog("[Sender Thread] Exited");
}

RelayNetworkClient::RelayNetworkClient(QObject* parent)
    : QObject(parent) {
    WSADATA wsaData;
    int ret = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (ret != 0) {
        appendLog(QString("[Network Error] WSAStartup failed: %1").arg(ret));
    }

    m_udpTwinSocket = new QUdpSocket(this);
    m_udpTwinSocket->bind(QHostAddress::Any, 0);
    connect(m_udpTwinSocket, &QUdpSocket::readyRead, this, &RelayNetworkClient::onTwinUdpReadyRead);

    m_twinHeartbeatTimer = new QTimer(this);
    connect(m_twinHeartbeatTimer, &QTimer::timeout, this, &RelayNetworkClient::sendTwinHeartbeat);

    // 🚀 核心修复：在构造函数主线程中初始化线程结构，彻底解决点击Start卡死和连接慢的问题！
    m_pingThread = new QThread(this);
    m_pingTask = new PingReceiverTask();
    m_pingTask->moveToThread(m_pingThread);
    m_pingThread->start(); // 让它空载挂起，消耗为0
}

RelayNetworkClient::~RelayNetworkClient() {
    stopAll();

    // 安全退出全局测速线程
    if (m_pingThread) {
        QMetaObject::invokeMethod(m_pingTask, "stopSockets", Qt::BlockingQueuedConnection);
        m_pingThread->quit();
        m_pingThread->wait();
        delete m_pingTask;
    }

    WSACleanup();
}

void RelayNetworkClient::setTwinEnabled(bool enabled) {
    m_twinEnabled = enabled;
}

bool RelayNetworkClient::isTwinEnabled() const {
    return m_twinEnabled;
}

void RelayNetworkClient::appendLog(const QString& msg) {
    emit logMessage(msg);
}

bool RelayNetworkClient::isConnected() const {
    return isTcpConnected;
}

void RelayNetworkClient::connectAndInitializeAsync(const QString& ip, int port) {
    bool expected = false;
    if (!m_connectInitBusy.compare_exchange_strong(expected, true)) {
        appendLog("[Network] Connecting/Initializing, please do not click repeatedly");
        return;
    }

    std::thread([this, ip, port]() {
        this->connectToRelay(ip, port);

        if (!this->isConnected()) {
            emit this->initializationFinished(false);
            this->m_connectInitBusy = false;
            return;
        }

        this->startRobotInitialization();
        this->m_connectInitBusy = false;
        }).detach();
}

void RelayNetworkClient::refreshRobotBaseAsync() {
    bool expected = false;
    if (!m_poseRefreshBusy.compare_exchange_strong(expected, true)) {
        return;
    }

    std::thread([this]() {
        const bool ok = this->getRobotCurrentPos();
        emit this->robotPoseRefreshFinished(ok);
        this->m_poseRefreshBusy = false;
        }).detach();
}

void RelayNetworkClient::connectToRelay(const QString& ip, int port) {
    m_relayIp = ip;
    m_relayPort = port;
    isClosing = false;

    if (connectRelay()) {
        // 🚀 核心修复：因为当前在 std::thread 中，绝不能在这里创建 QObject。
        // 通过 BlockingQueuedConnection 抛回主线程安全创建，确保线程绑定正确无误！
        QMetaObject::invokeMethod(this, [this, ip, port]() {
            isTcpConnected = true;
            clientRunning = true;
            emit connectionStatusChanged(true);
            appendLog(QString("[Network] Successfully connected to Relay Station %1:%2").arg(ip).arg(port));

            // 安全命令已在运行的测速线程干活
            m_pingTask->ip = ip;
            QMetaObject::invokeMethod(m_pingTask, "setupSockets", Qt::QueuedConnection);

            // 🚀 新增：一旦侦察兵报告中转站断开，立刻触发 Touch 端的全面清理机制！
            connect(m_pingTask, &PingReceiverTask::networkDisconnected, this, [this]() {
                if (isTcpConnected) {
                    appendLog(">> [Warning] Relay Station closed the connection unexpectedly!");
                    this->stopAll(); // 这会自动断开所有连接，并发出 connectionStatusChanged(false) 信号
                }
                }, Qt::QueuedConnection);

            // 安全在主线程启动发送线程
            if (!m_senderThread) {
                m_senderThread = new RelaySenderThread(this);
                m_senderThread->start();
            }
            }, Qt::BlockingQueuedConnection);
    }
    else {
        QMetaObject::invokeMethod(this, [this, ip, port]() {
            isTcpConnected = false;
            emit connectionStatusChanged(false);
            appendLog(QString("[Network Error] Failed to connect to Relay Station %1:%2").arg(ip).arg(port));
            }, Qt::BlockingQueuedConnection);
    }
}

void RelayNetworkClient::disconnectFromRelay() {
    clientRunning = false;
    isTcpConnected = false;

    disconnectTwin();

    // 🚀 安全让测速 Socket 关停，但保留线程以便下次秒连
    if (m_pingTask) {
        QMetaObject::invokeMethod(m_pingTask, "stopSockets", Qt::BlockingQueuedConnection);
    }

    if (m_senderThread) {
        m_senderThread->stop();
        m_senderThread->wait();
        delete m_senderThread;
        m_senderThread = nullptr;
    }

    {
        QMutexLocker locker(&queueMutex);
        sendQueue.clear();
    }

    if (relaySocket != INVALID_SOCKET) {
        sendToRelay(Config::ENABLE_PORT, DISABLE_CMD);
        Sleep(100);
    }

    closeRelay();

    isRobotBaseSet = false;
    isCpSet = false;

    emit connectionStatusChanged(false);
    appendLog("[Network] Disconnected");
}

void RelayNetworkClient::stopAll() {
    if (isClosing) return;
    isClosing = true;
    disconnectFromRelay();
}

bool RelayNetworkClient::connectRelay() {
    QMutexLocker locker(&relaySocketMutex);

    if (relaySocket != INVALID_SOCKET) {
        closesocket(relaySocket);
        relaySocket = INVALID_SOCKET;
    }

    relaySocket = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, 0);
    if (relaySocket == INVALID_SOCKET) {
        appendLog(QString("[Network Error] Failed to create socket: %1").arg(WSAGetLastError()));
        return false;
    }

    sockaddr_in relayAddr;
    memset(&relayAddr, 0, sizeof(relayAddr));
    relayAddr.sin_family = AF_INET;
    relayAddr.sin_port = htons(static_cast<u_short>(m_relayPort));
    inet_pton(AF_INET, m_relayIp.toStdString().c_str(), &relayAddr.sin_addr);

    int result = ::connect(relaySocket, reinterpret_cast<SOCKADDR*>(&relayAddr), sizeof(relayAddr));
    if (result == SOCKET_ERROR) {
        appendLog(QString("[Network Error] Connection failed: %1").arg(WSAGetLastError()));
        closesocket(relaySocket);
        relaySocket = INVALID_SOCKET;
        return false;
    }

    return true;
}

void RelayNetworkClient::closeRelay() {
    QMutexLocker locker(&relaySocketMutex);
    if (relaySocket != INVALID_SOCKET) {
        closesocket(relaySocket);
        relaySocket = INVALID_SOCKET;
    }
}

bool RelayNetworkClient::sendToRelay(int targetPort, const char* cmd) {
    if (!isTcpConnected || isClosing) {
        appendLog("[Send Error] Not connected to Relay Station");
        return false;
    }

    QMutexLocker transLocker(&m_transactionMutex);

    SOCKET currentSocket = INVALID_SOCKET;
    {
        QMutexLocker locker(&relaySocketMutex);
        currentSocket = relaySocket;
        bool socketValid = (currentSocket != INVALID_SOCKET);
        if (!socketValid) {
            appendLog("[Send Error] Relay Station socket invalid");
            return false;
        }

        char buffer[512];
        int len = sprintf_s(buffer, sizeof(buffer), "%d|%s", targetPort, cmd);
        if (len <= 0 || len >= static_cast<int>(sizeof(buffer))) {
            appendLog("[Send Error] Command formatting failed");
            return false;
        }

        int bytesSent = send(currentSocket, buffer, len, 0);
        if (bytesSent == SOCKET_ERROR || bytesSent != len) {
            appendLog(QString("[Send Error] Send failed: %1").arg(WSAGetLastError()));
            return false;
        }
    }

    bool success = false;
    DWORD startWaitTime = GetTickCount();
    char feedbackBuf[1024];

    while (!success) {
        success = readFeedbackAndCheck(currentSocket, "Relay Station", feedbackBuf, sizeof(feedbackBuf));
        if (success) break;

        Sleep(10);

        DWORD currentTime = GetTickCount();
        DWORD totalWaitTime = (currentTime >= startWaitTime)
            ? (currentTime - startWaitTime)
            : (currentTime + (0xFFFFFFFF - startWaitTime));

        if (totalWaitTime >= Config::MAX_RELAY_WAIT_TIME) {
            appendLog(QString("[Feedback Timeout] Target Port=%1, Command=%2").arg(targetPort).arg(cmd));
            return false;
        }

        Sleep(50);
    }

    appendLog(QString("[Send Success] Port=%1, Command=%2").arg(targetPort).arg(cmd));
    return true;
}

bool RelayNetworkClient::readFeedbackAndCheck(SOCKET sock, const char* portName, char* outBuf, int outBufLen) {
    memset(outBuf, 0, outBufLen);
    DWORD startWaitTime = GetTickCount();

    while (true) {
        {
            QMutexLocker locker(&recvBufferMutex);
            char* delimiter = strstr(recvBuffer, ";");
            if (delimiter != nullptr) {
                int msgLen = delimiter - recvBuffer + 1;
                int copyLen = min(msgLen, outBufLen - 1);
                memcpy(outBuf, recvBuffer, copyLen);
                outBuf[copyLen] = '\0';

                int remainingLen = recvBufferLen - msgLen;
                if (remainingLen > 0) {
                    memmove(recvBuffer, recvBuffer + msgLen, remainingLen);
                }
                recvBuffer[remainingLen] = '\0';
                recvBufferLen = remainingLen;

                break;
            }
        }

        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(sock, &readfds);

        timeval tv;
        tv.tv_sec = 0;
        tv.tv_usec = 1000 * 10;

        int ret = select(0, &readfds, NULL, NULL, &tv);
        if (ret > 0 && FD_ISSET(sock, &readfds)) {
            char temp[4096] = { 0 };
            int recvLen = recv(sock, temp, sizeof(temp) - 1, 0);

            if (recvLen > 0) {
                temp[recvLen] = '\0';

                if (strlen(temp) > 0) {
                    QMutexLocker locker(&recvBufferMutex);
                    int copyLen = min(static_cast<int>(strlen(temp)), static_cast<int>(sizeof(recvBuffer)) - recvBufferLen - 1);
                    if (copyLen > 0) {
                        memcpy(recvBuffer + recvBufferLen, temp, copyLen);
                        recvBufferLen += copyLen;
                        recvBuffer[recvBufferLen] = '\0';
                    }
                }
            }
            else if (recvLen == 0) {
                return false;
            }
            else {
                if (WSAGetLastError() != WSAEWOULDBLOCK) return false;
            }
        }

        DWORD currentTime = GetTickCount();
        DWORD waitTime = (currentTime >= startWaitTime)
            ? (currentTime - startWaitTime)
            : (0xFFFFFFFF - startWaitTime + currentTime);

        if (waitTime >= Config::FEEDBACK_TIMEOUT) {
            appendLog(QString("[Feedback Timeout] %1").arg(portName));
            return false;
        }
    }

    appendLog(QString("[%1 Feedback] %2").arg(portName).arg(QString::fromLocal8Bit(outBuf)));

    char* realStart = strstr(outBuf, "0,");
    if (!realStart) realStart = strstr(outBuf, "1,");
    if (!realStart) realStart = strstr(outBuf, "-1,");

    if (realStart != nullptr) {
        return (realStart[0] == '0');
    }

    return (outBuf[0] == '0');
}

bool RelayNetworkClient::clearRobotAlarm() {
    if (!sendToRelay(Config::ENABLE_PORT, CLEAR_ERROR_CMD)) {
        appendLog("[Alarm] ClearError send failed");
        return false;
    }

    Sleep(300);
    updateAlarmStatus();
    return !isRobotInAlarm;
}

void RelayNetworkClient::updateAlarmStatus() {
    if (!isTcpConnected) {
        isRobotInAlarm = false;
        return;
    }

    QMutexLocker transLocker(&m_transactionMutex);

    const char* cmd = "RobotMode()";
    SOCKET sock;

    {
        QMutexLocker locker(&relaySocketMutex);
        sock = relaySocket;
    }

    if (sock == INVALID_SOCKET) {
        isRobotInAlarm = true;
        return;
    }

    char buffer[512];
    int len = sprintf_s(buffer, sizeof(buffer), "%d|%s", Config::ENABLE_PORT, cmd);
    send(sock, buffer, len, 0);

    char feedbackBuf[1024];
    bool checkResult = readFeedbackAndCheck(sock, "Relay Station", feedbackBuf, sizeof(feedbackBuf));

    if (checkResult) {
        char* valueStart = strstr(feedbackBuf, "{");
        char* valueEnd = strstr(feedbackBuf, "}");
        if (valueStart && valueEnd && valueStart < valueEnd) {
            int robotMode = atoi(valueStart + 1);
            isRobotInAlarm = (robotMode == 9);
            return;
        }
    }

    isRobotInAlarm = !checkResult;
}

bool RelayNetworkClient::getRobotCurrentPos() {
    if (!isTcpConnected || isClosing) {
        appendLog("[Init Error] Not connected to Relay, unable to get robot pose");
        return false;
    }

    QMutexLocker transLocker(&m_transactionMutex);

    const char* cmd = "GetPose()";
    char relayCmd[256];
    sprintf_s(relayCmd, sizeof(relayCmd), "%d|%s", Config::ENABLE_PORT, cmd);

    SOCKET currentSocket;
    {
        QMutexLocker locker(&relaySocketMutex);
        currentSocket = relaySocket;
    }

    if (currentSocket == INVALID_SOCKET) {
        appendLog("[Init Error] Socket invalid");
        return false;
    }

    int bytesSent = send(currentSocket, relayCmd, static_cast<int>(strlen(relayCmd)), 0);
    if (bytesSent == SOCKET_ERROR) {
        appendLog(QString("[Init Error] GetPose send failed: %1").arg(WSAGetLastError()));
        return false;
    }

    char feedbackBuf[1024];
    if (!readFeedbackAndCheck(currentSocket, "Relay Station", feedbackBuf, sizeof(feedbackBuf))) {
        appendLog("[Init Error] No valid pose feedback received");
        return false;
    }

    char* posStart = strstr(feedbackBuf, "{");
    char* posEnd = strstr(feedbackBuf, "}");
    if (!posStart || !posEnd || posStart >= posEnd) {
        appendLog("[Init Error] Pose format error");
        return false;
    }

    *posEnd = '\0';
    int parseCount = sscanf_s(
        posStart + 1, "%lf,%lf,%lf,%lf,%lf,%lf",
        &robotBaseX, &robotBaseY, &robotBaseZ,
        &robotBaseRx, &robotBaseRy, &robotBaseRz
    );

    if (parseCount != 6) {
        appendLog("[Init Error] Pose parsing failed");
        return false;
    }

    isRobotBaseSet = true;
    appendLog(QString("[Init Success] Base Pose: (%1, %2, %3)")
        .arg(robotBaseX, 0, 'f', 2)
        .arg(robotBaseY, 0, 'f', 2)
        .arg(robotBaseZ, 0, 'f', 2));
    return true;
}

bool RelayNetworkClient::sendCpCommand(unsigned int smoothRatio) {
    if (smoothRatio > 100) {
        appendLog("[Init Error] CP parameter must be between 0~100");
        return false;
    }

    if (!isTcpConnected || isClosing) {
        appendLog("[Init Error] Not connected to Relay, unable to send CP");
        return false;
    }

    char cpCmd[64];
    sprintf_s(cpCmd, sizeof(cpCmd), "CP(%d)", smoothRatio);

    bool ok = sendToRelay(Config::ENABLE_PORT, cpCmd);
    if (ok) {
        isCpSet = true;
        appendLog(QString("[Init] CP(%1) set successfully").arg(smoothRatio));
    }
    return ok;
}

bool RelayNetworkClient::sendCoordinates(double x, double y, double z) {
    updateAlarmStatus();
    Sleep(100);

    if (isRobotInAlarm) {
        appendLog("[Motion] Robot in alarm, preparing to clear alarm");
        if (!clearRobotAlarm()) return false;

        if (!sendToRelay(Config::ENABLE_PORT, ENABLE_CMD)) {
            appendLog("[Motion] Re-enable failed");
            return false;
        }
        else {
            appendLog("[Motion] Robot re-enabled successfully");
            return false;
        }
    }

    if (isClosing) {
        appendLog("[Motion] Program closing, stopped sending");
        return false;
    }
    if (!isTcpConnected) {
        appendLog("[Motion] Not connected to Relay Station");
        return false;
    }
    if (!isRobotBaseSet) {
        appendLog("[Motion] Robot base position not set");
        return false;
    }

    double targetX = robotBaseX + x;
    double targetY = robotBaseY + y;
    double targetZ = robotBaseZ + z;
    double targetRx = robotBaseRx;
    double targetRy = robotBaseRy;
    double targetRz = robotBaseRz;

    char motionCmd[256];
    int cmdLen = sprintf_s(motionCmd, sizeof(motionCmd),
        REALTIME_MOVE_CMD,
        targetX, targetY, targetZ,
        targetRx, targetRy, targetRz,
        Config::SpeedL);

    if (cmdLen <= 0 || cmdLen >= static_cast<int>(sizeof(motionCmd))) {
        appendLog("[Motion] MovL command formatting failed");
        return false;
    }

    bool sendSuccess = sendToRelay(Config::MOTION_PORT, motionCmd);
    if (!sendSuccess) {
        appendLog("[Motion] MovL send failed");
        return false;
    }

    appendLog(QString("[Motion Success] %1").arg(motionCmd));
    return true;
}

void RelayNetworkClient::startRobotInitialization() {
    if (!isConnected()) {
        emit initializationFinished(false);
        return;
    }

    if (!clearRobotAlarm()) {
        appendLog("[Init] Failed to clear alarm");
        emit initializationFinished(false);
        return;
    }

    if (!sendToRelay(Config::ENABLE_PORT, ENABLE_CMD)) {
        appendLog("[Init] Robot enable failed");
        emit initializationFinished(false);
        return;
    }
    else {
        appendLog("[Init] Robot enabled successfully");
    }

    if (!sendCpCommand(Config::CP_SMOOTH_RATIO)) {
        appendLog("[Init] Failed to set CP");
        emit initializationFinished(false);
        return;
    }

    if (!getRobotCurrentPos()) {
        appendLog("[Init] Failed to get base pose");
        emit initializationFinished(false);
        return;
    }

    emit initializationFinished(true);
}

void RelayNetworkClient::sendMoveCommand(double dx, double dy, double dz) {
    if (!isConnected() || !isRobotBaseSet) return;

    QMutexLocker locker(&queueMutex);

    if (sendQueue.size() >= Config::MAX_QUEUE_SIZE) {
        sendQueue.dequeue();
    }

    SendData data;
    data.deltaX = static_cast<float>(dx);
    data.deltaY = static_cast<float>(dy);
    data.deltaZ = static_cast<float>(dz);
    sendQueue.enqueue(data);
}

void RelayNetworkClient::connectTwin() {
    m_twinEnabled = true;
    sendTwinHeartbeat();
    if (m_twinHeartbeatTimer) {
        m_twinHeartbeatTimer->start(2000);
    }
}

void RelayNetworkClient::disconnectTwin() {
    m_twinEnabled = false;
    if (m_twinHeartbeatTimer) {
        m_twinHeartbeatTimer->stop();
    }
    if (!m_relayIp.isEmpty() && m_udpTwinSocket) {
        m_udpTwinSocket->writeDatagram("TWIN_STOP", QHostAddress(m_relayIp), Config::STATUS_PORT);
    }
}

void RelayNetworkClient::sendTwinHeartbeat() {
    if (m_twinEnabled && !m_relayIp.isEmpty() && m_udpTwinSocket) {
        m_udpTwinSocket->writeDatagram("TWIN_START", QHostAddress(m_relayIp), Config::STATUS_PORT);
    }
}

void RelayNetworkClient::onTwinUdpReadyRead() {
    while (m_udpTwinSocket->hasPendingDatagrams()) {
        QByteArray datagram;
        datagram.resize(int(m_udpTwinSocket->pendingDatagramSize()));
        m_udpTwinSocket->readDatagram(datagram.data(), datagram.size());

        QString oneLine = QString::fromUtf8(datagram).trimmed();

        if (oneLine.startsWith("TWIN:")) {
            int pipeIndex = oneLine.indexOf('|');
            if (pipeIndex != -1) {
                QByteArray jointPart = oneLine.mid(pipeIndex + 1).toUtf8();
                QList<QByteArray> joints = jointPart.split(',');

                if (joints.size() == 6) {
                    double j1 = joints[0].toDouble();
                    double j2 = joints[1].toDouble();
                    double j3 = joints[2].toDouble();
                    double j4 = joints[3].toDouble();
                    double j5 = joints[4].toDouble();
                    double j6 = joints[5].toDouble();

                    emit twinDataReceived(j1, j2, j3, j4, j5, j6);
                }
            }
        }
    }
}