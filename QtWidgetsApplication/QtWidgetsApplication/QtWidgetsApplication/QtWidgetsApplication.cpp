// 数字孪生可以关掉，但是图像不能

#include "QtWidgetsApplication.h"
#include <QScrollBar>
#include <QMessageBox>
#include <cmath>
#include <QQmlContext>
#include "RobotTwinBackend.h"

// 【新增】工业级 3D 窗口需要的头文件
#include <QQuickView>
#include <QTimer> 
#include <QQmlApplicationEngine>
#include "VideoImageProvider.h"
#include "VideoStreamReceiver.h"

// test
QtWidgetsApplication::QtWidgetsApplication(QWidget* parent)
    : QWidget(parent), m_isControllingRobot(false) {
    ui.setupUi(this);

    // ==========================================================
    // --- 机械臂数字孪生体连线 (硬核原生版) ---
    // ==========================================================
    m_twinBackend = new RobotTwinBackend(this);

    // 🚀 【视窗一：3D 数字孪生】
    // 1. 创建原生的 QQuickView 独立窗口
    m_quickView = new QQuickView();
    m_quickView->setFormat(QSurfaceFormat::defaultFormat());
    m_quickView->setResizeMode(QQuickView::SizeRootObjectToView);

    // 2. 将独立窗口包装成一个真正的 QWidget 容器
    QWidget* containerTwin = QWidget::createWindowContainer(m_quickView, this);
    containerTwin->setFocusPolicy(Qt::StrongFocus); // 确保接收焦点

    // 3. 把新容器塞进你 UI 里预留的 groupBox_Twin 中
    if (ui.groupBox_Twin->layout()) {
        ui.groupBox_Twin->layout()->addWidget(containerTwin);
    }

    // ==========================================================
    // 🚀 【视窗二：2D 视频监控】 (完全独立的第二引擎)
    // ==========================================================
    // 1. 创建二号独立窗口
    m_videoView = new QQuickView();
    m_videoView->setResizeMode(QQuickView::SizeRootObjectToView);
    QWidget* containerVideo = QWidget::createWindowContainer(m_videoView, this);

    // 2. 塞进你 UI 里专属的视频布局中
    if (ui.verticalLayout_video) {
        ui.verticalLayout_video->addWidget(containerVideo);
    }

    // 3. 创建图像提供者和 UDP 视频接收器 (绑定到 m_videoView)
    VideoImageProvider* videoProvider = new VideoImageProvider();
    m_videoView->engine()->addImageProvider("liveStream", videoProvider);
    VideoStreamReceiver* videoReceiver = new VideoStreamReceiver(videoProvider, this);
    // ==========================================================

    // 4. 延时 100 毫秒加载 QML，避开 UI 初始化高峰期，彻底根治白屏
    QTimer::singleShot(100, this, [=]() {
        // --- 初始化 3D 引擎 ---
        m_quickView->rootContext()->setContextProperty("twinBackend", m_twinBackend);
        m_quickView->setSource(QUrl(QStringLiteral("qrc:/QtWidgetsApplication/MainView.qml")));

        // --- 初始化 2D 视频引擎 ---
        m_videoView->rootContext()->setContextProperty("videoReceiver", videoReceiver);
        m_videoView->setSource(QUrl(QStringLiteral("qrc:/QtWidgetsApplication/VideoView.qml")));
        });

    // 5. 极致性能优化：监听 GroupBox 的展开/折叠
    // 【数字孪生】：打勾连 TCP，取消断 TCP
    connect(ui.groupBox_Twin, &QGroupBox::toggled, this, [=](bool checked) {
        if (checked) {
            containerTwin->show();
            m_quickView->update();
            m_relayClient->connectTwin();
        }
        else {
            containerTwin->hide();
            m_relayClient->disconnectTwin();
        }
        });

    // 🚀 【图像传输】：打勾发送 UDP 穿透包，取消发停止包
    connect(ui.groupBox_Camera, &QGroupBox::toggled, this, [=](bool checked) {
        if (checked) {
            videoReceiver->setRelayIp(ui.lineEdit->text().trimmed());
            videoReceiver->startCamera();
        }
        else {
            videoReceiver->stopCamera();
        }
        });
    // ==========================================================


    m_touchDevice = new TouchDeviceManager(this);
    m_relayClient = new RelayNetworkClient(this);

    // --- Touch 信号连线 ---
    connect(m_touchDevice, &TouchDeviceManager::logMessage, this, [=](const QString& msg) {
        ui.txt_touch_log->appendPlainText(msg);
        ui.txt_touch_log->verticalScrollBar()->setValue(ui.txt_touch_log->verticalScrollBar()->maximum());
        });

    connect(m_touchDevice, &TouchDeviceManager::transformUpdated,
        ui.opengl_touch_window, &TouchOpenGLWidget::updateTransform);

    connect(m_touchDevice, &TouchDeviceManager::positionUpdated,
        this, &QtWidgetsApplication::onTouchPositionUpdated);

    connect(m_touchDevice, &TouchDeviceManager::statusChanged,
        this, &QtWidgetsApplication::onTouchStatusChanged);

    connect(m_touchDevice, &TouchDeviceManager::buttonStateChanged,
        this, &QtWidgetsApplication::onTouchButtonEvent);

    // --- 机械臂网络信号连线 ---
    // 【核心新增：将网络的 8ms 高频数据流，直接泵入 3D 数字孪生体后端！】
    connect(m_relayClient, &RelayNetworkClient::twinDataReceived,
        m_twinBackend, &RobotTwinBackend::updateJoints);

    connect(m_relayClient, &RelayNetworkClient::logMessage, this, [=](const QString& msg) {
        ui.txt_robot_log->appendPlainText(msg);
        ui.txt_robot_log->verticalScrollBar()->setValue(ui.txt_robot_log->verticalScrollBar()->maximum());
        });

    connect(m_relayClient, &RelayNetworkClient::connectionStatusChanged,
        this, &QtWidgetsApplication::onRobotConnectionChanged);

    connect(m_relayClient, &RelayNetworkClient::initializationFinished,
        this, &QtWidgetsApplication::onRobotInitFinished);

    connect(m_relayClient, &RelayNetworkClient::robotPoseRefreshFinished,
        this, &QtWidgetsApplication::onRobotPoseRefreshFinished);

    // --- 按钮点击连线 ---
    connect(ui.pushButton_start, &QPushButton::clicked,
        this, &QtWidgetsApplication::onStartButtonClicked);

    connect(ui.pushButton_start_2, &QPushButton::clicked,
        this, &QtWidgetsApplication::onStopButtonClicked);

    // 启动设备检测（包含自动重连）
    if (m_touchDevice->initDevice()) {
        m_touchDevice->startServo();
    }
    else {
        m_touchDevice->stopServo(false);
    }
}

QtWidgetsApplication::~QtWidgetsApplication() {
    if (m_touchDevice) {
        m_touchDevice->disconnect();
        m_touchDevice->stopServo(true);
    }
    if (m_relayClient) {
        m_relayClient->stopAll();
        m_relayClient->disconnect();
    }
}

void QtWidgetsApplication::onTouchPositionUpdated(double x, double y, double z) {
    ui.lineEdit_2->setText(QString::number(x, 'f', 2));
    ui.lineEdit_3->setText(QString::number(y, 'f', 2));
    ui.lineEdit_4->setText(QString::number(z, 'f', 2));

    // 如果处于控制激活状态，计算相对位移并下发
    if (m_isControllingRobot && m_relayClient->isConnected()) {
        double dx = x - m_controlBasePos.x();
        double dy = y - m_controlBasePos.y();
        double dz = z - m_controlBasePos.z();

        if (std::abs(dx - m_lastSentDelta.x()) > 1.0 ||
            std::abs(dy - m_lastSentDelta.y()) > 1.0 ||
            std::abs(dz - m_lastSentDelta.z()) > 1.0) {

            m_relayClient->sendMoveCommand(dx, dy, dz);
            m_lastSentDelta = QVector3D(dx, dy, dz);
        }
    }
}

void QtWidgetsApplication::onTouchStatusChanged(const QString& status, bool isConnected) {
    if (!isConnected) {
        ui.lineEdit_5->setText("Disconnected");
    }
    else {
        if (m_isControllingRobot) {
            ui.lineEdit_5->setText("OnControlling");
        }
        else {
            ui.lineEdit_5->setText(status);
        }
    }

    // 🚀 修复1：恢复红绿配色，并加上死锁尺寸的咒语防止变方
    QString ledStyle = isConnected
        ? "background-color: #00E676; border-radius: 9px; border: 2px solid #161A23; min-width: 14px; max-width: 14px; min-height: 14px; max-height: 14px;"
        : "background-color: #FF3B30; border-radius: 9px; border: 2px solid #161A23; min-width: 14px; max-width: 14px; min-height: 14px; max-height: 14px;";
    ui.led_touch_status->setStyleSheet(ledStyle);
}

void QtWidgetsApplication::onTouchButtonEvent(bool isPressed) {
    QVector3D curPos(
        ui.lineEdit_2->text().toDouble(),
        ui.lineEdit_3->text().toDouble(),
        ui.lineEdit_4->text().toDouble()
    );

    if (ui.radioButton_Hold->isChecked()) {
        if (isPressed) {
            // 不再在 UI 线程直接 GetPose，改成后台线程刷新
            m_pendingControlEnable = true;
            m_pendingControlBasePos = curPos;
            m_relayClient->refreshRobotBaseAsync();
        }
        else {
            m_pendingControlEnable = false;
            m_isControllingRobot = false;
            m_lastSentDelta = QVector3D(0, 0, 0);
        }
    }
    else {
        if (isPressed) {
            if (m_isControllingRobot) {
                m_pendingControlEnable = false;
                m_isControllingRobot = false;
                m_lastSentDelta = QVector3D(0, 0, 0);
            }
            else {
                m_pendingControlEnable = true;
                m_pendingControlBasePos = curPos;
                m_relayClient->refreshRobotBaseAsync();
            }
        }
    }
}

void QtWidgetsApplication::onRobotPoseRefreshFinished(bool success) {
    if (!m_pendingControlEnable) {
        return;
    }

    if (!success) {
        ui.lineEdit_7->setText("Pose Sync Failed");
        ui.lineEdit_7->setStyleSheet("color: #FF5555;");
        m_isControllingRobot = false;
        m_lastSentDelta = QVector3D(0, 0, 0);
        m_pendingControlEnable = false;
        return;
    }

    m_controlBasePos = m_pendingControlBasePos;
    m_lastSentDelta = QVector3D(0, 0, 0);
    m_isControllingRobot = true;
    m_pendingControlEnable = false;
}

void QtWidgetsApplication::onStartButtonClicked() {
    QString ip = ui.lineEdit->text().trimmed();
    if (ip.isEmpty()) {
        QMessageBox::warning(this, "Prompt", "Please enter Relay Station IP");
        return;
    }

    ui.pushButton_start->setEnabled(false);
    ui.lineEdit_7->setText("Connecting...");
    ui.lineEdit_7->setStyleSheet("color: #FFD166;");

    // 改为后台线程连接 + 初始化
    m_relayClient->connectAndInitializeAsync(ip, 8888);
}

void QtWidgetsApplication::onStopButtonClicked() {
    m_pendingControlEnable = false;
    m_isControllingRobot = false;
    m_lastSentDelta = QVector3D(0, 0, 0);
    m_relayClient->stopAll();

    ui.pushButton_start->setEnabled(true);
    ui.lineEdit_7->clear();
}

void QtWidgetsApplication::onRobotConnectionChanged(bool connected) {
    // 🚀 修复2：机械臂的灯也同步恢复红绿配色，死锁尺寸
    QString style = connected
        ? "background-color: #00E676; border-radius: 9px; border: 2px solid #161A23; min-width: 14px; max-width: 14px; min-height: 14px; max-height: 14px;"
        : "background-color: #FF3B30; border-radius: 9px; border: 2px solid #161A23; min-width: 14px; max-width: 14px; min-height: 14px; max-height: 14px;";

    ui.led_dobot->setStyleSheet(style);
    ui.lineEdit->setReadOnly(connected);

    if (!connected) {
        ui.pushButton_start->setEnabled(true);
        if (ui.lineEdit_7) {
            ui.lineEdit_7->clear();
        }
        ui.txt_robot_log->appendPlainText(">> [Network] Relay Station main control port (8888) disconnected.");
    }
}

void QtWidgetsApplication::onRobotInitFinished(bool success) {
    ui.lineEdit_7->setText(success ? "Robot Ready" : "Init Failed");
    ui.lineEdit_7->setStyleSheet(success ? "color: #00FFCC;" : "color: #FF5555;");

    if (!success) {
        ui.pushButton_start->setEnabled(true);
    }
}