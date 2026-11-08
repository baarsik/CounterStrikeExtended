#include "network_worker.h"

#if !defined(_WIN32)
#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>
#endif

#include <array>
#include <chrono>
#include <system_error>

namespace cse {
namespace {

constexpr long kNetworkPollMicroseconds = 20000;
constexpr auto kNetworkDrainPause = std::chrono::milliseconds(1);
constexpr char kWakeByte = 1;

void CloseSocket(NetworkWorkerSocket socket) {
    if (socket != kInvalidNetworkWorkerSocket) {
#if defined(_WIN32)
        closesocket(socket);
#else
        close(socket);
#endif
    }
}

bool MakeNonblocking(NetworkWorkerSocket socket) {
#if defined(_WIN32)
    u_long nonblocking = 1;
    return ioctlsocket(socket, FIONBIO, &nonblocking) == 0;
#else
    int flags = fcntl(socket, F_GETFL, 0);
    return flags != -1 && fcntl(socket, F_SETFL, flags | O_NONBLOCK) != -1;
#endif
}

bool FitsSocketSet(NetworkWorkerSocket socket) {
#if defined(_WIN32)
    // Windows fd_set stores full-width SOCKET values, not descriptor indices.
    return socket != kInvalidNetworkWorkerSocket;
#else
    return socket >= 0 && socket < FD_SETSIZE;
#endif
}

}

NetworkWorker::NetworkWorker(std::recursive_mutex& packetMutex) : packetMutex_(packetMutex) {}

NetworkWorker::~NetworkWorker() {
    Stop();
}

bool NetworkWorker::OpenWakeSockets() {
    // A loopback datagram wakes select on both Winsock 1.1 and POSIX. It is
    // private to the worker and never enters an engine packet queue.
    wakeRead_ = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (!FitsSocketSet(wakeRead_) || !MakeNonblocking(wakeRead_)) {
        CloseWakeSockets();
        return false;
    }

    sockaddr_in address = {};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if (bind(wakeRead_, reinterpret_cast<const sockaddr*>(&address), sizeof(address)) != 0) {
        CloseWakeSockets();
        return false;
    }

#if defined(_WIN32)
    int addressLength = sizeof(address);
#else
    socklen_t addressLength = sizeof(address);
#endif
    if (getsockname(wakeRead_, reinterpret_cast<sockaddr*>(&address), &addressLength) != 0) {
        CloseWakeSockets();
        return false;
    }

    wakeWrite_ = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (wakeWrite_ == kInvalidNetworkWorkerSocket || !MakeNonblocking(wakeWrite_)
        || connect(wakeWrite_, reinterpret_cast<const sockaddr*>(&address), sizeof(address)) != 0) {
        CloseWakeSockets();
        return false;
    }
    return true;
}

void NetworkWorker::CloseWakeSockets() {
    CloseSocket(wakeWrite_);
    CloseSocket(wakeRead_);
    wakeWrite_ = kInvalidNetworkWorkerSocket;
    wakeRead_ = kInvalidNetworkWorkerSocket;
}

bool NetworkWorker::Start(CollectSockets collectSockets, QueuePacket queuePacket, bool sleepForever) {
    if (thread_.joinable()) {
        return true;
    }
    if (!collectSockets || !queuePacket || !OpenWakeSockets()) {
        return false;
    }

    collectSockets_ = collectSockets;
    queuePacket_ = queuePacket;
    sleepForever_ = sleepForever;
    stopping_.store(false, std::memory_order_release);
    try {
        thread_ = std::thread(&NetworkWorker::Run, this);
    } catch (const std::system_error&) {
        CloseWakeSockets();
        return false;
    }
    return true;
}

void NetworkWorker::Wake() {
    // A full wake socket is already readable, so a nonblocking send cannot
    // hold shutdown behind the engine's packet mutex.
    send(wakeWrite_, &kWakeByte, sizeof(kWakeByte), 0);
    pauseWake_.notify_all();
}

void NetworkWorker::Stop() {
    if (thread_.joinable()) {
        stopping_.store(true, std::memory_order_release);
        Wake();
        thread_.join();
    }
    CloseWakeSockets();
}

int NetworkWorker::WaitForPackets() {
    std::array<NetworkWorkerSocket, kNetworkWorkerSourceCount> sockets;
    int count;
    {
        std::lock_guard<std::recursive_mutex> lock(packetMutex_);
        count = collectSockets_(sockets.data(), static_cast<int>(sockets.size()));
    }

    fd_set readSockets;
    FD_ZERO(&readSockets);
    FD_SET(wakeRead_, &readSockets);
    NetworkWorkerSocket maxSocket = wakeRead_;
    if (count < 0 || count > static_cast<int>(sockets.size())) {
        return -1;
    }
    for (int index = 0; index < count; ++index) {
        NetworkWorkerSocket socket = sockets[index];
        if (!FitsSocketSet(socket)) {
            return -1;
        }
        FD_SET(socket, &readSockets);
        if (socket > maxSocket) {
            maxSocket = socket;
        }
    }

    timeval timeout = { 0, kNetworkPollMicroseconds };
#if defined(_WIN32)
    // Winsock ignores nfds. Do not truncate a 64-bit SOCKET to form it.
    constexpr int kIgnoredSocketCount = 0;
    return select(kIgnoredSocketCount, &readSockets, nullptr, nullptr, sleepForever_ ? nullptr : &timeout);
#else
    return select(maxSocket + 1, &readSockets, nullptr, nullptr, sleepForever_ ? nullptr : &timeout);
#endif
}

void NetworkWorker::Run() {
    while (!stopping_.load(std::memory_order_acquire)) {
        int ready = WaitForPackets();
        while (ready != 0 && !stopping_.load(std::memory_order_acquire)) {
            bool idle = true;
            // Preserve the engine's round-robin source order and drain until
            // idle, with a stop point between complete, owned queue packets.
            for (int source = 0; source < kNetworkWorkerSourceCount; ++source) {
                if (stopping_.load(std::memory_order_acquire)) {
                    break;
                }
                std::lock_guard<std::recursive_mutex> lock(packetMutex_);
                if (queuePacket_(source)) {
                    idle = false;
                }
            }
            if (idle) {
                break;
            }
        }
        std::unique_lock<std::mutex> lock(pauseMutex_);
        pauseWake_.wait_for(lock, kNetworkDrainPause, [this] {
            return stopping_.load(std::memory_order_acquire);
        });
    }
}

}
