#include <algorithm>
#include <array>
#include <cerrno>
#include <climits>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <limits>
#include <set>
#include <string>
#include <vector>

// Test observation access only: declarations, field types, object layout and methods remain real SDK ones.
#define private public
#define protected public
#include "HLTV/Core/Network.h"
#undef protected
#undef private
#include "HLTV/Core/NetSocket.h"
#include "HLTV/common/NetPacket.h"
#include "fixture_system.h"
#include "fixture_channel.h"
#include "syscalls.h"
#include "public/interface_registry.h"

// Controlled standalone module-local registry storage; actual inline registration remains unchanged.
InterfaceReg* InterfaceReg::s_pInterfaceRegs = nullptr;

namespace {

void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "FAIL %s errno=%d\n", message, errno);
        std::exit(EXIT_FAILURE);
    }
}

#ifdef _WIN32
constexpr int WouldBlock = WSAEWOULDBLOCK;
constexpr int ResetError = WSAECONNRESET;
constexpr int Refused = WSAECONNREFUSED;
constexpr int Unavailable = WSAEADDRNOTAVAIL;
constexpr int TooLarge = WSAEMSGSIZE;
constexpr int Interrupted = WSAEINTR;
constexpr int Invalid = WSAEINVAL;
#else
constexpr int WouldBlock = EWOULDBLOCK;
constexpr int ResetError = ECONNRESET;
constexpr int Refused = ECONNREFUSED;
constexpr int Unavailable = EADDRNOTAVAIL;
constexpr int TooLarge = EMSGSIZE;
constexpr int Interrupted = EINTR;
constexpr int Invalid = EINVAL;
#endif

struct Reply {
    TestTransfer count;
    int error = 0;
    std::vector<unsigned char> bytes;
};

std::deque<Reply> receives;
std::deque<Reply> sends;
std::vector<std::vector<unsigned char>> transmitted;
std::vector<std::string> operations;
std::set<TestSocket> liveDescriptors;
TestSocket nextDescriptor = 42;
int failedStep = 0;
int setupSteps = 0;
int closes = 0;
bool failedClose = false;
int invalidCloses = 0;
int receiveCalls = 0;
int sendCalls = 0;
int randomCalls = 0;
int reuseBytes = 0;
int ttlBytes = 0;
unsigned int selectedTtl = 0;
std::vector<TestLength> addressLengths;
NetSocket* observedSocket = nullptr;
bool invalidatedBeforeClose = false;

void Error(int value) {
#ifdef _WIN32
    WSASetLastError(value);
#endif
    errno = value;
}

int LastError() {
#ifdef _WIN32
    return WSAGetLastError();
#else
    return errno;
#endif
}

bool Step(const char* name) {
    operations.emplace_back(name);
    ++setupSteps;
    if (setupSteps == failedStep) {
        Error(Invalid);
        return false;
    }
    return true;
}

void Hex(const std::vector<unsigned char>& bytes) {
    for (unsigned char value : bytes) {
        std::printf("%02x", value);
    }
}

struct Context {
    ControlledSystem system;
    Network network;
    NetSocket socketObject;

    Context() {
        network.m_System = &system;
        network.m_Sockets.Init();
        socketObject.m_Network = &network;
        socketObject.m_System = &system;
        socketObject.m_Channels.Init();
        socketObject.m_IncomingPackets.Init();
        socketObject.m_Socket = 42;
        observedSocket = &socketObject;
    }
};

NetAddress Address() {
    NetAddress result;
    result.m_IP[0] = 192;
    result.m_IP[1] = 0;
    result.m_IP[2] = 2;
    result.m_IP[3] = 7;
    result.SetPort_(27020);
    return result;
}

std::vector<unsigned char> Payload(int size) {
    std::vector<unsigned char> result(static_cast<std::size_t>(size));
    for (int index = 0; index < size; ++index) {
        result[static_cast<std::size_t>(index)] = static_cast<unsigned char>(index % 251);
    }
    return result;
}

void Send(const std::string& mode) {
    Context context;
    NetAddress target = Address();
    int length = 16;
    if (mode == "send-short") {
        length = 3;
    } else if (mode == "send-long") {
        length = 1500;
    } else if (mode == "send-max") {
        length = 1400;
    }
    const auto bytes = Payload(length);
    if (mode == "send-zero-count") {
        sends.push_back({0, 0, {}});
    }
    const bool result = context.socketObject.SendPacket(&target, bytes.data(), length);
    Require(result && context.system.warnings == 0, "actual send accepts valid packet");
    const int expected = mode == "send-zero-count" ? 0 : length < 16 ? 16 : length;
    Require(context.socketObject.m_BytesOut == expected + 28, "retained successful byte statistics");
    if (length < 16) {
        Require(transmitted.size() == 1 && transmitted[0].size() == 16, "short packet is padded to sixteen bytes");
        for (std::size_t index = 0; index < transmitted[0].size(); ++index) {
            Require(transmitted[0][index] == (index < bytes.size() ? bytes[index] : 0), "literal short padding bytes");
        }
    } else if (length <= 1400) {
        Require(transmitted.size() == 1 && transmitted[0] == bytes, "direct UDP bytes remain exact");
    } else {
        constexpr std::array<unsigned char, 9> First = {{0xfe, 0xff, 0xff, 0xff, 1, 0, 0, 0, 2}};
        constexpr std::array<unsigned char, 9> Second = {{0xfe, 0xff, 0xff, 0xff, 1, 0, 0, 0, 0x12}};
        Require(transmitted.size() == 2 && transmitted[0].size() == 1400 && transmitted[1].size() == 118,
            "retained split sizes and order");
        Require(std::equal(First.begin(), First.end(), transmitted[0].begin())
            && std::equal(Second.begin(), Second.end(), transmitted[1].begin()), "independent nine-byte split goldens");
        Require(std::equal(bytes.begin(), bytes.begin() + 1391, transmitted[0].begin() + 9)
            && std::equal(bytes.begin() + 1391, bytes.end(), transmitted[1].begin() + 9), "split payload bytes");
    }
    std::printf("RESULT accepted=1 bytesOut=%d sends=%d\n", context.socketObject.m_BytesOut, sendCalls);
}

int ErrorFor(const std::string& mode) {
    if (mode.find("wouldblock") != std::string::npos) {
        return WouldBlock;
    }
    if (mode.find("reset") != std::string::npos) {
        return ResetError;
    }
    if (mode.find("refused") != std::string::npos) {
        return Refused;
    }
    if (mode.find("unavailable") != std::string::npos) {
        return Unavailable;
    }
    if (mode.find("large") != std::string::npos) {
        return TooLarge;
    }
    if (mode.find("interrupt") != std::string::npos) {
        return Interrupted;
    }
    return Invalid;
}

void SendError(const std::string& mode) {
    Context context;
    NetAddress target = Address();
    const int error = ErrorFor(mode);
    const bool tolerated = error == WouldBlock || error == ResetError || error == Refused || error == Unavailable;
    const auto bytes = Payload(mode.find("split") == std::string::npos ? 16 : 1500);
    if (bytes.size() > 1400) {
        sends.push_back({1400, 0, {}});
    }
    sends.push_back({-1, error, {}});
    const bool result = context.socketObject.SendPacket(&target, bytes.data(), static_cast<int>(bytes.size()));
    Require(result == tolerated && context.socketObject.m_BytesOut == 0, "retained tolerated/fatal send error policy");
    Require(context.system.warnings == (tolerated ? 0 : 1) && LastError() == error,
        "send error/errno survives observers");
    Require(sendCalls == (bytes.size() > 1400 ? 2 : 1), "send errors do not retry or continue split sequence");
    std::printf("RESULT accepted=%d warnings=%d sends=%d bytesOut=0\n", result, context.system.warnings, sendCalls);
}

void Queue(const std::string& mode) {
    Context context;
    if (mode == "queue") {
        receives.push_back({5, 0, {0xff, 0xff, 0xff, 0xff, 'A'}});
        receives.push_back({6, 0, {0xff, 0xff, 0xff, 0xff, 'B', 'C'}});
        Require(context.socketObject.DispatchIncoming() == 2, "both datagrams dispatch in order");
        for (const std::vector<unsigned char>& expected : {std::vector<unsigned char>{'A'}, {'B', 'C'}}) {
            NetPacket* packet = context.socketObject.ReceivePacket();
            Require(packet != nullptr && packet->connectionless && packet->seqnr == UINT_MAX && packet->time == 12.5,
                "actual queue packet metadata");
            Require(packet->data.CurrentSize() == 0 && packet->data.maxSize == static_cast<int>(expected.size())
                && std::memcmp(packet->data.data, expected.data(), expected.size()) == 0, "FIFO connectionless bytes");
            std::printf("RX ");
            Hex(expected);
            std::printf("\n");
            context.socketObject.FreePacket(packet);
        }
        Require(context.socketObject.ReceivePacket() == nullptr && context.socketObject.m_BytesIn == 67,
            "queue drains exactly once with retained input statistics");
    } else if (mode == "copy-flush") {
        NetPacket source;
        source.connectionless = true;
        source.time = 3;
        source.address = Address();
        Require(source.data.Resize(4), "source packet buffer");
        const std::array<unsigned char, 4> expected = {{1, 2, 3, 4}};
        source.data.WriteBuf(expected.data(), static_cast<int>(expected.size()));
        context.socketObject.AddPacket(&source);
        source.data.data[0] = 99;
        NetPacket* copy = context.socketObject.ReceivePacket();
        Require(copy != nullptr && std::memcmp(copy->data.data, expected.data(), expected.size()) == 0,
            "queued packet owns a deep copy");
        context.socketObject.FreePacket(copy);
        context.socketObject.AddPacket(&source);
        context.socketObject.Flush();
        Require(context.socketObject.ReceivePacket() == nullptr, "Flush releases queued owned packets");
    } else if (mode == "channel") {
        ControlledChannel first;
        ControlledChannel second;
        first.ready = false;
        first.target.m_IP[0] = 1;
        second.target = Address();
        context.socketObject.AddChannel(&first);
        context.socketObject.AddChannel(&second);
        receives.push_back({5, 0, {0xff, 0xff, 0xff, 0xff, 'A'}});
        Require(context.socketObject.DispatchIncoming() == 1 && second.incoming.size() == 5
            && context.socketObject.ReceivePacket() == nullptr,
            "matching channel gets original datagram before queueing");
        Require(context.socketObject.DrainChannels() == 1 && first.readyCalls == 1 && first.keepCalls == 0
            && second.readyCalls == 1 && second.keepCalls == 1 && second.connectedCalls == 1 && second.transmits == 1,
            "retained channel readiness short-circuit and transmit order");
        context.socketObject.RemoveChannel(&first);
        context.socketObject.RemoveChannel(&second);
    }
    std::printf("RESULT bytesIn=%d reads=%d random=%d queue=%d\n", context.socketObject.m_BytesIn,
        receiveCalls, randomCalls, context.socketObject.m_IncomingPackets.CountElements());
}

void Receive(const std::string& mode) {
    Context context;
    NetAddress from;
    if (mode == "receive-zero") {
        receives.push_back({0, 0, {}});
        Require(context.socketObject.ReceivePacketIntern(&from) == 0 && randomCalls == 0,
            "zero datagram stops without RNG");
    } else if (mode == "receive-short") {
        receives.push_back({3, 0, {1, 2, 3}});
        Require(context.socketObject.ReceivePacketIntern(&from) == 0 && context.system.warnings == 1
            && randomCalls == 1, "undersized datagram preserves RNG and warning order");
    } else if (mode == "receive-drop") {
        context.network.m_FakeLoss = 1;
        receives.push_back({5, 0, {0xff, 0xff, 0xff, 0xff, 'A'}});
        Require(context.socketObject.ReceivePacketIntern(&from) == 0 && randomCalls == 1, "fake loss consumes once");
    } else {
        const int error = ErrorFor(mode);
        receives.push_back({-1, error, {}});
        const bool quiet = error == WouldBlock || error == ResetError || error == Refused;
        Require(context.socketObject.ReceivePacketIntern(&from) == 0
            && context.system.warnings == (quiet ? 0 : 1), "retained receive error/warning policy");
        Require(LastError() == error, "receive error survives controlled logger/perror");
    }
    Require(context.socketObject.m_BytesIn == 0, "ignored/error packet does not accrue input bytes");
    std::printf("RESULT warnings=%d reads=%d random=%d bytesIn=0\n",
        context.system.warnings, receiveCalls, randomCalls);
}

void Split() {
    Context context;
    NetAddress from;
    const auto bytes = Payload(1500);
    std::vector<unsigned char> first = {0xfe, 0xff, 0xff, 0xff, 77, 0, 0, 0, 2};
    std::vector<unsigned char> second = {0xfe, 0xff, 0xff, 0xff, 77, 0, 0, 0, 0x12};
    first.insert(first.end(), bytes.begin(), bytes.begin() + 1391);
    second.insert(second.end(), bytes.begin() + 1391, bytes.end());
    receives.push_back({static_cast<TestTransfer>(first.size()), 0, first});
    receives.push_back({static_cast<TestTransfer>(second.size()), 0, second});
    Require(context.socketObject.ReceivePacketIntern(&from) == 0 && context.socketObject.m_BytesIn == 28,
        "incomplete split retains original accounting/return behavior");
    Require(context.socketObject.ReceivePacketIntern(&from) == 1500 && context.socketObject.m_BytesIn == 1556,
        "completed split size and retained accounting");
    Require(std::equal(bytes.begin(), bytes.end(), context.socketObject.m_Buffer),
        "reassembly preserves original bytes");
    std::printf("RESULT length=1500 bytesIn=1556 reads=%d\n", receiveCalls);
}

#ifndef _WIN32
void Count(const std::string& mode) {
    Context context;
    sockaddr target = {};
    const std::array<char, 1> payload = {{'A'}};
    if (mode == "count-receive-overflow") {
        receives.push_back({static_cast<TestTransfer>(INT_MAX) + 1, 0, {}});
        NetAddress from;
        Error(ENOENT);
        Require(context.socketObject.ReceivePacketIntern(&from) == 0 && errno == EOVERFLOW
            && context.socketObject.m_BytesIn == 0 && randomCalls == 0,
            "actual receive count overflow follows the error path before RNG/data/accounting");
        return;
    }
    if (mode == "count-direct-overflow" || mode == "count-split-overflow") {
        const auto bytes = Payload(mode == "count-direct-overflow" ? 16 : 1500);
        NetAddress address = Address();
        sends.push_back({static_cast<TestTransfer>(INT_MAX) + 1, 0, {}});
        Require(!context.socketObject.SendPacket(&address, bytes.data(), static_cast<int>(bytes.size()))
            && errno == EOVERFLOW && context.socketObject.m_BytesOut == 0 && sendCalls == 1,
            "actual direct/split send count overflow stops before accounting or the next fragment");
        return;
    }
    TestTransfer value = 0;
    if (mode == "count-max") {
        value = INT_MAX;
    } else if (mode == "count-overflow") {
        value = static_cast<TestTransfer>(INT_MAX) + 1;
    } else if (mode == "count-wide") {
        value = static_cast<TestTransfer>(UINT_MAX) + 1;
    } else if (mode == "count-native-max") {
        value = (std::numeric_limits<TestTransfer>::max)();
    } else if (mode == "count-error") {
        value = -1;
    } else if (mode == "count-invalid-negative") {
        value = -2;
    }
    Error(ENOENT);
    sends.push_back({value, value == -1 ? ENOENT : 0, {}});
    const int result = context.socketObject.SendShort(payload.data(), 1, 0, &target, sizeof(target));
    const bool overflow = value > INT_MAX || value < -1;
    Require(result == (overflow ? -1 : static_cast<int>(value)), "native transfer count never silently narrows");
    Require(errno == (overflow ? EOVERFLOW : ENOENT), "count preserves error/success errno or identifies overflow");
    std::printf("RESULT count=%d errno=%d\n", result, errno);
}

void Lifetime(const std::string& mode) {
    Context context;
    ControlledChannel channel;
    context.socketObject.m_Socket = -1;
    nextDescriptor = mode == "fd-zero" ? 0 : 42;
    const bool failure = mode.find("failure-") == 0 || mode == "cleanup-close-error";
    failedClose = mode == "cleanup-close-error" || mode == "close-error";
    if (failure) {
        failedStep = mode == "cleanup-close-error" ? 2 : std::atoi(mode.c_str() + std::strlen("failure-"));
    }
    const bool created = context.socketObject.Create(&context.network, 27020, true, false);
    if (failure) {
        Require(!created && context.socketObject.m_Socket == -1 && liveDescriptors.empty(),
            "every Create failure invalidates and releases owned descriptor");
        Require(closes == (failedStep == 1 ? 0 : 1) && LastError() == Invalid,
            "Create cleanup closes once and preserves failing errno");
        context.socketObject.Close();
        Require(invalidCloses == 0 && closes == (failedStep == 1 ? 0 : 1),
            "failed unregistered Close cannot close again");
    } else {
        Require(created, "native descriptor Create succeeds");
        if (mode == "options") {
            Require(reuseBytes == sizeof(int) && ttlBytes == sizeof(unsigned char) && selectedTtl == 32,
                "native socket options use the SDK payloads and original default TTL");
        }
        context.socketObject.AddChannel(&channel);
        NetPacket queued;
        queued.connectionless = true;
        queued.time = 1;
        Require(queued.data.Resize(4), "owned packet before Close");
        queued.data.WriteLong(0x1234);
        context.socketObject.AddPacket(&queued);
        if (mode == "live-create") {
            ControlledSystem otherSystem;
            Network otherNetwork;
            otherNetwork.m_System = &otherSystem;
            context.socketObject.m_BytesIn = 9;
            context.socketObject.m_BytesOut = 11;
            context.socketObject.m_Buffer[0] = 0x5a;
            const std::size_t calls = operations.size();
            Error(0);
            Require(!context.socketObject.Create(&otherNetwork, 99, false, false) && errno == EALREADY
                && context.socketObject.m_Socket == nextDescriptor && context.socketObject.m_Port == 27020
                && context.socketObject.m_Network == &context.network
                && context.socketObject.m_System == &context.system
                && context.socketObject.m_BytesIn == 9 && context.socketObject.m_BytesOut == 11
                && context.socketObject.m_Buffer[0] == 0x5a && context.socketObject.m_Channels.CountElements() == 1
                && context.socketObject.m_IncomingPackets.CountElements() == 1 && operations.size() == calls,
                "live Create rejects before mutation or syscalls");
        }
        context.network.m_Sockets.Add(&context.socketObject);
        if (mode == "fd-zero") {
            receives.push_back({4, 0, {0, 0, 0, 0}});
            context.socketObject.Flush();
            Require(receiveCalls == 2, "fd zero is valid for Flush");
        }
        context.socketObject.Close();
        Require(context.socketObject.m_Socket == -1 && liveDescriptors.empty() && closes == 1
            && invalidatedBeforeClose && context.network.m_Sockets.IsEmpty() && channel.closes == 1
            && context.socketObject.m_IncomingPackets.IsEmpty(), "Close invalidates before release and unregisters");
        liveDescriptors.insert(nextDescriptor); // A different owner reuses the same numeric descriptor.
        context.socketObject.Close();
        Require(liveDescriptors.count(nextDescriptor) == 1 && closes == 1 && invalidCloses == 0 && channel.closes == 1,
            "repeat cleanup cannot close another owner's reused descriptor");
        liveDescriptors.clear();
    }
    std::printf("RESULT creates=%d closes=%d invalid=%d\n", setupSteps, closes, invalidCloses);
}

void Ttl(const std::string& mode) {
    Context context;
    context.socketObject.m_Socket = -1;
    std::string text = mode.substr(4);
    context.system.ttl = &text[0];
    const bool valid = text == "0" || text == "255" || text == "32" || text == "  +7tail" || text == "nonsense";
    const bool created = context.socketObject.Create(&context.network, 27020, true, false);
    Require(created == valid, "legacy TTL text and range policy");
    if (valid) {
        const unsigned int expected = text == "255" ? 255 : text == "32" ? 32
            : text == "0" || text == "nonsense" ? 0 : 7;
        Require(ttlBytes == 1 && selectedTtl == expected, "TTL retains exact valid byte value");
        context.socketObject.Close();
    } else {
        Require(errno == EINVAL && context.socketObject.m_Socket == -1 && liveDescriptors.empty() && closes == 1,
            "invalid or overflowed TTL fails without wrap and preserves cleanup errno");
    }
}
#endif

}

TestSocket CseSocket(int family, int type, int protocol) {
    Require(family == AF_INET && type == SOCK_DGRAM && protocol == IPPROTO_UDP, "actual IPv4 UDP socket request");
    if (!Step("socket")) {
        return static_cast<TestSocket>(-1);
    }
    liveDescriptors.insert(nextDescriptor);
    return nextDescriptor;
}

TestTransfer CseRecvFrom(TestSocket, TestData* data,
#ifdef _WIN32
    int length,
#else
    std::size_t length,
#endif
    int flags, sockaddr* address, TestLength* addressLength) {
    ++receiveCalls;
    operations.emplace_back("recv");
    Require(flags == 0 && address != nullptr && addressLength != nullptr, "actual receive arguments");
    addressLengths.push_back(*addressLength);
    Require(*addressLength == sizeof(sockaddr), "real socket length starts at actual address capacity");
    if (receives.empty()) {
        Error(WouldBlock);
        return -1;
    }
    Reply reply = receives.front();
    receives.pop_front();
    if (reply.count < 0) {
        Error(reply.error);
        return reply.count;
    }
    Require(reply.bytes.size() <= static_cast<std::size_t>(length), "controlled datagram fits receive storage");
    if (!reply.bytes.empty()) {
        std::memcpy(data, reply.bytes.data(), reply.bytes.size());
    }
    sockaddr_in from = {};
    from.sin_family = AF_INET;
    from.sin_addr.s_addr = htonl(0xc0000207u);
    from.sin_port = htons(27020);
    std::memcpy(address, &from, sizeof(from));
    *addressLength = sizeof(from);
    return reply.count;
}

TestTransfer CseSendTo(TestSocket, const TestData* data,
#ifdef _WIN32
    int length,
#else
    std::size_t length,
#endif
    int flags, const sockaddr* address, TestLength addressLength) {
    ++sendCalls;
    operations.emplace_back("send");
    Require(flags == 0 && address != nullptr && addressLength == sizeof(sockaddr), "actual send address/flags");
    const auto* bytes = static_cast<const unsigned char*>(static_cast<const void*>(data));
    transmitted.emplace_back(bytes, bytes + length);
    if (sends.empty()) {
        return static_cast<TestTransfer>(length);
    }
    Reply reply = sends.front();
    sends.pop_front();
    if (reply.count < 0) {
        Error(reply.error);
    }
    return reply.count;
}

int CseSetSocketOption(TestSocket, int level, int option,
#ifdef _WIN32
    const char* value,
#else
    const void* value,
#endif
    TestLength length) {
    const bool reuse = level == SOL_SOCKET && option == SO_REUSEADDR;
    const bool broadcast = level == SOL_SOCKET && option == SO_BROADCAST;
    if (reuse) {
        reuseBytes = length;
    }
    if (!broadcast && !reuse) {
        ttlBytes = length;
        selectedTtl = 0;
        std::memcpy(&selectedTtl, value, (std::min)(static_cast<std::size_t>(length), sizeof(selectedTtl)));
    }
    return Step(broadcast ? "broadcast" : reuse ? "reuse" : "ttl") ? 0 : -1;
}

int CseBind(TestSocket, const sockaddr* address, TestLength length) {
    Require(address->sa_family == AF_INET && length == sizeof(sockaddr_in), "actual bind family/length");
    return Step("bind") ? 0 : -1;
}

int CseShutdown(TestSocket, int) {
    operations.emplace_back("shutdown");
    return 0;
}

int CseCloseSocket(TestSocket value) {
    operations.emplace_back("close");
    ++closes;
    invalidCloses += liveDescriptors.erase(value) == 0;
#ifndef _WIN32
    invalidatedBeforeClose = observedSocket != nullptr && observedSocket->m_Socket == -1;
#endif
    Error(failedClose ? Interrupted : EBADF); // Cleanup must retain the original failing operation errno.
    return failedClose ? -1 : 0;
}

void CsePerror(const char*) {
    operations.emplace_back("perror");
}
#ifdef _WIN32
int CseIoctlSocket(SOCKET, long, u_long* value) {
#else
int CseIoctlSocket(int, unsigned long command, ...) {
    va_list args;
    va_start(args, command);
    unsigned int* value = va_arg(args, unsigned int*);
    va_end(args);
#endif
    Require(*value == 1, "nonblocking ioctl flag");
    return Step("ioctl") ? 0 : -1;
}

#ifdef _WIN32
int CseGetHostname(char*, int) {
#else
int CseGetHostname(char*, std::size_t) {
#endif
    Require(false, "DNS/Network Init is forbidden in the controlled fixture");
    return -1;
}

hostent* CseGetHostByName(const char*) {
    Require(false, "real DNS is forbidden");
    return nullptr;
}
#ifdef _WIN32
int CseStartup(WORD, LPWSADATA) {
    Require(false, "Network startup is forbidden");
    return -1;
}
int CseCleanup() {
    Require(false, "Network cleanup is forbidden");
    return -1;
}
#endif
extern "C" float RandomFloat(float low, float high) {
    Require(low == 0 && high == 1, "retained random range");
    ++randomCalls;
    return 0.5f;
}

int main(int argc, char** argv) {
    Require(argc == 2, "one isolated fixture case");
    const std::string mode = argv[1];
    if (mode == "send-short" || mode == "send-long" || mode == "send-max" || mode == "send"
        || mode == "send-zero-count") {
        Send(mode);
    } else if (mode.find("send-error-") == 0) {
        SendError(mode);
    } else if (mode.find("receive-") == 0) {
        Receive(mode);
    } else if (mode == "queue" || mode == "copy-flush" || mode == "channel") {
        Queue(mode);
    } else if (mode == "split") {
        Split();
    }
#ifndef _WIN32
    else if (mode.find("count-") == 0) {
        Count(mode);
    } else if (mode.find("ttl-") == 0) {
        Ttl(mode);
    } else if (mode.find("failure-") == 0 || mode == "fd-zero" || mode == "live-create"
        || mode == "close" || mode == "options" || mode == "cleanup-close-error"
        || mode == "close-error") {
        Lifetime(mode);
    }
#endif
    else {
        Require(false, "known isolated case");
    }
    for (const auto& bytes : transmitted) {
        std::printf("TX ");
        Hex(bytes);
        std::printf("\n");
    }
    std::printf("TRACE");
    for (const auto& operation : operations) {
        std::printf(" %s", operation.c_str());
    }
    std::printf("\nPASS %s\n", argv[1]);
    return EXIT_SUCCESS;
}
