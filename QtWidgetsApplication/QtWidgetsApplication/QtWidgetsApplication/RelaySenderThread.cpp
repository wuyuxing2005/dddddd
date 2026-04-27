#include "RelaySenderThread.h"

#include "RelayNetworkClient.h"

#include <QMutexLocker>

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
        RelayMoveDelta data;
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
