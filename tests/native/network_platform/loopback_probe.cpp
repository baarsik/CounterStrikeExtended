#include "transport_context.h"
#include <chrono>

namespace {
constexpr std::size_t kPayloadBytes = 1401;
constexpr int kMaximumWaits = 50;
constexpr auto kMaximumRuntime = std::chrono::seconds(5);
using Clock = std::chrono::steady_clock;

std::vector<byte> Receive(SOCKET socket, unsigned short senderPort, Clock::time_point deadline) {
    ip_sockets[0] = socket;
    for (int attempt = 0; attempt < kMaximumWaits; ++attempt) {
        Require(Clock::now() < deadline, "total loopback deadline");
        Require(NET_Sleep() >= 0, "bounded real loopback readiness");
        std::array<char, MAX_ROUTABLE_PACKET + 1> data{};
        sockaddr_in from{};
        int fromLength = sizeof(from);
        const int count = NET_PosixReceiveFrom(socket, data.data(), data.size(), 0,
            reinterpret_cast<sockaddr*>(&from), &fromLength);
        if (count >= 0) {
            Require(count <= MAX_ROUTABLE_PACKET, "oversize datagram is not accepted after truncation");
            Require(fromLength == sizeof(from) && from.sin_family == AF_INET
                && from.sin_addr.s_addr == htonl(INADDR_LOOPBACK) && from.sin_port == senderPort,
                "exact owned loopback sender address and port only");
            return std::vector<byte>(data.begin(), data.begin() + count);
        }
        Require(errno == EAGAIN || errno == EWOULDBLOCK, "only nonblocking retry allowed");
    }
    Require(false, "loopback receive bounded deadline");
    return {};
}

void Run() {
    const auto deadline = Clock::now() + kMaximumRuntime;
    liveLoopback = true;
    char interfaceName[] = "127.0.0.1";
    const SOCKET receiver = NET_IPSocket(interfaceName, PORT_ANY, false);
    Require(receiver > 0, "owned receiver created with actual production open");
    const SOCKET sender = NET_IPSocket(interfaceName, PORT_ANY, false);
    Require(sender > 0 && sender != receiver, "independently owned sender");
    sockaddr_in destination{};
    socklen_t destinationLength = sizeof(destination);
    Require(getsockname(receiver, reinterpret_cast<sockaddr*>(&destination), &destinationLength) == 0,
        "query owned ephemeral receiver port");
    Require(destination.sin_addr.s_addr == htonl(INADDR_LOOPBACK), "no wildcard/public bind");
    sockaddr_in senderAddress{};
    socklen_t senderLength = sizeof(senderAddress);
    Require(getsockname(sender, reinterpret_cast<sockaddr*>(&senderAddress), &senderLength) == 0
        && senderAddress.sin_addr.s_addr == htonl(INADDR_LOOPBACK), "query owned loopback sender");
    constexpr std::array<byte, 4> kSmallPacket{0, 0x7f, 0x80, 0xff};
    Require(NET_SendToImpl(NS_CLIENT, sender, const_cast<char*>(reinterpret_cast<const char*>(kSmallPacket.data())),
        static_cast<int>(kSmallPacket.size()), 0, reinterpret_cast<sockaddr*>(&destination), sizeof(destination))
        == static_cast<int>(kSmallPacket.size()), "actual native send boundary");
    Require(Receive(receiver, senderAddress.sin_port, deadline) == std::vector<byte>(kSmallPacket.begin(), kSmallPacket.end()), "exact small datagram");
    std::vector<byte> payload(kPayloadBytes);
    for (std::size_t index = 0; index < payload.size(); ++index) {
        payload[index] = static_cast<byte>(index % 251);
    }
    Require(NET_SendLong(NS_SERVER, sender, reinterpret_cast<const char*>(payload.data()),
        static_cast<int>(payload.size()), 0, reinterpret_cast<sockaddr*>(&destination), sizeof(destination))
        == static_cast<int>(payload.size()), "actual split send");
    std::vector<byte> first{0xfe, 0xff, 0xff, 0xff, 2, 0, 0, 0, 0x02};
    std::vector<byte> second{0xfe, 0xff, 0xff, 0xff, 2, 0, 0, 0, 0x12};
    first.insert(first.end(), payload.begin(), payload.begin() + SPLIT_PAYLOAD_BYTES);
    second.insert(second.end(), payload.begin() + SPLIT_PAYLOAD_BYTES, payload.end());
    Require(Receive(receiver, senderAddress.sin_port, deadline) == first && Receive(receiver, senderAddress.sin_port, deadline) == second, "actual split bytes/order on loopback");
    sockaddr_in from{};
    int fromLength = sizeof(from);
    char empty = 0;
    Require(NET_PosixReceiveFrom(receiver, &empty, sizeof(empty), 0, reinterpret_cast<sockaddr*>(&from), &fromLength)
        == -1 && (errno == EAGAIN || errno == EWOULDBLOCK), "nonblocking empty receive");
    ip_sockets[0] = 0;
    Require(FixtureClose(sender) == 0 && FixtureClose(receiver) == 0, "close both sole owned references");
    Require(api.closed == std::vector<SOCKET>({sender, receiver}), "one close per owned reference");
    Require(fakeSendCalls == 0, "no FakeIP provider invoked");
    Require(Clock::now() < deadline, "complete within total five-second deadline");
    std::puts("real_ipv4_loopback_passed=1 datagrams=3 payload_bytes=1405 split_record_bytes=9");
}
}

int main(int argc, const char** argv) {
    Require(argc == 2, "explicit --describe or --loopback required");
    if (std::strcmp(argv[1], "--describe") == 0) {
        std::puts("build_only=1 execution_requires_explicit_authorization=1 bind=127.0.0.1:ephemeral sockets=2 "
            "datagrams=3 max_datagram_bytes=1400 no_dns=1 no_provider=1 no_game=1 close_each_owned_reference_once=1 max_waits_per_packet=50 finite_select_us=20000 "
            "receive_capacity=1401 reject_received_bytes_above=1400 sender_port_verified=1 total_deadline_ms=5000");
        return 0;
    }
    Require(std::strcmp(argv[1], "--loopback") == 0, "unknown execution mode");
    Run();
    return 0;
}
