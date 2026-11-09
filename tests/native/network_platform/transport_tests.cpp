#include "transport_context.h"

namespace {
constexpr SOCKET kFirstSocket = 3;
constexpr SOCKET kSecondSocket = 7;

void ReadSet() {
#if !defined(_WIN32)
    struct GuardedSet {
        std::array<byte, 16> before;
        fd_set descriptors;
        std::array<byte, 16> after;
    } guarded;
    guarded.before.fill(0xa5);
    guarded.after.fill(0x5a);
    SOCKET sockets[NET_SOCKET_SOURCES]{kFirstSocket, FD_SETSIZE - 1, kFirstSocket};
    int count = 0;
    Require(NET_PosixBuildReadSet(sockets, guarded.descriptors, count), "build actual POSIX bitmap");
    Require(count == FD_SETSIZE && FD_ISSET(kFirstSocket, &guarded.descriptors)
        && FD_ISSET(FD_SETSIZE - 1, &guarded.descriptors) && !FD_ISSET(0, &guarded.descriptors),
        "max descriptor plus one and closed zero sentinel");
    sockets[1] = FD_SETSIZE;
    Require(!NET_PosixBuildReadSet(sockets, guarded.descriptors, count) && errno == EINVAL && count == 0,
        "high descriptor fails before FD_SET");
    sockets[1] = -1;
    Require(!NET_PosixBuildReadSet(sockets, guarded.descriptors, count) && errno == EBADF,
        "negative descriptor rejected");
    for (const byte value : guarded.before) {
        Require(value == 0xa5, "bitmap prefix guard");
    }
    for (const byte value : guarded.after) {
        Require(value == 0x5a, "bitmap suffix guard");
    }
#else
    // Windows SOCKET is a handle, not a bitmap index. Keep every high bit in the existing fd_array.
    if constexpr (sizeof(SOCKET) == sizeof(std::uint64_t)) {
        ip_sockets[0] = static_cast<SOCKET>((std::uint64_t{1} << 40) + kFirstSocket);
        ip_sockets[1] = static_cast<SOCKET>((std::uint64_t{1} << 41) + kSecondSocket);
    } else {
        ip_sockets[0] = kFirstSocket;
        ip_sockets[1] = kSecondSocket;
    }
    Require(NET_Sleep() == 0, "controlled Windows readiness result");
    const auto& set = selectCalls.back().read;
    Require(set.fd_count == 2 && set.fd_array[0] == ip_sockets[0] && set.fd_array[1] == ip_sockets[1],
        "Windows source order and full-width handles retained");
#endif
    std::printf("read_set_passed=1 socket_bytes=%zu fd_set_bytes=%zu capacity=%d\n",
        sizeof(SOCKET), sizeof(fd_set), FD_SETSIZE);
}

void SleepArguments() {
    ip_sockets[0] = kFirstSocket;
    ip_sockets[1] = kSecondSocket;
    controlledSelectResult = 2;
    Require(NET_Sleep() == 2, "positive select result unchanged");
    auto call = selectCalls.back();
    Require(call.hasRead && call.hasTimeout && call.seconds == 0 && call.microseconds == 20000,
        "existing finite sleep timeout");
    Require(call.descriptorCount == kSecondSocket + 1, "nfds uses max descriptor plus one");
    net_sleepforever = 1;
    Require(NET_Sleep() == 2 && !selectCalls.back().hasTimeout, "infinite wait passes null timeout");
    net_sleepforever = 0;
    controlledSelectResult = -1;
    Require(NET_Sleep() == -1 && errno == EINTR, "select error and errno passed through");
    controlledSelectResult = 0;
    ip_sockets[0] = 0;
    ip_sockets[1] = 0;
    Require(NET_Sleep() == 0, "timeout result unchanged for empty set");
#if !defined(_WIN32)
    Require(selectCalls.back().descriptorCount == 0, "empty POSIX set uses nfds zero");
    ip_sockets[0] = FD_SETSIZE;
    const auto before = selectCalls.size();
    Require(NET_Sleep() == -1 && errno == EINVAL && selectCalls.size() == before,
        "invalid bitmap descriptor never reaches select");
#endif
}

void Stagger() {
    ip_sockets[0] = kSecondSocket;
    fixtureClock = 1;
    controlledSelectResult = 4;
    Require(NET_Sleep_Timeout() == 4, "initial stagger result");
    Require(!selectCalls.back().hasRead && selectCalls.back().descriptorCount == 0, "initial sleep-only branch");
    fixtureClock = 3;
    NET_Sleep_Timeout();
    Require(!selectCalls.back().hasRead, "whole stagger interval sleeps");
    NET_Sleep_Timeout();
    Require(selectCalls.back().hasRead && selectCalls.back().descriptorCount == kSecondSocket + 1,
        "next stagger interval waits for readiness");
    NET_Sleep_Timeout();
    Require(!selectCalls.back().hasRead, "stagger state decrements in the original order");
    for (const auto& call : selectCalls) {
        Require(call.hasTimeout && call.seconds == 0 && call.microseconds == 10000,
            "sys_ticrate timeout arithmetic preserved");
    }
}

void Lengths() {
    sockaddr_in address{};
    address.sin_family = AF_INET;
    char bytes[16]{};
#if !defined(_WIN32)
    constexpr ssize_t kMaximumEngineCount = std::numeric_limits<int>::max();
    Require(NET_PosixSocketCount(kMaximumEngineCount) == std::numeric_limits<int>::max(), "maximum count retained");
    errno = EAGAIN;
    Require(NET_PosixSocketCount(-1) == -1 && errno == EAGAIN, "native error count retains errno");
    Require(NET_PosixSocketCount(kMaximumEngineCount + 1) == -1 && errno == EOVERFLOW,
        "large native count is not truncated");
    Require(NET_PosixSocketCount(-2) == -1 && errno == EOVERFLOW, "non-API negative count rejected");
    socklen_t nativeLength = 5;
    Require(!NET_PosixAddressLength(-1, nativeLength) && nativeLength == 5 && errno == EINVAL,
        "negative address length never reinterpreted unsigned");
    Require(NET_PosixAddressLength(0, nativeLength) && nativeLength == 0, "zero address length retained");
    Require(NET_PosixAddressLength(std::numeric_limits<int>::max(), nativeLength), "native address width can hold int");
    io = {};
    Require(NET_PosixSendTo(17, bytes, -1, 0, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == -1
        && errno == EINVAL && io.sendCalls == 0, "negative payload length rejected before API");
    Require(NET_PosixSendTo(17, bytes, 1, 0, reinterpret_cast<sockaddr*>(&address), -1) == -1
        && errno == EINVAL && io.sendCalls == 0, "negative destination length rejected before API");
    io.captureSend = false;
    io.forceSend = true;
    io.sendResult = kMaximumEngineCount;
    Require(NET_PosixSendTo(17, bytes, std::numeric_limits<int>::max(), 0,
        reinterpret_cast<sockaddr*>(&address), sizeof(address)) == std::numeric_limits<int>::max(),
        "typed no-read API control sees the full maximum requested length");
    Require(io.requestedLength == static_cast<std::size_t>(std::numeric_limits<int>::max()), "requested size preserved");
    io.sendResult = kMaximumEngineCount + 1;
    Require(NET_PosixSendTo(17, bytes, 1, 0, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == -1
        && errno == EOVERFLOW, "unrepresentable native send result rejected");
    io = {};
    int fromLength = sizeof(address);
    const auto excessiveCapacity = static_cast<std::size_t>(std::numeric_limits<int>::max()) + 1;
    Require(NET_PosixReceiveFrom(17, bytes, excessiveCapacity, 0, reinterpret_cast<sockaddr*>(&address), &fromLength)
        == -1 && errno == EMSGSIZE && io.receiveCalls == 0, "unrepresentable receive capacity rejected");
    fromLength = -1;
    Require(NET_PosixReceiveFrom(17, bytes, sizeof(bytes), 0, reinterpret_cast<sockaddr*>(&address), &fromLength)
        == -1 && errno == EINVAL && io.receiveCalls == 0, "negative receive address capacity rejected");
    fromLength = sizeof(address);
    Require(NET_PosixReceiveFrom(17, bytes, sizeof(bytes), 0, reinterpret_cast<sockaddr*>(&address), &fromLength)
        == -1 && errno == EAGAIN && fromLength == sizeof(address), "receive error and caller length preserved");
    io.receiveResult = 5;
    io.receiveData = {1, 2, 3, 4, 5};
    Require(NET_PosixReceiveFrom(17, bytes, sizeof(bytes), 0, reinterpret_cast<sockaddr*>(&address), &fromLength)
        == 5 && fromLength == sizeof(address) && std::memcmp(bytes, io.receiveData.data(), 5) == 0,
        "native socklen storage copied back through a checked int boundary");
    io.receiveAddressLength = static_cast<socklen_t>(std::numeric_limits<int>::max()) + 1u;
    fromLength = sizeof(address);
    Require(NET_PosixReceiveFrom(17, bytes, sizeof(bytes), 0, reinterpret_cast<sockaddr*>(&address), &fromLength)
        == -1 && errno == EOVERFLOW && fromLength == sizeof(address), "large output address length rejected");
    io = {};
    io.receiveResult = 0;
    Require(NET_PosixReceiveFrom(17, nullptr, 0, 0, nullptr, nullptr) == 0, "zero datagram count retained");
#else
    io = {};
    io.captureSend = false;
    Require(NET_SendToImpl(NS_CLIENT, 17, bytes, std::numeric_limits<int>::max(), 0,
        reinterpret_cast<sockaddr*>(&address), sizeof(address)) == std::numeric_limits<int>::max(),
        "unchanged Windows API sees the full int request");
    Require(io.requestedLength == std::numeric_limits<int>::max(), "Windows request width retained");
#endif
}

void Wire() {
    static_assert(sizeof(SPLITPACKET) == 9 && sizeof(int) == 4, "fixed split record ABI");
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(27015);
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    std::vector<byte> payload(1401);
    for (std::size_t index = 0; index < payload.size(); ++index) {
        payload[index] = static_cast<byte>(index % 251);
    }
    io = {};
    Require(NET_SendLong(NS_SERVER, 17, reinterpret_cast<const char*>(payload.data()), MAX_ROUTABLE_PACKET, 0,
        reinterpret_cast<sockaddr*>(&address), sizeof(address)) == MAX_ROUTABLE_PACKET && io.sent.size() == 1
        && io.sent[0] == std::vector<byte>(payload.begin(), payload.begin() + MAX_ROUTABLE_PACKET),
        "server datagram at the exact threshold remains unsplit");
    io = {};
    Require(NET_SendLong(NS_CLIENT, 17, reinterpret_cast<const char*>(payload.data()), 1401, 0,
        reinterpret_cast<sockaddr*>(&address), sizeof(address)) == 1401 && io.sent.size() == 1
        && io.sent[0] == payload, "client datagram remains unsplit");
    io = {};
    Require(NET_SendLong(NS_SERVER, 17, reinterpret_cast<const char*>(payload.data()), 1401, 0,
        reinterpret_cast<sockaddr*>(&address), sizeof(address)) == 1401 && io.sent.size() == 2,
        "server threshold produces two packets");
    std::vector<byte> first{0xfe, 0xff, 0xff, 0xff, 2, 0, 0, 0, 0x02};
    std::vector<byte> second{0xfe, 0xff, 0xff, 0xff, 2, 0, 0, 0, 0x12};
    first.insert(first.end(), payload.begin(), payload.begin() + 1391);
    second.insert(second.end(), payload.begin() + 1391, payload.end());
    Require(io.sent[0] == first && io.sent[1] == second, "exact split bytes, sequence, order and packet numbers");
    for (std::size_t index = 0; index < io.sent.size(); ++index) {
        const std::string filename = "wire-" + std::to_string(index) + ".bin";
        auto* file = std::fopen(filename.c_str(), "wb");
        Require(file != nullptr, "owned wire capture file");
        Require(std::fwrite(io.sent[index].data(), 1, io.sent[index].size(), file) == io.sent[index].size(), "wire capture");
        std::fclose(file);
    }
    io = {};
    io.forceSend = true;
    io.sendResult = -1;
    Require(NET_SendLong(NS_SERVER, 17, reinterpret_cast<const char*>(payload.data()), 1401, 0,
        reinterpret_cast<sockaddr*>(&address), sizeof(address)) == -1 && io.sendCalls == 1,
        "split send stops immediately on the first native failure");
    io = {};
    io.forceSend = true;
    io.sendResult = 0;
    Require(NET_SendLong(NS_SERVER, 17, reinterpret_cast<const char*>(payload.data()), 1401, 0,
        reinterpret_cast<sockaddr*>(&address), sizeof(address)) == 0 && io.sendCalls == 2,
        "inherited short-send counting and packet order retained");
    io = {};
    fakeRoute = true;
    Require(NET_SendToImpl(NS_CLIENT, 17, reinterpret_cast<char*>(payload.data()), 1401, 0,
        reinterpret_cast<sockaddr*>(&address), sizeof(address)) == 1401 && fakeSendCalls == 1 && io.sendCalls == 0,
        "FakeIP delegation still precedes native send");
    fakeRoute = false;
}

void PipeReadiness() {
#if !defined(_WIN32)
    int descriptors[2]{};
    Require(::pipe(descriptors) == 0, "owned local pipe");
    Require(descriptors[0] > 0 && descriptors[0] < FD_SETSIZE, "pipe fits native select capability");
    constexpr byte kReadyByte = 0x5a;
    Require(::write(descriptors[1], &kReadyByte, sizeof(kReadyByte)) == sizeof(kReadyByte), "pipe readiness byte");
    ip_sockets[0] = descriptors[0];
    realDescriptorSelect = true;
    Require(NET_Sleep() == 1, "actual POSIX select readiness count");
    byte value = 0;
    Require(::read(descriptors[0], &value, sizeof(value)) == sizeof(value) && value == kReadyByte, "consume pipe byte");
    Require(NET_Sleep() == 0, "actual finite timeout");
    Require(::close(descriptors[0]) == 0, "release owned read descriptor once");
    Require(NET_Sleep() == -1 && errno == EBADF, "actual closed-descriptor select error");
    ip_sockets[0] = 0;
    Require(::close(descriptors[1]) == 0, "release owned write descriptor once");
#else
    Require(false, "real POSIX pipe case is not a Windows socket fixture");
#endif
}
}

int main(int argc, const char** argv) {
    Require(argc == 2, "one pure scenario required");
    if (std::strcmp(argv[1], "read-set") == 0) {
        ReadSet();
    } else if (std::strcmp(argv[1], "sleep") == 0) {
        SleepArguments();
    } else if (std::strcmp(argv[1], "stagger") == 0) {
        Stagger();
    } else if (std::strcmp(argv[1], "lengths") == 0) {
        Lengths();
    } else if (std::strcmp(argv[1], "wire") == 0) {
        Wire();
    } else if (std::strcmp(argv[1], "pipe") == 0) {
        PipeReadiness();
    } else {
        Require(false, "unknown scenario");
    }
    std::puts("actual_transport_control_passed=1");
    return 0;
}
