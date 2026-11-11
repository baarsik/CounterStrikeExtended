#include "transport_context.h"

#if !defined(_WIN32)
constexpr int MAX_PATH = 260;
constexpr int TRUE = 1;
constexpr int FALSE = 0;
#endif
inline double realtime = 0;
inline qboolean gfExtendedError = FALSE;
inline netadr_t g_GameServerAddress{};
inline FixtureCvar cl_resend{6.0f};
inline int disconnectCalls = 0;
inline std::vector<std::string> disconnectReasons;
inline bool Host_IsServerActive() {
    return false;
}
namespace {
int connectPacketCalls = 0;
int leaveGroupCalls = 0;

bool NoExcludedResendCalls() {
    return connectPacketCalls == 0 && leaveGroupCalls == 0;
}
}

inline void CL_SendConnectPacket() {
    ++connectPacketCalls;
}
inline void NET_LeaveGroup(netsrc_t, netadr_t) {
    ++leaveGroupCalls;
}
inline void COM_ExplainDisconnection(qboolean, const char* reason, ...) {
    disconnectReasons.emplace_back(reason);
}
inline void CL_Disconnect() {
    ++disconnectCalls;
}
inline void Cvar_SetValue(const char* name, float value) {
    Require(std::strcmp(name, "cl_resend") == 0, "only the retained resend cvar clamp is reachable");
    cl_resend.value = value;
}
inline unsigned short BigShort(unsigned short value) {
    return htons(value);
}
inline void SetLoadingProgressBarStatusText(const char*) {}
inline int Q_strlen(const char* text) {
    return static_cast<int>(std::strlen(text));
}
#include "actual_resend.h"

namespace {
constexpr SOCKET kClientSocket = 17;
constexpr SOCKET kServerSocket = 19;
constexpr unsigned short kDestinationPort = 27439;
constexpr byte kRequest[] = {0xff, 0xff, 0xff, 0xff, 'g', 'e', 't', 'c', 'h', 'a', 'l', 'l', 'e', 'n', 'g', 'e',
    ' ', 's', 't', 'e', 'a', 'm', '\n'};
#if defined(_WIN32)
constexpr int kHostUnreachable = WSAEHOSTUNREACH;
constexpr int kNetworkUnreachable = WSAENETUNREACH;
constexpr int kWouldBlock = WSAEWOULDBLOCK;
constexpr int kBadDescriptor = WSAENOTSOCK;
constexpr int kInvalidArgument = WSAEINVAL;
constexpr int kAddressUnavailable = WSAEADDRNOTAVAIL;
constexpr int kNoBuffers = WSAENOBUFS;
constexpr int kConnectionRefused = WSAECONNREFUSED;
constexpr int kConnectionReset = WSAECONNRESET;
#else
constexpr int kHostUnreachable = EHOSTUNREACH;
constexpr int kNetworkUnreachable = ENETUNREACH;
constexpr int kWouldBlock = EAGAIN;
constexpr int kBadDescriptor = EBADF;
constexpr int kInvalidArgument = EINVAL;
constexpr int kAddressUnavailable = EADDRNOTAVAIL;
constexpr int kNoBuffers = ENOBUFS;
constexpr int kConnectionRefused = ECONNREFUSED;
constexpr int kConnectionReset = ECONNRESET;
#endif
#if defined(__APPLE__) && !defined(CSE_NETWORK_NON_APPLE_CONTROL)
constexpr bool kAppleReachability = true;
#else
constexpr bool kAppleReachability = false;
#endif

netadr_t Destination() {
    netadr_t destination{};
    char text[] = "10.211.55.3:27439";
    Require(NET_StringToAdr(text, &destination) != 0, "actual numeric address parser");
    Require(destination.type == NA_IP && destination.port == htons(kDestinationPort), "numeric address and port");
    return destination;
}

void ResetSend(int error = kWouldBlock) {
    ResetApi();
    io = {};
    io.forceSend = true;
    io.sendResult = -1;
    io.sendError = error;
    cls = {};
    ip_sockets[NS_CLIENT] = kClientSocket;
    ip_sockets[NS_SERVER] = kServerSocket;
}

void Send(netadr_t destination = Destination(), netsrc_t source = NS_CLIENT) {
    NET_SendPacket(source, sizeof(kRequest), const_cast<byte*>(kRequest), destination);
}

void RequireFatal(int error, const char* description) {
    ResetSend(error);
    try {
        Send();
    } catch (const std::runtime_error& failure) {
        Require(std::strstr(failure.what(), "NET_SendPacket ERROR:") != nullptr, "actual fatal boundary diagnostic");
        Require(io.sendCalls == 1, "fatal send attempts exactly one native datagram");
        return;
    }
    Require(false, description);
}

void Reachability(int error) {
    if (!kAppleReachability) {
        RequireFatal(error, "non-Apple reachability remains fatal");
        return;
    }
    ResetSend(error);
    Send();
    Require(io.sendCalls == 1 && api.diagnostics.size() == 1 && api.debugDiagnosticCalls == 0,
        "reachability failure reports visibly and returns without an immediate retry");
    Require(api.diagnostics[0].find("10.211.55.3:27439") != std::string::npos,
        "visible reachability diagnostic identifies the failed destination");
    Require(api.diagnostics[0].find(std::to_string(error)) != std::string::npos,
        "reachability diagnostic identifies numeric errno");
    Require(api.diagnostics[0].find(std::strerror(error)) != std::string::npos,
        "reachability diagnostic retains native error text despite diagnostic errno clobber");
    io.forceSend = false;
    Send();
    Require(io.sendCalls == 2 && io.sent.size() == 2 && io.sent[1] == std::vector<byte>(std::begin(kRequest), std::end(kRequest)),
        "a later successful call sends the original challenge bytes once");
    Require(api.diagnostics.size() == 1, "success emits no error and does not invent a resend");
}

void Diagnostics() {
    if (!kAppleReachability) {
        RequireFatal(kHostUnreachable, "non-Apple diagnostic control retains fatal behavior");
        return;
    }
    ResetSend(kHostUnreachable);
    fixtureClock = 0;
    Send();
    constexpr int kBurstSends = 1000;
    for (int index = 0; index < kBurstSends; ++index) {
        fixtureClock = 0.999;
        io.sendError = index % 2 ? kHostUnreachable : kNetworkUnreachable;
        Send();
    }
    Require(api.diagnostics.size() == 1 && io.sendCalls == kBurstSends + 1,
        "one visible diagnostic bounds a high-frequency reachability failure burst");
    fixtureClock = 1.0;
    Send();
    Require(api.diagnostics.size() == 2 && io.sendCalls == kBurstSends + 2,
        "a persistent failure becomes visible again after the diagnostic interval");
}

void Errors() {
    RequireFatal(kBadDescriptor, "invalid socket must remain fatal");
    RequireFatal(kInvalidArgument, "invalid argument must remain fatal");
    for (int error : {kWouldBlock, kConnectionRefused, kConnectionReset}) {
        ResetSend(error);
        Send();
        Require(io.sendCalls == 1 && api.diagnostics.empty(), "retained ignored transient sends remain silent");
    }
    for (int error : {kAddressUnavailable, kNoBuffers}) {
        ResetSend(error);
        Send();
        Require(io.sendCalls == 1 && api.diagnostics.size() == 1, "existing client warning errors remain nonfatal");
    }
    ResetSend(kAddressUnavailable);
    auto broadcast = Destination();
    broadcast.type = NA_BROADCAST;
    Send(broadcast);
    Require(api.diagnostics.empty() && io.sentAddress.sin_addr.s_addr == INADDR_BROADCAST,
        "broadcast address-unavailable remains silently ignored");
    for (int error : {kHostUnreachable, kNetworkUnreachable}) {
        ResetSend(error);
        bool fatal = false;
        try {
            Send(broadcast);
        } catch (const std::runtime_error&) {
            fatal = true;
        }
        Require(fatal && io.sendCalls == 1, "broadcast unreachable errors retain their original fatal behavior");
    }
    for (int error : {kHostUnreachable, kNetworkUnreachable, kBadDescriptor, kInvalidArgument}) {
        ResetSend(error);
        cls.state = ca_dedicated;
        Send(Destination(), NS_SERVER);
        Send(Destination(), NS_SERVER);
        Require(io.sendCalls == 2 && api.diagnostics.size() == 2,
            "dedicated errors retain their existing continue-and-print policy without new throttling");
    }
}

void Packet() {
    ResetSend();
    io.forceSend = false;
    Send();
    Require(io.sent.size() == 1 && io.sent[0] == std::vector<byte>(std::begin(kRequest), std::end(kRequest))
        && io.sentSocket == kClientSocket && io.sentAddress.sin_family == AF_INET
        && io.sentAddress.sin_port == htons(kDestinationPort) && io.sentAddress.sin_addr.s_addr == htonl(0x0ad33703),
        "actual packet owner preserves destination, source socket and exact unsplit bytes");
    auto broadcast = Destination();
    broadcast.type = NA_BROADCAST;
    Send(broadcast, NS_SERVER);
    Require(io.sentSocket == kServerSocket && io.sentAddress.sin_addr.s_addr == INADDR_BROADCAST,
        "successful broadcast uses the server socket and broadcast destination");
    ip_sockets[NS_CLIENT] = 0;
    Send();
    Require(io.sendCalls == 2, "closed zero socket does not send");
    auto loopback = Destination();
    loopback.type = NA_LOOPBACK;
    use_thread = 1;
    net_thread_initialized = 1;
    Send(loopback);
    Require(io.sendCalls == 2 && loopbacks[NS_SERVER].send == 1
        && loopbacks[NS_SERVER].msgs[0].datalen == sizeof(kRequest)
        && std::memcmp(loopbacks[NS_SERVER].msgs[0].data, kRequest, sizeof(kRequest)) == 0,
        "loopback uses the actual peer ring with original bytes and no native send");
    Require(net_thread_mutex.try_lock(), "loopback releases its actual mutex");
    net_thread_mutex.unlock();
    std::vector<byte> payload(1401, 0x5a);
    io = {};
    NET_SendPacket(NS_SERVER, static_cast<int>(payload.size()), payload.data(), Destination());
    std::vector<byte> first{0xfe, 0xff, 0xff, 0xff, 2, 0, 0, 0, 0x02};
    std::vector<byte> second{0xfe, 0xff, 0xff, 0xff, 2, 0, 0, 0, 0x12};
    first.insert(first.end(), 1391, 0x5a);
    second.insert(second.end(), 10, 0x5a);
    Require(io.sent == std::vector<std::vector<byte>>{first, second},
        "NET_SendPacket delegates unchanged split sequence, header, length and payload bytes to NET_SendLong");
    io = {};
    io.forceSend = true;
    io.sendResult = -1;
    io.sendError = kWouldBlock;
    NET_SendPacket(NS_SERVER, static_cast<int>(payload.size()), payload.data(), Destination());
    Require(io.sendCalls == 1, "actual split packet owner stops at its first failed native send");
    io = {};
    fakeRoute = true;
    NET_SendPacket(NS_SERVER, sizeof(kRequest), const_cast<byte*>(kRequest), Destination());
    Require(io.sendCalls == 0 && fakeSendCalls == 1, "packet owner retains existing FakeIP delegation");
}

void StartResend(int error) {
    ResetSend(error);
    std::strcpy(cls.servername, "10.211.55.3:27439");
    cls.connect_time = -99999;
    realtime = 0;
    fixtureClock = 0;
    disconnectCalls = 0;
    connectPacketCalls = 0;
    leaveGroupCalls = 0;
    disconnectReasons.clear();
    cl_resend.value = 6.0f;
}

void Tick(double time) {
    realtime = time;
    fixtureClock = time;
    CL_CheckForResend();
    Require(NoExcludedResendCalls(), "unicast resend must not enter local/passive connect or leave a multicast group");
}

void Resend(int error, bool laterSuccess) {
    // Non-Apple controls exercise the same retained retry owner with its existing ignored errno.
    StartResend(kAppleReachability ? error : kWouldBlock);
    Tick(0);
    Require(cls.state == ca_connecting && cls.connect_retry == 1 && io.sendCalls == 1,
        "failed first challenge remains in the existing connecting state");
    Tick(5.999);
    Require(io.sendCalls == 1, "no challenge before retained six-second retry deadline");
    io.forceSend = !laterSuccess;
    Tick(6.0);
    Require(io.sendCalls == 2 && cls.connect_retry == 2
        && io.sent[1] == std::vector<byte>(std::begin(kRequest), std::end(kRequest)),
        "second attempt comes only from the existing timed resend with exact challenge bytes");
    // A send success alone is not a connection acceptance. Without a reply the same bound must expire.
    io.forceSend = true;
    Tick(12.0);
    Tick(18.0);
    Require(io.sendCalls == 4 && cls.connect_retry == 4 && cls.state == ca_connecting,
        "exactly four retained attempts with no reply");
    Tick(23.999);
    Require(disconnectCalls == 0 && io.sendCalls == 4, "wait for the final reply interval");
    Tick(24.0);
    Require(io.sendCalls == 4 && cls.state == ca_disconnected && cls.connect_retry == 0
        && cls.connect_time == -99999 && disconnectCalls == 1 && disconnectReasons.size() == 1
        && disconnectReasons[0] == "#GameUI_CouldNotContactGameServer",
        "bounded retry exhaustion follows the actual normal disconnect path, never a fifth send");
    const netadr_t cleared{};
    Require(std::memcmp(&g_GameServerAddress, &cleared, sizeof(cleared)) == 0, "timeout clears the pending server address");
    Tick(100.0);
    Require(io.sendCalls == 4 && disconnectCalls == 1, "disconnected client stops resending");
    std::printf("actual_resend_attempts=4 timeout_seconds=24 later_send_success=%d\n", laterSuccess);
}

void ResendUnexpectedPaths() {
    constexpr double kPassiveTimeoutTime = 31.0;
    constexpr double kResetConnectTime = -99999;
    constexpr int kPendingRetryCount = 2;
    const netadr_t cleared{};

    StartResend(kWouldBlock);
    Require(NoExcludedResendCalls(), "new resend starts without excluded calls");
    cls.state = ca_connecting;
    cls.passive = TRUE;
    cls.connect_time = 0;
    cls.connect_retry = kPendingRetryCount;
    cls.challenge = 1;
    g_GameServerAddress = Destination();
    const netadr_t pendingAddress = g_GameServerAddress;
    realtime = kPassiveTimeoutTime;
    fixtureClock = kPassiveTimeoutTime;
    CL_CheckForResend();
    Require(leaveGroupCalls == 1 && connectPacketCalls == 1 && io.sendCalls == 0,
        "actual passive challenge fallback leaves the group and connects once without a unicast datagram");
    Require(!NoExcludedResendCalls(), "ordinary unicast guard rejects actual passive challenge fallback");
    Require(cls.state == ca_connecting && cls.passive == FALSE && cls.challenge == 1
        && cls.connect_time == 0 && cls.connect_retry == kPendingRetryCount
        && disconnectCalls == 0 && disconnectReasons.empty()
        && std::memcmp(&g_GameServerAddress, &pendingAddress, sizeof(pendingAddress)) == 0,
        "challenge fallback preserves pending connection state without disconnecting");

    StartResend(kWouldBlock);
    Require(NoExcludedResendCalls(), "new resend clears both prior excluded-call counts");
    cls.state = ca_connecting;
    cls.passive = TRUE;
    cls.connect_time = 0;
    cls.connect_retry = kPendingRetryCount;
    cls.challenge = 0;
    g_GameServerAddress = Destination();
    realtime = kPassiveTimeoutTime;
    fixtureClock = kPassiveTimeoutTime;
    CL_CheckForResend();
    Require(leaveGroupCalls == 1 && connectPacketCalls == 0 && io.sendCalls == 0,
        "actual passive timeout without challenge leaves once without connecting or a unicast datagram");
    Require(!NoExcludedResendCalls(), "ordinary unicast guard rejects actual passive timeout without challenge");
    Require(cls.state == ca_disconnected && cls.passive == FALSE && cls.challenge == 0
        && cls.connect_time == kResetConnectTime && cls.connect_retry == 0
        && disconnectCalls == 0 && disconnectReasons.size() == 1
        && disconnectReasons[0] == "Joining multicast group failed after %.0f seconds."
        && std::memcmp(&g_GameServerAddress, &cleared, sizeof(cleared)) == 0,
        "no-challenge timeout explains disconnection and resets pending state without CL_Disconnect");
}

void ResendTiming() {
    StartResend(kWouldBlock);
    cl_resend.value = 0.0f;
    Tick(0);
    Tick(1.499);
    Require(io.sendCalls == 1, "minimum retry interval prevents early send");
    Tick(1.5);
    Require(io.sendCalls == 2, "retained lower clamp sends at 1.5 seconds");
    StartResend(kWouldBlock);
    cl_resend.value = 99.0f;
    Tick(0);
    Tick(19.999);
    Require(io.sendCalls == 1, "maximum retry interval prevents early send");
    Tick(20.0);
    Require(io.sendCalls == 2, "retained upper clamp sends at 20 seconds");
    cls.state = ca_connected;
    Tick(100.0);
    Require(io.sendCalls == 2, "connected state stops challenge resends");
}
}

int main(int argc, const char** argv) {
    Require(argc == 2, "one controlled send scenario required");
    try {
        const std::string scenario = argv[1];
        if (scenario == "host-unreachable") {
            Reachability(kHostUnreachable);
        } else if (scenario == "network-unreachable") {
            Reachability(kNetworkUnreachable);
        } else if (scenario == "diagnostics") {
            Diagnostics();
        } else if (scenario == "errors") {
            Errors();
        } else if (scenario == "packet") {
            Packet();
        } else if (scenario == "resend-host") {
            Resend(kHostUnreachable, true);
        } else if (scenario == "resend-network") {
            Resend(kNetworkUnreachable, true);
        } else if (scenario == "exhaust-host") {
            Resend(kHostUnreachable, false);
        } else if (scenario == "exhaust-network") {
            Resend(kNetworkUnreachable, false);
        } else if (scenario == "resend-unexpected-paths") {
            ResendUnexpectedPaths();
        } else if (scenario == "resend-timing") {
            ResendTiming();
        } else {
            Require(false, "unknown send scenario");
        }
    } catch (const std::runtime_error& failure) {
        std::fprintf(stderr, "FAILED: unexpected actual Sys_Error: %s\n", failure.what());
        return EXIT_FAILURE;
    }
    std::printf("actual_send_owner_passed=1 apple_reachability=%d\n", kAppleReachability);
    return EXIT_SUCCESS;
}
