#pragma once

#include <QObject>
#include <QVector>
#include <QTimer>
#include <QString>
#include <mutex>
#include <atomic>

// OpenHaptics 头文件
#include <HD/hd.h>

class TouchDeviceManager : public QObject {
    Q_OBJECT

public:
    explicit TouchDeviceManager(QObject* parent = nullptr);
    ~TouchDeviceManager();

    // 设备生命周期控制
    bool initDevice(bool isAutoReconnect = false);
    void startServo();
    void stopServo(bool isShuttingDown = false); // 加入关闭程序时的静默标志

signals:
    // --------------------------------------------------------
    // 向外发送的跨线程信号 (UI 线程安全)
    // --------------------------------------------------------
    void transformUpdated(const QVector<double>& transformMatrix);
    void positionUpdated(double x, double y, double z);
    void statusChanged(const QString& statusText, bool isConnected);
    void logMessage(const QString& msg);
    void buttonStateChanged(bool isPressed);

private slots:
    // 定时器槽函数
    void onSyncTimer();       // 60Hz UI 刷新
    void onReconnectTimer();  // 断线重连探测

private:
    HHD m_hHD;
    HDSchedulerHandle m_callbackHandle;
    QTimer* m_syncTimer;
    QTimer* m_reconnectTimer;

    // --------------------------------------------------------
    // 跨线程共享数据区 (必须使用互斥锁保护)
    // --------------------------------------------------------
    std::mutex m_dataMutex;
    QVector<double> m_currentTransform;
    double m_posX, m_posY, m_posZ;
    double m_velocityX, m_velocityY, m_velocityZ;

    // 原子变量，保证多线程读写安全
    std::atomic<bool> m_isConnected;
    std::atomic<bool> m_isButtonPressed;
    std::atomic<bool> m_deviceError;

    bool m_lastButtonState;

    // OpenHaptics 静态底层回调函数 (1000Hz)
    static HDCallbackCode HDCALLBACK DeviceStateCallback(void* pUserData);
};