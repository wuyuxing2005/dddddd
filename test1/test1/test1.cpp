#include "test1.h"
#include <QDebug> 
#include <QNetworkProxy>

// =========================================================
// 🚀 PingWorker 核心实现：彻底剥离 UI 干扰
// =========================================================
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

// =========================================================
// 🚀 test1 主类实现 
// =========================================================
test1::test1(QWidget* parent)
    : QMainWindow(parent)
{
    ui.setupUi(this);
    initProcessLatencyPlot();
    ui.group_network_latency->setChecked(false);

    QComboBox* comboProtocol = ui.group_network_latency->findChild<QComboBox*>("combo_ping_protocol");
    if (ui.combo_payload_type && comboProtocol) {
        connect(ui.combo_payload_type, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this, comboProtocol](int index) {
            m_networkLatencyHistory.clear();
            if (index == 2) comboProtocol->setCurrentIndex(0);
            if (m_worker) m_worker->currentPayloadIndex = index;
            });
        connect(comboProtocol, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int index) {
            m_networkLatencyHistory.clear();
            if (index == 1 && ui.combo_payload_type->currentIndex() == 2) {
                ui.combo_payload_type->setCurrentIndex(1);
            }
            });
    }
    else {
        connect(ui.combo_payload_type, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int index) {
            m_networkLatencyHistory.clear();
            if (m_worker) m_worker->currentPayloadIndex = index;
            });
    }

    m_netTestTimer.start();
    m_netTimer = new QTimer(this);
    static qint64 lastPingTimeMs = 0;

    connect(m_netTimer, &QTimer::timeout, this, [this]() {
        if (m_isWaitingPONG && (m_netTestTimer.elapsed() - lastPingTimeMs) > 3000) {
            m_isWaitingPONG = false;
        }

        if (ui.group_network_latency->isChecked() && !m_isWaitingPONG && m_worker) {
            QComboBox* protoCombo = ui.group_network_latency->findChild<QComboBox*>("combo_ping_protocol");
            bool isUdp = protoCombo ? (protoCombo->currentIndex() == 1) : false;

            int payloadSize = 32;
            if (ui.combo_payload_type) {
                int index = ui.combo_payload_type->currentIndex();
                if (index == 1) payloadSize = 1024 * 50;
                else if (index == 2) payloadSize = 1024 * 1024 * 3;
            }

            if (isUdp && payloadSize > 60000) payloadSize = 60000;

            m_isWaitingPONG = true;
            lastPingTimeMs = m_netTestTimer.elapsed();

            QMetaObject::invokeMethod(m_worker, "sendPing", Qt::QueuedConnection,
                Q_ARG(int, payloadSize), Q_ARG(bool, isUdp));
        }
        });

    ui.plot_network_latency->setBackground(QBrush(QColor("#1B212D")));
    QPen axisPen(QColor("#E2E8F0"));
    ui.plot_network_latency->xAxis->setBasePen(axisPen);
    ui.plot_network_latency->yAxis->setBasePen(axisPen);
    ui.plot_network_latency->xAxis->setTickPen(axisPen);
    ui.plot_network_latency->yAxis->setTickPen(axisPen);
    ui.plot_network_latency->xAxis->setSubTickPen(axisPen);
    ui.plot_network_latency->yAxis->setSubTickPen(axisPen);
    ui.plot_network_latency->xAxis->setTickLabelColor(QColor("#E2E8F0"));
    ui.plot_network_latency->yAxis->setTickLabelColor(QColor("#E2E8F0"));
    ui.plot_network_latency->xAxis->setLabelColor(QColor("#E2E8F0"));
    ui.plot_network_latency->yAxis->setLabelColor(QColor("#E2E8F0"));
    ui.plot_network_latency->xAxis->grid()->setPen(QPen(QColor("#31394E"), 1, Qt::DotLine));
    ui.plot_network_latency->yAxis->grid()->setPen(QPen(QColor("#31394E"), 1, Qt::DotLine));
    ui.plot_network_latency->xAxis->grid()->setZeroLinePen(Qt::NoPen);
    ui.plot_network_latency->yAxis->grid()->setZeroLinePen(Qt::NoPen);

    ui.plot_network_latency->addGraph();
    QPen netPen(QColor("#F15BB5"));
    netPen.setWidth(2);
    ui.plot_network_latency->graph(0)->setPen(netPen);
    ui.plot_network_latency->xAxis->setRange(0, 100);
    ui.plot_network_latency->xAxis->setLabel("Network Count");
    ui.plot_network_latency->yAxis->setLabel("Latency (ms)");
    ui.plot_network_latency->legend->setVisible(false);

    QFont axisLabelFont("Segoe UI", 12, QFont::Bold);
    QFont tickLabelFont("Segoe UI", 10);
    ui.plot_network_latency->xAxis->setLabelFont(axisLabelFont);
    ui.plot_network_latency->yAxis->setLabelFont(axisLabelFont);
    ui.plot_network_latency->xAxis->setTickLabelFont(tickLabelFont);
    ui.plot_network_latency->yAxis->setTickLabelFont(tickLabelFont);

    // ==========================================================
    // 🚀 核心机制 1：解除强制互斥，实现点击暂停功能
    // ==========================================================
    ui.chk_show_control->setAutoExclusive(false);
    ui.chk_show_digitaltwin->setAutoExclusive(false);
    ui.chk_show_video->setAutoExclusive(false);

    // 🚀 核心机制 2：动态图例与【历史视野秒切】机制
    auto updatePlotVisibility = [this]() {
        bool ctrl = ui.chk_show_control->isChecked();
        bool twin = ui.chk_show_digitaltwin->isChecked();
        bool vid = ui.chk_show_video->isChecked();

        // 冻结状态：全都没选中，保持画面和视野不变
        if (!ctrl && !twin && !vid) return;

        // 设置线条可见性
        ui.plot_relay_latency->graph(0)->setVisible(ctrl);
        ui.plot_relay_latency->graph(3)->setVisible(ctrl);
        ui.plot_relay_latency->graph(1)->setVisible(twin);
        ui.plot_relay_latency->graph(2)->setVisible(vid);

        // 动态踢出/加入图例
        if (ctrl) {
            ui.plot_relay_latency->graph(0)->addToLegend();
            ui.plot_relay_latency->graph(3)->addToLegend();
            ui.plot_relay_latency->graph(1)->removeFromLegend();
            ui.plot_relay_latency->graph(2)->removeFromLegend();
        }
        else if (twin) {
            ui.plot_relay_latency->graph(1)->addToLegend();
            ui.plot_relay_latency->graph(0)->removeFromLegend();
            ui.plot_relay_latency->graph(2)->removeFromLegend();
            ui.plot_relay_latency->graph(3)->removeFromLegend();
        }
        else if (vid) {
            ui.plot_relay_latency->graph(2)->addToLegend();
            ui.plot_relay_latency->graph(0)->removeFromLegend();
            ui.plot_relay_latency->graph(1)->removeFromLegend();
            ui.plot_relay_latency->graph(3)->removeFromLegend();
        }

        // 🚀【本次修复重点】：切频道时，瞬间把坐标轴推回该频道历史数据的视野！
        int refX = 0;
        double maxInWindow = 0.15;
        double sum = 0;
        int count = 0;

        if (ctrl) {
            refX = m_packetCountControl;
            for (double v : m_latencyHistory[0]) { if (v * 2.0 > maxInWindow) maxInWindow = v * 2.0; sum += v; count++; }
            for (double v : m_latencyHistory[3]) { if (v * 2.0 > maxInWindow) maxInWindow = v * 2.0; }
        }
        else if (twin) {
            refX = m_packetCountTwin;
            for (double v : m_latencyHistory[1]) { if (v * 2.0 > maxInWindow) maxInWindow = v * 2.0; sum += v; count++; }
        }
        else if (vid) {
            refX = m_packetCountVideo;
            for (double v : m_latencyHistory[2]) { if (v * 2.0 > maxInWindow) maxInWindow = v * 2.0; sum += v; count++; }
        }

        // 瞬间恢复文本框的平均值
        if (count > 0) {
            ui.avg_relay_latency->setText(QString::number(sum / count, 'f', 3));
        }
        else {
            ui.avg_relay_latency->clear();
        }

        // 瞬间恢复 Y 轴峰值
        ui.plot_relay_latency->yAxis->setRange(0, maxInWindow);

        // 瞬间拉回 X 轴视野，不再需要等新包
        if (refX > 100) ui.plot_relay_latency->xAxis->setRange(refX - 100, refX);
        else ui.plot_relay_latency->xAxis->setRange(0, 100);

        ui.plot_relay_latency->replot();
        };

    // 绑定点击事件
    connect(ui.chk_show_control, &QAbstractButton::clicked, this, [=](bool checked) {
        if (checked) {
            ui.chk_show_digitaltwin->setChecked(false);
            ui.chk_show_video->setChecked(false);
            updatePlotVisibility();
        }
        });

    connect(ui.chk_show_digitaltwin, &QAbstractButton::clicked, this, [=](bool checked) {
        if (checked) {
            ui.chk_show_control->setChecked(false);
            ui.chk_show_video->setChecked(false);
            updatePlotVisibility();
        }
        });

    connect(ui.chk_show_video, &QAbstractButton::clicked, this, [=](bool checked) {
        if (checked) {
            ui.chk_show_control->setChecked(false);
            ui.chk_show_digitaltwin->setChecked(false);
            updatePlotVisibility();
        }
        });

    // 默认开启控制流
    ui.chk_show_control->setChecked(true);

    m_server = new QTcpServer(this);
    m_client = nullptr;

    m_udpTwinSocket = new QUdpSocket(this);
    connect(m_udpTwinSocket, &QUdpSocket::readyRead, this, &test1::onUdpTwinReadyRead);

    m_twinHeartbeatTimer = new QTimer(this);
    m_twinHeartbeatTimer->setSingleShot(true);
    connect(m_twinHeartbeatTimer, &QTimer::timeout, this, [this]() {
        ui.txt_state_log->appendPlainText(">> [Twin Stream] UDP hole punch timeout, stopping transmission.");
        m_twinTargetIp.clear();

        if (ui.robot_30004_status) {
            ui.robot_30004_status->setText("Timeout/Stopped");
            ui.robot_30004_status->setStyleSheet("color: #FFD166;");
        }
        });

    m_robotStatusSocket = new QTcpSocket(this);
    m_robotStatusSocket->setProxy(QNetworkProxy::NoProxy);
    connect(m_robotStatusSocket, &QTcpSocket::readyRead, this, &test1::onRobotStatusReadyRead);

    connect(m_robotStatusSocket, &QTcpSocket::connected, this, [this]() {
        ui.txt_state_log->appendPlainText(">> Robot status port 30004 connected.");
        ui.LED_Robot->setStyleSheet("QLabel { background-color: #00FF00; border-radius: 8px; border: 1px solid #00AA00; }");
        });

    connect(m_robotStatusSocket, &QTcpSocket::errorOccurred, this, [this](QTcpSocket::SocketError socketError) {
        ui.txt_state_log->appendPlainText(QString(">> [Error] Robot port 30004 disconnected or failed: %1").arg(m_robotStatusSocket->errorString()));
        ui.LED_Robot->setStyleSheet("QLabel { background-color: #FF0000; border-radius: 8px; border: 1px solid #AA0000; }");
        });

    connect(m_server, &QTcpServer::newConnection, this, [this]() {
        if (this->m_client) {
            this->m_client->disconnectFromHost();
            this->m_client->deleteLater();
        }
        this->m_client = m_server->nextPendingConnection();

        ui.LED_Touch->setStyleSheet("QLabel { background-color: #00FF00; border-radius: 8px; border: 1px solid #00AA00; }");
        ui.txt_state_log->appendPlainText(">> [Connected] Touch client has joined the relay station!");

        connect(this->m_client, &QTcpSocket::disconnected, this, [this]() {
            ui.LED_Touch->setStyleSheet("QLabel { background-color: #FF0000; border-radius: 8px; border: 1px solid #AA0000; }");
            ui.txt_state_log->appendPlainText(">> [Disconnected] Touch client lost!");
            if (m_videoThread) m_videoThread->stopStreaming();
            if (this->m_client) {
                this->m_client->deleteLater();
                this->m_client = nullptr;
            }
            });

        connect(this->m_client, &QTcpSocket::errorOccurred, this, [this](QTcpSocket::SocketError socketError) {
            if (socketError != QTcpSocket::RemoteHostClosedError) {
                ui.txt_state_log->appendPlainText(">> [Error] Touch connection abnormally terminated!");
                ui.LED_Touch->setStyleSheet("QLabel { background-color: #FF0000; border-radius: 8px; border: 1px solid #AA0000; }");
            }
            });

        connect(this->m_client, &QTcpSocket::readyRead, this, [this]() {
            QElapsedTimer processTimer;
            processTimer.start();
            QByteArray data = this->m_client->readAll();

            int idx30003 = data.indexOf("30003|");
            int idx29999 = data.indexOf("29999|");
            int targetIdx = (idx30003 != -1 && idx29999 != -1) ? qMax(idx30003, idx29999) : qMax(idx30003, idx29999);

            if (targetIdx != -1) {
                QByteArray cmdData = data.mid(targetIdx);
                int separatorIdx = cmdData.indexOf('|');
                if (separatorIdx != -1) {
                    int targetPort = cmdData.left(separatorIdx).toInt();
                    QByteArray cmdBody = cmdData.mid(separatorIdx + 1);

                    if (targetPort == 29999 && m_robot_enable && m_robot_enable->isOpen()) {
                        m_robot_enable->write(cmdBody);
                    }
                    else if (targetPort == 30003 && m_robot_motion && m_robot_motion->isOpen()) {
                        m_robot_motion->write(cmdBody);
                    }

                    // 👉 通道 0：控制流
                    double relayLatencyMs = processTimer.nsecsElapsed() / 1000000.0;
                    updateProcessLatencyUI(0, relayLatencyMs);

                    QString cmdText = QString::fromUtf8(cmdBody);
                    ui.txt_control_log->appendPlainText(QString(">> Route to %1: %2").arg(targetPort).arg(cmdText));
                }
            }
            });
        });

    m_statusTimeoutTimer = new QTimer(this);
    m_statusTimeoutTimer->setSingleShot(true);

    connect(m_statusTimeoutTimer, &QTimer::timeout, this, [this]() {
        if (ui.robot_30004_status) {
            ui.robot_30004_status->setText("Robot Data Lost");
            ui.robot_30004_status->setStyleSheet("color: #FF3B30;");
        }
        });

    m_udpCommandSocket = new QUdpSocket(this);
    m_udpCommandSocket->bind(8886, QUdpSocket::ShareAddress);
    connect(m_udpCommandSocket, &QUdpSocket::readyRead, this, &test1::onUdpCommandReadyRead);

    m_videoHeartbeatTimer = new QTimer(this);
    m_videoHeartbeatTimer->setSingleShot(true);
    connect(m_videoHeartbeatTimer, &QTimer::timeout, this, [this]() {
        if (m_videoThread) m_videoThread->stopStreaming();
        if (ui.relay_video_status) {
            ui.relay_video_status->setText("Timeout/Stopped");
            ui.relay_video_status->setStyleSheet("color: #FFD166;");
        }
        });
}

test1::~test1() {}

void test1::on_btn_start_clicked() {
    QString robot_ip = ui.edit_robot_ip->text();
    int motion_port = ui.edit_motion_port->text().toInt();
    int enable_port = ui.edit_enable_port->text().toInt();
    int relay_port = ui.edit_relay_port_control->text().toInt();
    int digitaltwin_port = ui.edit_relay_port_digitaltwin->text().toInt();
    int realtime_port = ui.edit_realtime_port->text().toInt();

    ui.txt_state_log->appendPlainText("===== Starting Embodied AI Relay Station =====");

    if (!m_latencyThread) {
        m_latencyThread = new QThread();
        m_worker = new PingWorker();
        m_worker->timer = &m_netTestTimer;
        if (ui.combo_payload_type) m_worker->currentPayloadIndex = ui.combo_payload_type->currentIndex();

        m_worker->moveToThread(m_latencyThread);

        connect(m_latencyThread, &QThread::started, m_worker, &PingWorker::setup);
        connect(m_worker, &PingWorker::resultReady, this, &test1::updateLatencyFromWorker);
        connect(m_worker, &PingWorker::logToUi, ui.txt_state_log, &QPlainTextEdit::appendPlainText);
        connect(m_worker, &PingWorker::pongReceived, this, [this]() { m_isWaitingPONG = false; });

        m_latencyThread->start();
    }

    if (!m_server->isListening()) {
        if (m_server->listen(QHostAddress::Any, relay_port)) {
            ui.txt_state_log->appendPlainText(QString(">> Relay control server started, port: %1").arg(relay_port));
        }
    }

    if (m_udpTwinSocket->state() != QAbstractSocket::BoundState) {
        if (m_udpTwinSocket->bind(QHostAddress::Any, digitaltwin_port, QUdpSocket::ShareAddress)) {
            ui.txt_state_log->appendPlainText(QString(">> Twin UDP server started, port: %1").arg(digitaltwin_port));
        }
    }

    initRobotConnections(robot_ip, motion_port, enable_port, realtime_port);

    ui.btn_start->setEnabled(false);
    m_netTimer->start(500);
}

void test1::initRobotConnections(QString ip, int motionPort, int enablePort, int realtimePort) {
    m_robot_motion = new QTcpSocket(this);
    m_robot_motion->setProxy(QNetworkProxy::NoProxy);
    m_robot_enable = new QTcpSocket(this);
    m_robot_enable->setProxy(QNetworkProxy::NoProxy);

    connect(m_robot_motion, &QTcpSocket::connected, this, [=]() {
        ui.txt_state_log->appendPlainText(QString(">> Robot motion port %1 connected").arg(motionPort));
        ui.LED_Robot->setStyleSheet("QLabel { background-color: #00FF00; border-radius: 8px; border: 1px solid #00AA00; }");
        });

    connect(m_robot_motion, &QTcpSocket::errorOccurred, this, [=](QTcpSocket::SocketError socketError) {
        ui.txt_state_log->appendPlainText(QString(">> [Error] Motion Port 30003 Failed: %1").arg(m_robot_motion->errorString()));
        ui.LED_Robot->setStyleSheet("QLabel { background-color: #FF0000; border-radius: 8px; border: 1px solid #AA0000; }");
        });

    connect(m_robot_motion, &QTcpSocket::readyRead, this, [=]() {
        QElapsedTimer processTimer;
        processTimer.start();
        QByteArray data = m_robot_motion->readAll();

        if (m_client && m_client->isOpen()) m_client->write(data);

        // 👉 通道 3：机器人向上传递的控制反馈
        double relayLatencyMs = processTimer.nsecsElapsed() / 1000000.0;
        updateProcessLatencyUI(3, relayLatencyMs);
        ui.txt_feedback_log->appendPlainText(">> Robot 30003 to Relay: " + data);
        });

    m_robot_motion->connectToHost(ip, motionPort);

    connect(m_robot_enable, &QTcpSocket::connected, this, [=]() {
        ui.txt_state_log->appendPlainText(QString(">> Robot enable port %1 connected").arg(enablePort));
        });

    connect(m_robot_enable, &QTcpSocket::errorOccurred, this, [=](QTcpSocket::SocketError socketError) {
        ui.txt_state_log->appendPlainText(QString(">> [Error] Enable Port 29999 Failed: %1").arg(m_robot_enable->errorString()));
        });

    connect(m_robot_enable, &QTcpSocket::readyRead, this, [=]() {
        QElapsedTimer processTimer;
        processTimer.start();
        QByteArray data = m_robot_enable->readAll();

        if (m_client && m_client->isOpen()) m_client->write(data);

        // 👉 通道 3：机器人向上传递的控制反馈
        double relayLatencyMs = processTimer.nsecsElapsed() / 1000000.0;
        updateProcessLatencyUI(3, relayLatencyMs);
        ui.txt_feedback_log->appendPlainText(">> Robot 29999 to Relay: " + data);
        });

    m_robot_enable->connectToHost(ip, enablePort);
    m_robotStatusSocket->connectToHost(ip, 30004);
}

void test1::on_btn_stop_clicked() {
    if (m_netTimer && m_netTimer->isActive()) m_netTimer->stop();
    m_isWaitingPONG = false;

    ui.txt_state_log->appendPlainText("===== Stopping Embodied AI Relay Station =====");

    if (m_latencyThread) {
        QMetaObject::invokeMethod(m_worker, "stopAll", Qt::BlockingQueuedConnection);
        m_latencyThread->quit();
        m_latencyThread->wait();
        delete m_worker; delete m_latencyThread;
        m_worker = nullptr; m_latencyThread = nullptr;
    }

    if (m_server->isListening()) m_server->close();
    if (m_client && m_client->isOpen()) m_client->disconnectFromHost();

    if (m_twinHeartbeatTimer && m_twinHeartbeatTimer->isActive()) m_twinHeartbeatTimer->stop();
    m_twinTargetIp.clear();

    if (m_statusTimeoutTimer && m_statusTimeoutTimer->isActive()) m_statusTimeoutTimer->stop();
    if (m_videoHeartbeatTimer && m_videoHeartbeatTimer->isActive()) m_videoHeartbeatTimer->stop();

    if (ui.relay_video_status) {
        ui.relay_video_status->setText("Closed");
        ui.relay_video_status->setStyleSheet("color: #A0A0A0;");
    }

    if (ui.robot_30004_status) {
        ui.robot_30004_status->setText("Closed");
        ui.robot_30004_status->setStyleSheet("color: #A0A0A0;");
    }

    if (m_robotStatusSocket && m_robotStatusSocket->isOpen()) m_robotStatusSocket->disconnectFromHost();
    if (m_robot_motion && m_robot_motion->isOpen()) m_robot_motion->disconnectFromHost();
    if (m_robot_enable && m_robot_enable->isOpen()) m_robot_enable->disconnectFromHost();

    if (m_videoThread) m_videoThread->stopStreaming();

    ui.LED_Touch->setStyleSheet("QLabel { background-color: #A0A0A0; border-radius: 8px; border: 1px solid #888888; }");
    ui.LED_Robot->setStyleSheet("QLabel { background-color: #A0A0A0; border-radius: 8px; border: 1px solid #888888; }");

    // 🚀 【新增】：全局停止时，灯也要变灰
    ui.LED_Camera->setStyleSheet("QLabel { background-color: #A0A0A0; border-radius: 8px; border: 1px solid #888888; }");

    ui.btn_start->setEnabled(true);
    ui.txt_state_log->appendPlainText("===== Relay Station Fully Closed =====");
}

void test1::updateLatencyFromWorker(double latencyMs) {
    updateNetworkLatencyUI(latencyMs);
}

void test1::onUdpTwinReadyRead() {
    while (m_udpTwinSocket->hasPendingDatagrams()) {
        QByteArray datagram;
        datagram.resize(int(m_udpTwinSocket->pendingDatagramSize()));
        QHostAddress senderIp;
        quint16 senderPort;
        m_udpTwinSocket->readDatagram(datagram.data(), datagram.size(), &senderIp, &senderPort);

        if (!m_server || !m_server->isListening()) continue;
        if (senderIp.protocol() == QAbstractSocket::IPv6Protocol) senderIp = QHostAddress(senderIp.toIPv4Address());

        QString cmd = QString::fromUtf8(datagram).trimmed();
        if (cmd == "TWIN_START") {
            if (m_twinTargetIp != senderIp || m_twinTargetPort != senderPort) {
                ui.txt_state_log->appendPlainText(QString(">> [Twin UDP successful] Mapped address: %1:%2").arg(senderIp.toString()).arg(senderPort));
            }
            m_twinTargetIp = senderIp;
            m_twinTargetPort = senderPort;
            m_twinHeartbeatTimer->start(5000);

            if (ui.robot_30004_status) {
                ui.robot_30004_status->setText("UDP Streaming");
                ui.robot_30004_status->setStyleSheet("color: #FFD166;");
            }
        }
        else if (cmd == "TWIN_STOP") {
            m_twinHeartbeatTimer->stop();
            m_twinTargetIp.clear();
            ui.txt_state_log->appendPlainText(">> [Twin Stream] Touch triggered STOP. Ceasing data transmission.");

            if (ui.robot_30004_status) {
                ui.robot_30004_status->setText("Stopped");
                ui.robot_30004_status->setStyleSheet("color: #A0A0A0;");
            }
        }
    }
}

void test1::onRobotStatusReadyRead() {
    m_statusBuffer.append(m_robotStatusSocket->readAll());

    while (m_statusBuffer.size() >= 1440) {
        QElapsedTimer processTimer;
        processTimer.start();

        QByteArray packet = m_statusBuffer.left(1440);
        m_statusBuffer.remove(0, 1440);

        DobotRealTimeData* data = reinterpret_cast<DobotRealTimeData*>(packet.data());
        double* pose = data->toolVectorActual;
        double* joint = data->qActual;

        QString twinStr = QString("TWIN:%1,%2,%3,%4,%5,%6|%7,%8,%9,%10,%11,%12\n")
            .arg(pose[0], 0, 'f', 2).arg(pose[1], 0, 'f', 2).arg(pose[2], 0, 'f', 2)
            .arg(pose[3], 0, 'f', 2).arg(pose[4], 0, 'f', 2).arg(pose[5], 0, 'f', 2)
            .arg(joint[0], 0, 'f', 2).arg(joint[1], 0, 'f', 2).arg(joint[2], 0, 'f', 2)
            .arg(joint[3], 0, 'f', 2).arg(joint[4], 0, 'f', 2).arg(joint[5], 0, 'f', 2);

        m_statusTimeoutTimer->start(500);

        if (!m_twinTargetIp.isNull() && m_udpTwinSocket) {
            m_udpTwinSocket->writeDatagram(twinStr.toUtf8(), m_twinTargetIp, m_twinTargetPort);
        }

        // 👉 通道 1：Digital Twin 时延统计
        double relayLatencyMs = processTimer.nsecsElapsed() / 1000000.0;
        updateProcessLatencyUI(1, relayLatencyMs);
    }
}

void test1::onUdpCommandReadyRead() {
    while (m_udpCommandSocket->hasPendingDatagrams()) {
        QByteArray datagram;
        datagram.resize(int(m_udpCommandSocket->pendingDatagramSize()));
        QHostAddress senderIp;
        quint16 senderPort;

        m_udpCommandSocket->readDatagram(datagram.data(), datagram.size(), &senderIp, &senderPort);
        if (!m_server || !m_server->isListening()) continue;

        if (senderIp.protocol() == QAbstractSocket::IPv6Protocol) senderIp = QHostAddress(senderIp.toIPv4Address());

        QString cmd = QString::fromUtf8(datagram).trimmed();
        if (cmd == "VIDEO_START") {
            static QHostAddress lastVideoIp;
            static quint16 lastVideoPort = 0;
            if (lastVideoIp != senderIp || lastVideoPort != senderPort || !m_videoHeartbeatTimer->isActive()) {
                ui.txt_state_log->appendPlainText(QString(">> [Video UDP successful] Mapped address: %1:%2").arg(senderIp.toString()).arg(senderPort));
                lastVideoIp = senderIp;
                lastVideoPort = senderPort;
            }

            if (ui.relay_video_status) {
                ui.relay_video_status->setText("UDP Streaming");
                ui.relay_video_status->setStyleSheet("color: #FFD166;");
            }

            if (m_videoThread == nullptr) {
                m_videoThread = new VideoStreamThread(this);
                connect(m_videoThread, &VideoStreamThread::frameEncoded, this, [this](const QByteArray& data, const QHostAddress& ip, int port, double latencyMs) {
                    if (m_udpCommandSocket) m_udpCommandSocket->writeDatagram(data, ip, port);
                    updateProcessLatencyUI(2, latencyMs); // 👉 通道 2：视频时延
                    });


                // 🚀 【新增】：接收摄像头状态信号，控制 LED 灯！
                connect(m_videoThread, &VideoStreamThread::cameraStatusChanged, this, [this](bool isOpen) {
                    if (isOpen) {
                        ui.LED_Camera->setStyleSheet("QLabel { background-color: #00FF00; border-radius: 8px; border: 1px solid #00AA00; }");
                        ui.txt_state_log->appendPlainText(">> [Video Stream] Camera Hardware activated.");
                    }
                    else {
                        ui.LED_Camera->setStyleSheet("QLabel { background-color: #FF0000; border-radius: 8px; border: 1px solid #AA0000; }");
                        ui.txt_state_log->appendPlainText(">> [Error] Camera Hardware failed to open!");
                    }
                    });
            }
            m_videoThread->startStreaming(senderIp, senderPort);
            m_videoHeartbeatTimer->start(5000);
        }
        else if (cmd == "VIDEO_STOP") {
            if (m_videoThread) m_videoThread->stopStreaming();
            m_videoHeartbeatTimer->stop();

            ui.txt_state_log->appendPlainText(">> [Video Stream] Touch triggered STOP. Ceasing video transmission.");

            if (ui.relay_video_status) {
                ui.relay_video_status->setText("Stopped");
                ui.relay_video_status->setStyleSheet("color: #A0A0A0;");
            }

            // 🚀 【新增】：关闭视频时灯变灰
            ui.LED_Camera->setStyleSheet("QLabel { background-color: #A0A0A0; border-radius: 8px; border: 1px solid #888888; }");
        }
    }
}

void test1::initProcessLatencyPlot() {
    ui.plot_relay_latency->setBackground(QBrush(QColor("#1B212D")));
    QPen whiteAxisPen(QColor("#E2E8F0"));
    ui.plot_relay_latency->xAxis->setBasePen(whiteAxisPen);
    ui.plot_relay_latency->xAxis->setTickPen(whiteAxisPen);
    ui.plot_relay_latency->xAxis->setSubTickPen(whiteAxisPen);
    ui.plot_relay_latency->xAxis->setTickLabelColor(QColor("#E2E8F0"));
    ui.plot_relay_latency->xAxis->setLabelColor(QColor("#E2E8F0"));
    ui.plot_relay_latency->yAxis->setBasePen(whiteAxisPen);
    ui.plot_relay_latency->yAxis->setTickPen(whiteAxisPen);
    ui.plot_relay_latency->yAxis->setSubTickPen(whiteAxisPen);
    ui.plot_relay_latency->yAxis->setTickLabelColor(QColor("#E2E8F0"));
    ui.plot_relay_latency->yAxis->setLabelColor(QColor("#E2E8F0"));
    ui.plot_relay_latency->xAxis->grid()->setPen(QPen(QColor("#31394E"), 1, Qt::DotLine));
    ui.plot_relay_latency->yAxis->grid()->setPen(QPen(QColor("#31394E"), 1, Qt::DotLine));
    ui.plot_relay_latency->legend->setBrush(QBrush(QColor(27, 33, 45, 220)));
    ui.plot_relay_latency->legend->setBorderPen(QPen(QColor("#454E65"), 1));
    ui.plot_relay_latency->legend->setTextColor(QColor("#E2E8F0"));
    ui.plot_relay_latency->xAxis->setLabel("Processing Count");
    ui.plot_relay_latency->yAxis->setLabel("Latency (ms)");

    QFont axisLabelFont("Segoe UI", 12, QFont::Bold);
    QFont tickLabelFont("Segoe UI", 10);
    ui.plot_relay_latency->xAxis->setLabelFont(axisLabelFont);
    ui.plot_relay_latency->yAxis->setLabelFont(axisLabelFont);
    ui.plot_relay_latency->xAxis->setTickLabelFont(tickLabelFont);
    ui.plot_relay_latency->yAxis->setTickLabelFont(tickLabelFont);

    ui.plot_relay_latency->addGraph();
    ui.plot_relay_latency->graph(0)->setName("Control (Touch -> Robot)");
    QPen penControl(QColor("#9D4EDD")); // Purple
    penControl.setWidth(2);
    ui.plot_relay_latency->graph(0)->setPen(penControl);

    ui.plot_relay_latency->addGraph();
    ui.plot_relay_latency->graph(1)->setName("Digital Twin");
    QPen penTwin(QColor("#00F5D4"));    // Mint Green
    penTwin.setWidth(2);
    ui.plot_relay_latency->graph(1)->setPen(penTwin);

    ui.plot_relay_latency->addGraph();
    ui.plot_relay_latency->graph(2)->setName("Video");
    QPen penVideo(QColor("#FEE440"));   // Yellow
    penVideo.setWidth(2);
    ui.plot_relay_latency->graph(2)->setPen(penVideo);

    ui.plot_relay_latency->addGraph();
    ui.plot_relay_latency->graph(3)->setName("Feedback (Robot -> Touch)");
    QPen penFeedback(QColor("#00D2FF")); // Light Blue
    penFeedback.setWidth(2);
    ui.plot_relay_latency->graph(3)->setPen(penFeedback);

    ui.plot_relay_latency->legend->setVisible(false); // 初始隐藏，由自动更新逻辑接管
    QFont legendFont("Segoe UI", 10);
    legendFont.setWeight(QFont::Medium);
    ui.plot_relay_latency->legend->setFont(legendFont);
    ui.plot_relay_latency->legend->setBrush(QBrush(QColor(27, 33, 45, 230)));
    ui.plot_relay_latency->legend->setBorderPen(QPen(QColor("#454E65"), 1));
    ui.plot_relay_latency->legend->setIconSize(35, 12);
    ui.plot_relay_latency->legend->setRowSpacing(2);
    ui.plot_relay_latency->legend->setColumnSpacing(8);
    ui.plot_relay_latency->legend->setMargins(QMargins(8, 6, 8, 6));
    ui.plot_relay_latency->axisRect()->insetLayout()->setInsetAlignment(0, Qt::AlignTop | Qt::AlignRight);

    // 默认 Control 状态的图例
    ui.plot_relay_latency->graph(1)->removeFromLegend();
    ui.plot_relay_latency->graph(2)->removeFromLegend();
    ui.plot_relay_latency->replot();
}

void test1::updateProcessLatencyUI(int channelIndex, double latencyMs) {
    // 🚀 核心机制 3：未选中时强制返回，既丢弃历史数据防后台累积，又实现了暂停冻结效果
    if (channelIndex == 0 || channelIndex == 3) {
        if (!ui.chk_show_control->isChecked()) return;
    }
    else if (channelIndex == 1) {
        if (!ui.chk_show_digitaltwin->isChecked()) return;
    }
    else if (channelIndex == 2) {
        if (!ui.chk_show_video->isChecked()) return;
    }

    m_latencyHistory[channelIndex].append(latencyMs);
    while (m_latencyHistory[channelIndex].size() > 20) {
        m_latencyHistory[channelIndex].removeFirst();
    }

    int currentX = 0;
    if (channelIndex == 0) currentX = ++m_packetCountControl;
    else if (channelIndex == 1) currentX = ++m_packetCountTwin;
    else if (channelIndex == 2) currentX = ++m_packetCountVideo;
    else if (channelIndex == 3) currentX = ++m_packetCountFeedback;

    ui.plot_relay_latency->graph(channelIndex)->addData(currentX, latencyMs);

    if (channelIndex == 0 || channelIndex == 1 || channelIndex == 2) {
        double sum = 0;
        for (double val : m_latencyHistory[channelIndex]) sum += val;
        double avg = (m_latencyHistory[channelIndex].size() > 0) ? (sum / m_latencyHistory[channelIndex].size()) : 0;
        ui.avg_relay_latency->setText(QString::number(avg, 'f', 3));
    }

    double maxInWindow = 0.15;
    if (ui.chk_show_control->isChecked()) {
        for (double v : m_latencyHistory[0]) if (v * 2.0 > maxInWindow) maxInWindow = v * 2.0;
        for (double v : m_latencyHistory[3]) if (v * 2.0 > maxInWindow) maxInWindow = v * 2.0;
    }
    else {
        for (double v : m_latencyHistory[channelIndex]) {
            if (v * 2.0 > maxInWindow) maxInWindow = v * 2.0;
        }
    }
    ui.plot_relay_latency->yAxis->setRange(0, maxInWindow);

    int refX = currentX;
    if (ui.chk_show_control->isChecked()) refX = m_packetCountControl;
    else if (ui.chk_show_digitaltwin->isChecked()) refX = m_packetCountTwin;
    else if (ui.chk_show_video->isChecked()) refX = m_packetCountVideo;

    if (refX > 100) ui.plot_relay_latency->xAxis->setRange(refX - 100, refX);
    else ui.plot_relay_latency->xAxis->setRange(0, 100);

    ui.plot_relay_latency->replot();
}

void test1::updateNetworkLatencyUI(double netLatency) {
    m_networkLatencyHistory.append(netLatency);
    while (m_networkLatencyHistory.size() > 20) {
        m_networkLatencyHistory.removeFirst();
    }
    int currentX = ++m_packetCountNetwork;
    ui.plot_network_latency->graph(0)->addData(currentX, netLatency);

    if (ui.group_network_latency->isChecked()) {
        double sum = 0;
        for (double val : m_networkLatencyHistory) sum += val;
        if (!m_networkLatencyHistory.isEmpty()) {
            double avg = sum / m_networkLatencyHistory.size();
            ui.avg_network_latency->setText(QString::number(avg, 'f', 3));
        }
        double maxInWindow = 5.0;
        for (double v : m_networkLatencyHistory) {
            if (v * 2.0 > maxInWindow) maxInWindow = v * 2.0;
        }
        ui.plot_network_latency->yAxis->setRange(0, maxInWindow);
        if (currentX > 100) {
            ui.plot_network_latency->xAxis->setRange(currentX - 100, currentX);
        }
        else {
            ui.plot_network_latency->xAxis->setRange(0, 100);
        }
        ui.plot_network_latency->replot();
    }
}