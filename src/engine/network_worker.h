#pragma once

#if defined(_WIN32)
#include <winsock2.h>
#else
#include <sys/socket.h>
#endif

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>

namespace cse {

#if defined(_WIN32)
using NetworkWorkerSocket = SOCKET;
constexpr NetworkWorkerSocket kInvalidNetworkWorkerSocket = INVALID_SOCKET;
#else
using NetworkWorkerSocket = int;
constexpr NetworkWorkerSocket kInvalidNetworkWorkerSocket = -1;
#endif

constexpr int kNetworkWorkerSourceCount = 3;

// Start/Stop are owned by the engine thread. Callbacks run with the packet
// mutex held; watched sockets must be replaced only after Stop has joined.
class NetworkWorker {
public:
    using CollectSockets = int (*)(NetworkWorkerSocket* sockets, int capacity);
    using QueuePacket = bool (*)(int source);

    explicit NetworkWorker(std::recursive_mutex& packetMutex);
    ~NetworkWorker();
    NetworkWorker(const NetworkWorker&) = delete;
    NetworkWorker& operator=(const NetworkWorker&) = delete;

    // The legacy sleep preference is bounded because non-socket transports also need polling.
    bool Start(CollectSockets collectSockets, QueuePacket queuePacket, bool sleepForever);
    void Stop();

private:
    bool OpenWakeSockets();
    void CloseWakeSockets();
    void Wake();
    int WaitForPackets();
    void Run();

    std::recursive_mutex& packetMutex_;
    std::thread thread_;
    std::atomic<bool> stopping_{false};
    std::mutex pauseMutex_;
    std::condition_variable pauseWake_;
    NetworkWorkerSocket wakeRead_ = kInvalidNetworkWorkerSocket;
    NetworkWorkerSocket wakeWrite_ = kInvalidNetworkWorkerSocket;
    CollectSockets collectSockets_ = nullptr;
    QueuePacket queuePacket_ = nullptr;
};

}
