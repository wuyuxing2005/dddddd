#pragma once

#include <QWidget>
#include <QVector3D>
#include "ui_QtWidgetsApplication.h"
#include "TouchDeviceManager.h"
#include "RelayNetworkClient.h"
#include <QQuickView>

class RobotTwinBackend;

class QtWidgetsApplication : public QWidget {
    Q_OBJECT

public:
    QtWidgetsApplication(QWidget* parent = nullptr);
    ~QtWidgetsApplication();

private slots:
    void onTouchPositionUpdated(double x, double y, double z);
    void onTouchStatusChanged(const QString& status, bool isConnected);
    void onTouchButtonEvent(bool isPressed);

    void onRobotConnectionChanged(bool connected);
    void onRobotInitFinished(bool success);
    void onRobotPoseRefreshFinished(bool success);

    void onStartButtonClicked();
    void onStopButtonClicked();

private:
    Ui::QtWidgetsApplicationClass ui;
    TouchDeviceManager* m_touchDevice = nullptr;
    RelayNetworkClient* m_relayClient = nullptr;

    bool m_isControllingRobot = false;
    QVector3D m_controlBasePos;
    QVector3D m_lastSentDelta = QVector3D(0, 0, 0);

    // 异步控制激活辅助状态
    bool m_pendingControlEnable = false;
    QVector3D m_pendingControlBasePos = QVector3D(0, 0, 0);

    RobotTwinBackend* m_twinBackend = nullptr;

    QQuickView* m_quickView = nullptr;

    QQuickView* m_videoView;
};