#pragma once
#include <QQuickImageProvider>
#include <QImage>
#include <QMutex>
#include <QMutexLocker>

// 这个类就像一个缓存仓库，存放最新收到的一帧画面
class VideoImageProvider : public QQuickImageProvider {
public:
    VideoImageProvider() : QQuickImageProvider(QQuickImageProvider::Image) {}

    // 核心功能：QML 会调用这个函数来拿图片
    QImage requestImage(const QString& id, QSize* size, const QSize& requestedSize) override {
        QMutexLocker locker(&m_mutex);
        if (size) {
            *size = m_currentFrame.size();
        }
        return m_currentFrame;
    }

    // 给 UDP 接收线程用的：存入新画面
    void updateImage(const QImage& newFrame) {
        QMutexLocker locker(&m_mutex);
        m_currentFrame = newFrame;
    }

private:
    QImage m_currentFrame;
    QMutex m_mutex;
};