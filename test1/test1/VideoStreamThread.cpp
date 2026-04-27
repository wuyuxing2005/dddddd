#include "VideoStreamThread.h"

#include <QDebug>
#include <QElapsedTimer>
#include <opencv2/opencv.hpp>
#include <vector>

VideoStreamThread::VideoStreamThread(QObject* parent)
    : QThread(parent) {
}

void VideoStreamThread::startStreaming(const QHostAddress& targetIp, int targetPort) {
    if (m_running && m_targetIp == targetIp && m_targetPort == targetPort) return;
    m_targetIp = targetIp;
    m_targetPort = targetPort;
    if (!m_running) {
        m_running = true;
        start();
    }
}

void VideoStreamThread::stopStreaming() {
    m_running = false;
    wait();
}

void VideoStreamThread::run() {
    cv::VideoCapture cap(0, cv::CAP_DSHOW);
    if (!cap.isOpened()) {
        qDebug() << "[Video Stream] Camera open failed!";
        emit cameraStatusChanged(false);
        return;
    }

    emit cameraStatusChanged(true);

    cap.set(cv::CAP_PROP_FRAME_WIDTH, 640);
    cap.set(cv::CAP_PROP_FRAME_HEIGHT, 480);

    cv::Mat frame;
    std::vector<uchar> buffer;
    std::vector<int> encodeParams = { cv::IMWRITE_JPEG_QUALITY, 60 };

    qDebug() << "[Video Stream] Started pushing to" << m_targetIp.toString() << ":" << m_targetPort;

    while (m_running) {
        QElapsedTimer processTimer;
        processTimer.start();

        cap >> frame;
        if (frame.empty()) continue;

        cv::imencode(".jpg", frame, buffer, encodeParams);

        if (!buffer.empty() && buffer.size() < 65500) {
            double latency = processTimer.nsecsElapsed() / 1000000.0;
            QByteArray frameData(reinterpret_cast<const char*>(buffer.data()), buffer.size());
            emit frameEncoded(frameData, m_targetIp, m_targetPort, latency);
        }

        QThread::msleep(33);
    }

    cap.release();
    qDebug() << "[Video Stream] Push thread safely exited";
}
