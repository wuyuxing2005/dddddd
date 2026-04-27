#pragma once

#include <QThread>
#include <atomic>

class RelayNetworkClient;

class RelaySenderThread : public QThread {
    Q_OBJECT

public:
    explicit RelaySenderThread(RelayNetworkClient* owner);
    void stop();

protected:
    void run() override;

private:
    RelayNetworkClient* m_owner;
    std::atomic<bool> m_running;
};
