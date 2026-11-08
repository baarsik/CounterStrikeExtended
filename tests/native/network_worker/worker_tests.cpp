#if defined(_WIN32)
#include <winsock2.h>
#include <Windows.h>
#else
#include <arpa/inet.h>
#include <cerrno>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/resource.h>
#include <unistd.h>
using SOCKET = int;
#endif

#include <array>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <condition_variable>
#include <cstddef>
#include <future>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <unordered_set>
#include <vector>

#include "network_worker.h"
#include "../common/netadr.h"

using byte = unsigned char;
using qboolean = int;
enum netsrc_t { NS_CLIENT, NS_SERVER, NS_MULTICAST };
constexpr int NET_SOCKET_SOURCES = 3;
constexpr int MAX_LOOPBACK = 4;
constexpr int LOOPBACK_MESSAGE_BYTES = 6037;
[[maybe_unused]] constexpr int LOOPBACK_CHANNELS = 2;
constexpr int MAX_RECEIVE_PACKET_BYTES = 6010;
constexpr int PREALLOCATED_PACKETS = 40;
constexpr int PREALLOCATED_PACKET_BYTES = 1536;
constexpr int SPLIT_PACKET_ID = -2;
constexpr int ca_dedicated = 1;

struct sizebuf_t {
    byte* data;
    int cursize;
};

struct loopmsg_t {
    byte data[LOOPBACK_MESSAGE_BYTES];
    int datalen;
};

struct loopback_t {
    loopmsg_t msgs[MAX_LOOPBACK];
    int get;
    int send;
};

#include "actual_network_types.h"

#pragma pack(push, 1)
struct SPLITPACKET {
    int netID;
    int sequenceNumber;
    byte packetID;
};
#pragma pack(pop)

static int use_thread;
static qboolean net_thread_initialized;
[[maybe_unused]] static int net_sleepforever = 1;
static SOCKET ip_sockets[NET_SOCKET_SOURCES];
static std::recursive_mutex net_thread_mutex;
static cse::NetworkWorker net_worker(net_thread_mutex);
static loopback_t loopbacks[NET_SOCKET_SOURCES];
static net_messages_t* messages[NET_SOCKET_SOURCES];
static net_messages_t* normalqueue;
static std::array<byte, MAX_RECEIVE_PACKET_BYTES> inputBytes;
static std::array<byte, MAX_RECEIVE_PACKET_BYTES> outputBytes;
static sizebuf_t in_message = { inputBytes.data(), 0 };
static sizebuf_t net_message = { outputBytes.data(), 0 };
static netadr_t in_from;
static netadr_t net_from;
static int msg_readcount;
static qboolean noip;
static int net_configured;
static packetlag_t g_pLagData[NET_SOCKET_SOURCES];
static std::array<sockaddr_in, NET_SOCKET_SOURCES> configuredAddresses;
static struct { int state; } cls = { ca_dedicated };
static std::unordered_set<void*> allocations;
static std::vector<std::vector<byte>> copiedPackets;
static struct {
    std::mutex mutex;
    std::condition_variable wake;
    bool enabled = false;
    bool entered = false;
    bool release = false;
} copyGate;

static void Require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

static void* Mem_ZeroMalloc(std::size_t size) {
    void* allocation = std::calloc(1, size);
    Require(allocation != nullptr, "fixture allocation failed");
    allocations.insert(allocation);
    return allocation;
}

static void Mem_Free(void* allocation) {
    Require(allocations.erase(allocation) == 1, "production queue freed an unowned allocation twice");
    std::free(allocation);
}

static void Q_memcpy(void* destination, const void* source, int length) {
    std::memcpy(destination, source, static_cast<std::size_t>(length));
    if (allocations.count(destination) != 0) {
        const byte* bytes = static_cast<const byte*>(source);
        copiedPackets.emplace_back(bytes, bytes + length);
        std::unique_lock<std::mutex> lock(copyGate.mutex);
        if (copyGate.enabled) {
            copyGate.entered = true;
            copyGate.wake.notify_all();
            copyGate.wake.wait(lock, [] { return copyGate.release; });
        }
    }
}

static void Q_memset(void* destination, int value, std::size_t length) {
    std::memset(destination, value, length);
}

[[noreturn]] static void Sys_Error(const char*, ...) {
    throw std::runtime_error("network thread initialization failed");
}

static void Con_Printf(const char*, ...) {}
static void Con_DPrintf(const char*, ...) {}
static void NET_AdjustLag() {}
static qboolean NET_LagPacket(qboolean newData, netsrc_t, netadr_t*, sizebuf_t*) { return newData; }
static qboolean NET_GetLong(char*, int, int*) { return false; }
static const char* NET_AdrToString(netadr_t) { return "fixture"; }
[[maybe_unused]] static const char* NET_ErrorString(int) { return "fixture socket error"; }
static int NET_SteamFakeIPRecvFrom(netsrc_t, char*, int, int, int result, sockaddr*, int*) { return result; }
static bool NET_ShouldUseSteamFakeIP() { return true; }
static void NET_GetLocalAddress() {}
static void NET_OpenIP();
static void NET_SteamFakeIPDestroySocket(netsrc_t) {
    Require(!net_thread_initialized, "socket lifetime changed before worker join");
}
static void NET_ClearLaggedList(packetlag_t*) {
    Require(!net_thread_initialized, "shutdown freed lag storage before worker join");
}

#include "actual_network.h"

static SOCKET OpenSocket(sockaddr_in& address) {
    SOCKET socket = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    Require(socket != cse::kInvalidNetworkWorkerSocket && socket != 0, "could not open fixture UDP socket");
    address = {};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    Require(bind(socket, reinterpret_cast<const sockaddr*>(&address), sizeof(address)) == 0, "could not bind UDP");
#if defined(_WIN32)
    int length = sizeof(address);
    u_long nonblocking = 1;
    Require(ioctlsocket(socket, FIONBIO, &nonblocking) == 0, "could not make UDP nonblocking");
#else
    socklen_t length = sizeof(address);
    Require(fcntl(socket, F_SETFL, O_NONBLOCK) == 0, "could not make UDP nonblocking");
#endif
    Require(getsockname(socket, reinterpret_cast<sockaddr*>(&address), &length) == 0, "could not read UDP address");
    constexpr int kFixtureReceiveBufferBytes = 1024 * 1024;
    int receiveBufferBytes = kFixtureReceiveBufferBytes;
    Require(setsockopt(socket, SOL_SOCKET, SO_RCVBUF, reinterpret_cast<const char*>(&receiveBufferBytes),
        sizeof(receiveBufferBytes)) == 0, "could not size fixture UDP receive buffer");
    return socket;
}

static void CloseSocket(SOCKET socket) {
#if defined(_WIN32)
    closesocket(socket);
#else
    close(socket);
#endif
}

static void NET_OpenIP() {
    Require(!net_thread_initialized, "socket opened before worker join");
    for (int source = 0; source < NET_SOCKET_SOURCES; ++source) {
        ip_sockets[source] = OpenSocket(configuredAddresses[source]);
    }
}

static void TestPrivateQueueLayouts() {
    constexpr bool kPointer32 = sizeof(void*) == 4;
    const std::array<std::size_t, 6> lagOffsets = {
        offsetof(packetlag_t, pPacketData), offsetof(packetlag_t, nSize), offsetof(packetlag_t, net_from),
        offsetof(packetlag_t, receivedTime), offsetof(packetlag_t, pNext), offsetof(packetlag_t, pPrev)
    };
    const std::array<std::size_t, 6> expectedLagOffsets = kPointer32
        ? std::array<std::size_t, 6>{ 0, 4, 8, 28, 32, 36 }
        : std::array<std::size_t, 6>{ 0, 8, 12, 32, 40, 48 };
    const std::array<std::size_t, 5> messageOffsets = {
        offsetof(net_messages_t, next), offsetof(net_messages_t, preallocated), offsetof(net_messages_t, buffer),
        offsetof(net_messages_t, from), offsetof(net_messages_t, buffersize)
    };
    const std::array<std::size_t, 5> expectedMessageOffsets = kPointer32
        ? std::array<std::size_t, 5>{ 0, 4, 8, 12, 32 }
        : std::array<std::size_t, 5>{ 0, 8, 16, 24, 44 };
    Require(sizeof(netadr_t) == 20, "production network address layout changed");
    Require(sizeof(packetlag_t) == (kPointer32 ? 40 : 56) && lagOffsets == expectedLagOffsets,
        "private lag queue layout disagrees with native pointer storage");
    Require(sizeof(net_messages_t) == (kPointer32 ? 36 : 48) && messageOffsets == expectedMessageOffsets,
        "private packet queue layout disagrees with native pointer storage");
    std::printf("private queues: pointer=%zu netadr=%zu lag=%zu message=%zu\n", sizeof(void*), sizeof(netadr_t),
        sizeof(packetlag_t), sizeof(net_messages_t));
    std::printf("lag offsets: %zu %zu %zu %zu %zu %zu; message offsets: %zu %zu %zu %zu %zu\n",
        lagOffsets[0], lagOffsets[1], lagOffsets[2], lagOffsets[3], lagOffsets[4], lagOffsets[5],
        messageOffsets[0], messageOffsets[1], messageOffsets[2], messageOffsets[3], messageOffsets[4]);
}

static void AllocateFixture(bool sleepForever = true) {
    Require(allocations.empty(), "previous fixture leaked queue storage");
    copiedPackets.clear();
    use_thread = 0;
    net_sleepforever = sleepForever;
    NET_AllocateQueues();
    use_thread = 1;
}

static void SendPacket(SOCKET sender, const sockaddr_in& destination, const std::vector<byte>& packet) {
    int length = static_cast<int>(packet.size());
    Require(sendto(sender, reinterpret_cast<const char*>(packet.data()), length, 0,
        reinterpret_cast<const sockaddr*>(&destination), sizeof(destination)) == length, "could not send UDP packet");
}

static void WaitForPackets(std::size_t count) {
    constexpr auto kReceiveDeadline = std::chrono::seconds(2);
    const auto deadline = std::chrono::steady_clock::now() + kReceiveDeadline;
    std::size_t copied;
    do {
        {
            std::lock_guard<std::recursive_mutex> lock(net_thread_mutex);
            copied = copiedPackets.size();
        }
        if (copied < count) {
            std::this_thread::yield();
        }
    } while (copied < count && std::chrono::steady_clock::now() < deadline);
    Require(copied == count, "-netthread must receive real UDP packets without main-thread polling");
}

static void FlushFixture() {
    NET_FlushQueues();
    for (SOCKET& socket : ip_sockets) {
        if (socket) {
            CloseSocket(socket);
            socket = 0;
        }
    }
    Require(!net_thread_initialized, "flush must join the worker");
    Require(allocations.empty(), "production queue leaked allocation");
}

static void RequirePacket(netsrc_t source, const std::vector<byte>& packet, unsigned short senderPort) {
    Require(NET_GetPacket(source) != false, "worker packet must remain consumable after join");
    Require(net_message.cursize == static_cast<int>(packet.size())
        && std::memcmp(net_message.data, packet.data(), packet.size()) == 0, "worker changed packet bytes or order");
    Require(net_from.type == NA_IP && net_from.port == senderPort
        && net_from.ip[0] == 127 && net_from.ip[1] == 0 && net_from.ip[2] == 0 && net_from.ip[3] == 1,
        "worker changed sender address");
}

static void TestSourceDrainOrderAndBytes(bool sleepForever) {
    AllocateFixture(sleepForever);
    std::array<sockaddr_in, NET_SOCKET_SOURCES> destinations;
    for (int source = 0; source < NET_SOCKET_SOURCES; ++source) {
        ip_sockets[source] = OpenSocket(destinations[source]);
    }
    sockaddr_in senderAddress;
    SOCKET sender = OpenSocket(senderAddress);
    const std::vector<std::vector<byte>> packets = {
        { 1, 2, 3, 4, 0, 10 }, { 1, 2, 3, 4, 1, 10 }, { 1, 2, 3, 4, 2, 10 },
        { 5, 6, 7, 8, 0, 11 }, { 5, 6, 7, 8, 1, 11 }, { 5, 6, 7, 8, 2, 11 },
        { 9, 10, 11, 12, 0, 12 }, { 9, 10, 11, 12, 1, 12 }, { 9, 10, 11, 12, 2, 12 }
    };
    for (const auto& packet : packets) {
        SendPacket(sender, destinations[packet[4]], packet);
    }
    NET_StartThread();
    WaitForPackets(packets.size());
    NET_StopThread();
    Require(copiedPackets == packets, "worker must drain sources 0, 1, 2 once per pass until idle");
    for (const auto& packet : packets) {
        RequirePacket(static_cast<netsrc_t>(packet[4]), packet, senderAddress.sin_port);
    }
    CloseSocket(sender);
    FlushFixture();
}

static void TestPreallocatedAndHeapPacketOwnership() {
    AllocateFixture();
    sockaddr_in destination;
    ip_sockets[NS_CLIENT] = OpenSocket(destination);
    sockaddr_in senderAddress;
    SOCKET sender = OpenSocket(senderAddress);
    constexpr int kSmallPacketCount = 45;
    std::vector<std::vector<byte>> packets;
    for (int index = 0; index < kSmallPacketCount; ++index) {
        packets.push_back({ 1, 2, 3, 4, static_cast<byte>(index), 0x7f });
    }
    packets.emplace_back(1536, byte{0x5a});
    packets.emplace_back(1537, byte{0xa5});
    packets.emplace_back(6009, byte{0x3c});
    for (const auto& packet : packets) {
        SendPacket(sender, destination, packet);
    }
    NET_StartThread();
    WaitForPackets(packets.size());
    NET_StopThread();
    Require(copiedPackets == packets, "preallocation boundaries must preserve every packet byte");
    net_messages_t* message = messages[NS_CLIENT];
    for (std::size_t index = 0; index < packets.size(); ++index) {
        Require(message != nullptr, "queued packet disappeared");
        Require((message->preallocated != false) == (index < 40), "queue changed preallocated packet ownership");
        message = message->next;
    }
    for (const auto& packet : packets) {
        RequirePacket(NS_CLIENT, packet, senderAddress.sin_port);
    }
    Require(allocations.size() == 80, "heap packets must be freed and all 40 pooled packets retained on consume");
    CloseSocket(sender);
    FlushFixture();
}

static void TestStopJoinsInFlightPacket(bool flush) {
    AllocateFixture();
    sockaddr_in destination;
    ip_sockets[NS_CLIENT] = OpenSocket(destination);
    sockaddr_in senderAddress;
    SOCKET sender = OpenSocket(senderAddress);
    const std::vector<byte> packet = { 1, 2, 3, 4, 0xab, 0xcd };
    {
        std::lock_guard<std::mutex> lock(copyGate.mutex);
        copyGate.enabled = true;
        copyGate.entered = false;
        copyGate.release = false;
    }
    SendPacket(sender, destination, packet);
    NET_StartThread();
    {
        std::unique_lock<std::mutex> lock(copyGate.mutex);
        Require(copyGate.wake.wait_for(lock, std::chrono::seconds(2), [] { return copyGate.entered; }),
            "worker did not enter packet ownership boundary");
    }
    auto stopped = std::async(std::launch::async, [flush] {
        if (flush) {
            NET_FlushQueues();
        } else {
            NET_StopThread();
        }
    });
    const bool returnedBeforePacketCompleted = stopped.wait_for(std::chrono::milliseconds(50))
        == std::future_status::ready;
    {
        std::lock_guard<std::mutex> lock(copyGate.mutex);
        copyGate.release = true;
        copyGate.enabled = false;
        copyGate.wake.notify_all();
    }
    stopped.get();
    Require(!returnedBeforePacketCompleted, "stop must join after a complete queued packet, never terminate it");
    if (flush) {
        Require(allocations.empty(), "flush must free a completed in-flight packet after join");
    } else {
        RequirePacket(NS_CLIENT, packet, senderAddress.sin_port);
    }
    CloseSocket(sender);
    FlushFixture();
}

static std::atomic<int> busyCalls;
static std::atomic<bool> collectedIdle;

static int IdleSockets(cse::NetworkWorkerSocket*, int) {
    collectedIdle.store(true);
    return 0;
}

static bool AlwaysBusy(int) {
    ++busyCalls;
    return true;
}

static int ReadySocket(cse::NetworkWorkerSocket* sockets, int) {
    sockets[0] = ip_sockets[NS_CLIENT];
    return 1;
}

static void TestIdleWakeAndRepeatedRestart() {
    std::recursive_mutex mutex;
    cse::NetworkWorker worker(mutex);
    constexpr int kRestartCycles = 64;
    constexpr auto kStopDeadline = std::chrono::seconds(2);
    for (int cycle = 0; cycle < kRestartCycles; ++cycle) {
        collectedIdle.store(false);
        Require(worker.Start(IdleSockets, AlwaysBusy, true), "idle worker start failed");
        const auto waitDeadline = std::chrono::steady_clock::now() + kStopDeadline;
        while (!collectedIdle.load() && std::chrono::steady_clock::now() < waitDeadline) {
            std::this_thread::yield();
        }
        Require(collectedIdle.load(), "idle worker did not prepare select");
        const auto stopTime = std::chrono::steady_clock::now();
        worker.Stop();
        Require(std::chrono::steady_clock::now() - stopTime < kStopDeadline, "stop did not wake idle select");
        worker.Stop();
    }
}

static void TestBusyDrainStop() {
    AllocateFixture();
    sockaddr_in destination;
    ip_sockets[NS_CLIENT] = OpenSocket(destination);
    sockaddr_in senderAddress;
    SOCKET sender = OpenSocket(senderAddress);
    const std::vector<byte> packet = { 1, 2, 3, 4 };
    SendPacket(sender, destination, packet);
    std::recursive_mutex mutex;
    cse::NetworkWorker worker(mutex);
    busyCalls.store(0);
    Require(worker.Start(ReadySocket, AlwaysBusy, true), "busy worker start failed");
    constexpr auto kStopDeadline = std::chrono::seconds(2);
    const auto waitDeadline = std::chrono::steady_clock::now() + kStopDeadline;
    while (busyCalls.load() < 100 && std::chrono::steady_clock::now() < waitDeadline) {
        std::this_thread::yield();
    }
    Require(busyCalls.load() >= 100, "busy worker did not repeatedly drain ready sources");
    const auto stopTime = std::chrono::steady_clock::now();
    worker.Stop();
    Require(std::chrono::steady_clock::now() - stopTime < kStopDeadline, "busy drain ignored stop between packets");
    CloseSocket(sender);
    FlushFixture();
}

static void TestCreationFailureAndRestart() {
    AllocateFixture();
#if defined(_WIN32)
    Require(WSACleanup() == 0, "could not suspend Winsock for resource failure fixture");
#else
    rlimit descriptorLimit;
    Require(getrlimit(RLIMIT_NOFILE, &descriptorLimit) == 0, "cannot read descriptor resource limit");
    rlimit exhaustedLimit = descriptorLimit;
    exhaustedLimit.rlim_cur = 0;
    Require(setrlimit(RLIMIT_NOFILE, &exhaustedLimit) == 0, "cannot exhaust descriptor resource limit");
#endif
    bool failed = false;
    try {
        NET_StartThread();
    } catch (const std::runtime_error&) {
        failed = true;
    }
#if defined(_WIN32)
    WSADATA data;
    Require(WSAStartup(MAKEWORD(1, 1), &data) == 0, "could not restore Winsock after failure fixture");
#else
    Require(setrlimit(RLIMIT_NOFILE, &descriptorLimit) == 0, "cannot restore descriptor resource limit");
#endif
    Require(failed && !use_thread && !net_thread_initialized, "failed start must leave a stopped synchronous state");
    NET_StopThread();
#if !defined(_WIN32)
    // Leave room for the receive wake socket but deny its sender. This reaches
    // the partial-creation cleanup path with real OS resources, without hooks.
    int nextDescriptor = dup(STDIN_FILENO);
    Require(nextDescriptor >= 0, "cannot find next available descriptor");
    close(nextDescriptor);
    exhaustedLimit.rlim_cur = static_cast<rlim_t>(nextDescriptor + 1);
    Require(setrlimit(RLIMIT_NOFILE, &exhaustedLimit) == 0, "cannot limit partial wake socket creation");
    use_thread = 1;
    failed = false;
    try {
        NET_StartThread();
    } catch (const std::runtime_error&) {
        failed = true;
    }
    Require(setrlimit(RLIMIT_NOFILE, &descriptorLimit) == 0, "cannot restore descriptor resource limit");
    Require(failed && !use_thread && !net_thread_initialized, "partial wake socket creation was not rolled back");
    int availableDescriptor = dup(STDIN_FILENO);
    Require(availableDescriptor == nextDescriptor, "failed worker start leaked its receive wake socket");
    close(availableDescriptor);
#endif
    use_thread = 1;
    NET_StartThread();
    Require(net_thread_initialized != false, "worker could not restart after resource creation failure");
    FlushFixture();
}

template <typename Test>
static void RunCase(const char* name, Test test) {
    std::printf("RUN %s\n", name);
    std::fflush(stdout);
    test();
    std::printf("PASS %s\n", name);
}

static void TestConfigureAndShutdownLifetimes() {
    AllocateFixture();
    NET_StartThread();
    NET_Config(true);
    Require(net_thread_initialized && net_configured, "configure must restart worker against new sockets");
    sockaddr_in senderAddress;
    SOCKET sender = OpenSocket(senderAddress);
    const std::vector<byte> firstPacket = { 1, 2, 3, 4, 0xaa };
    SendPacket(sender, configuredAddresses[NS_CLIENT], firstPacket);
    WaitForPackets(1);
    NET_Config(false);
    Require(net_thread_initialized && !net_configured, "deconfigure must resume worker with empty socket set");
    RequirePacket(NS_CLIENT, firstPacket, senderAddress.sin_port);
    NET_Config(true);
    const std::vector<byte> secondPacket = { 5, 6, 7, 8, 0xbb };
    SendPacket(sender, configuredAddresses[NS_SERVER], secondPacket);
    WaitForPackets(2);
    NET_Shutdown();
    Require(!net_thread_initialized && !net_configured, "shutdown must leave network worker joined");
    Require(allocations.empty(), "shutdown must free both queued and pooled packets");
    CloseSocket(sender);
    FlushFixture();
}

int main() {
#if defined(_WIN32)
    WSADATA data;
    Require(WSAStartup(MAKEWORD(1, 1), &data) == 0, "Winsock startup failed");
#endif
    try {
        RunCase("private-native-queue-layouts", TestPrivateQueueLayouts);
        RunCase("source-drain-order-forever", [] { TestSourceDrainOrderAndBytes(true); });
        RunCase("source-drain-order-poll", [] { TestSourceDrainOrderAndBytes(false); });
        RunCase("preallocated-and-heap-packet-ownership", TestPreallocatedAndHeapPacketOwnership);
        RunCase("stop-joins-in-flight-packet", [] { TestStopJoinsInFlightPacket(false); });
        RunCase("flush-joins-in-flight-packet", [] { TestStopJoinsInFlightPacket(true); });
        RunCase("idle-wake-and-64-restarts", TestIdleWakeAndRepeatedRestart);
        RunCase("busy-drain-stop", TestBusyDrainStop);
        RunCase("creation-failure-and-restart", TestCreationFailureAndRestart);
        RunCase("configure-and-shutdown-lifetimes", TestConfigureAndShutdownLifetimes);
        std::puts("network worker fixtures passed");
    } catch (const std::exception& error) {
        std::fprintf(stderr, "FAIL: %s\n", error.what());
        NET_StopThread();
        NET_FlushQueues();
        return 1;
    }
#if defined(_WIN32)
    WSACleanup();
#endif
    return 0;
}
