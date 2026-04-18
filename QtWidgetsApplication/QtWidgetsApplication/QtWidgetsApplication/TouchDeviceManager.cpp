#include "TouchDeviceManager.h"
#include <cmath>

TouchDeviceManager::TouchDeviceManager(QObject* parent)
    : QObject(parent),
    m_hHD(HD_INVALID_HANDLE),
    m_isConnected(false),
    m_isButtonPressed(false),
    m_deviceError(false),
    m_lastButtonState(false)
{
    m_currentTransform.resize(16);
    m_currentTransform.fill(0.0);
    m_currentTransform[0] = 1.0; m_currentTransform[5] = 1.0;
    m_currentTransform[10] = 1.0; m_currentTransform[15] = 1.0;

    m_syncTimer = new QTimer(this);
    connect(m_syncTimer, &QTimer::timeout, this, &TouchDeviceManager::onSyncTimer);

    m_reconnectTimer = new QTimer(this);
    connect(m_reconnectTimer, &QTimer::timeout, this, &TouchDeviceManager::onReconnectTimer);
}

TouchDeviceManager::~TouchDeviceManager() {
    m_reconnectTimer->stop();
    m_syncTimer->stop();
    if (m_hHD != HD_INVALID_HANDLE) {
        hdStopScheduler();
        hdUnschedule(m_callbackHandle);
        hdDisableDevice(m_hHD);
    }
}

bool TouchDeviceManager::initDevice(bool isAutoReconnect) {
    if (!isAutoReconnect) {
        emit logMessage("[Touch] Searching and initializing 3D Touch device...");
    }

    m_hHD = hdInitDevice(HD_DEFAULT_DEVICE);
    HDErrorInfo err = hdGetError();
    if (HD_DEVICE_ERROR(err)) {
        if (!isAutoReconnect) {
            emit logMessage(QString("[Touch Error] Initialization failed! Check connection. Error code: %1").arg(err.errorCode));
        }
        m_isConnected = false;
        m_deviceError = false;
        emit statusChanged("Disconnected", false);
        return false;
    }


    if (m_hHD != HD_INVALID_HANDLE) {
        emit logMessage("[Touch] Device connected, preparing servo thread!");
        hdEnable(HD_FORCE_OUTPUT);
        m_isConnected = true;
        m_deviceError = false;
        return true;
    }
    emit logMessage("[Touch] Device does not connect, please check your device!");
    m_isConnected = false;
    m_deviceError = true;
    return false;


}

void TouchDeviceManager::startServo() {
    if (!m_isConnected) return;

    m_callbackHandle = hdScheduleAsynchronous(DeviceStateCallback, this, HD_MAX_SCHEDULER_PRIORITY);

    HDErrorInfo err = hdGetError();
    if (HD_DEVICE_ERROR(err)) {
        emit logMessage("[Touch Error] Callback scheduling failed!");
        return;
    }

    hdStartScheduler();
    err = hdGetError();
    if (HD_DEVICE_ERROR(err)) {
        emit logMessage("[Touch Error] Servo scheduler start failed!");
        return;
    }

    emit logMessage("[Touch] Servo thread running (1000Hz).");
    m_syncTimer->start(16);
    m_reconnectTimer->stop();
}

void TouchDeviceManager::stopServo(bool isShuttingDown) {
    if (m_hHD != HD_INVALID_HANDLE) {
        m_syncTimer->stop();

        hdStopScheduler();
        hdUnschedule(m_callbackHandle);
        hdDisableDevice(m_hHD);

        m_hHD = HD_INVALID_HANDLE;
        m_isConnected = false;
        m_deviceError = false;

        if (!isShuttingDown) {
            emit logMessage("[Touch] Device resources released, disconnected.");
            emit statusChanged("Disconnected", false);

            if (m_lastButtonState) {
                m_lastButtonState = false;
                m_isButtonPressed = false;
                emit buttonStateChanged(false);
            }
        }
    }

    if (!isShuttingDown && !m_reconnectTimer->isActive()) {
        emit logMessage("[Touch] Auto-reconnect detection enabled (2s)...");
        m_reconnectTimer->start(2000);
    }
}

void TouchDeviceManager::onReconnectTimer() {
    if (m_isConnected) return;
    if (initDevice(true)) {
        startServo();
    }
}

HDCallbackCode HDCALLBACK TouchDeviceManager::DeviceStateCallback(void* pUserData) {
    TouchDeviceManager* manager = static_cast<TouchDeviceManager*>(pUserData);

    hdBeginFrame(manager->m_hHD);

    if (HD_DEVICE_ERROR(hdGetError())) {
        manager->m_deviceError = true;
        return HD_CALLBACK_DONE;
    }

    HDdouble transform[16];
    HDdouble position[3];
    HDdouble velocity[3];
    HDint buttons;

    hdGetDoublev(HD_CURRENT_TRANSFORM, transform);
    hdGetDoublev(HD_CURRENT_POSITION, position);
    hdGetDoublev(HD_CURRENT_VELOCITY, velocity);
    hdGetIntegerv(HD_CURRENT_BUTTONS, &buttons);

    hdEndFrame(manager->m_hHD);

    if (HD_DEVICE_ERROR(hdGetError())) {
        manager->m_deviceError = true;
        return HD_CALLBACK_DONE;
    }

    std::lock_guard<std::mutex> lock(manager->m_dataMutex);
    for (int i = 0; i < 16; i++) {
        manager->m_currentTransform[i] = transform[i];
    }
    manager->m_posX = position[0];
    manager->m_posY = position[1];
    manager->m_posZ = position[2];
    manager->m_velocityX = velocity[0];
    manager->m_velocityY = velocity[1];
    manager->m_velocityZ = velocity[2];
    manager->m_isButtonPressed = (buttons & HD_DEVICE_BUTTON_1) != 0;

    return HD_CALLBACK_CONTINUE;
}

void TouchDeviceManager::onSyncTimer() {
    if (m_deviceError) {
        emit logMessage("[Touch Warning] Device communication unexpectedly interrupted (USB unplugged?)!");
        stopServo(false);
        return;
    }

    std::lock_guard<std::mutex> lock(m_dataMutex);
    emit transformUpdated(m_currentTransform);
    emit positionUpdated(m_posX, -m_posZ, m_posY);

    double speedSq = m_velocityX * m_velocityX + m_velocityY * m_velocityY + m_velocityZ * m_velocityZ;
    QString currentStatus = (speedSq > 1.0) ? "Moving" : "Static";

    if (m_isButtonPressed) {
        currentStatus = "OnControlling";
    }
    emit statusChanged(currentStatus, true);

    bool currentBtn = m_isButtonPressed;
    if (currentBtn != m_lastButtonState) {
        emit buttonStateChanged(currentBtn);
        m_lastButtonState = currentBtn;
    }
}