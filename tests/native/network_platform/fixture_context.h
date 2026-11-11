#pragma once
#if defined(CSE_TEST_SOCKET_OWNER)
#include "native_socket_lifetime.h"
#include "SocketImageOwner.h"
#include <dlfcn.h>
#endif
#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <Windows.h>
#else
#include <arpa/inet.h>
#include <cerrno>
#include <fcntl.h>
#include <netdb.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>
using SOCKET = int;
#endif
#include <algorithm>
#include <cerrno>
#include <array>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <string>
#include <vector>

using byte = unsigned char;
using qboolean = int;
enum netsrc_t { NS_CLIENT, NS_SERVER, NS_MULTICAST };
#include "actual_types.h"
#include "netadr.h"

inline void Require(bool condition, const char* description) {
    if (!condition) {
        std::fprintf(stderr, "FAILED: %s\n", description);
        std::abort();
    }
}

enum class Failure { None, Socket, Duplicate, Ioctl, Broadcast, Reuse, Tos, Bind, Loop };
struct ApiState {
    Failure failure = Failure::None;
    SOCKET created = 17;
    int duplicate = 18;
    bool reuse = false;
    bool tos = false;
    int injectedError = EIO;
    sockaddr_in boundAddress{};
    int loopOption = -1;
    int openCalls = 0;
    int duplicateCalls = 0;
    int ioctlCalls = 0;
    int bindCalls = 0;
    int options = 0;
    std::vector<SOCKET> closed;
    std::vector<std::string> diagnostics;
    int debugDiagnosticCalls = 0;
};
inline ApiState api;
inline bool liveLoopback = false;
constexpr int kInjectedError = EIO;
constexpr int kDiagnosticError = ERANGE;
#if defined(_WIN32)
constexpr int kInjectedSocketError = WSAENOBUFS;
inline int fixtureSocketError = kInjectedSocketError;
#endif

#if defined(CSE_TEST_SOCKET_OWNER)
void InitializeSocketFixture();
#endif
inline void ResetApi() {
#if defined(CSE_TEST_SOCKET_OWNER)
    InitializeSocketFixture();
    if (!liveLoopback) {
        cse::socket_lifetime::CloseUdp(api.created);
        cse::socket_lifetime::CloseUdp(api.duplicate);
    }
#endif
    api = {};
    errno = 0;
#if defined(_WIN32)
    fixtureSocketError = kInjectedSocketError;
#endif
}

inline int FailApi() {
    errno = api.injectedError;
#if defined(_WIN32)
    fixtureSocketError = kInjectedSocketError;
#endif
    return -1;
}

inline SOCKET FixtureSocket(int family, int type, int protocol) {
    Require(family == AF_INET && type == SOCK_DGRAM && protocol == IPPROTO_UDP, "original IPv4 UDP open contract");
    ++api.openCalls;
    if (liveLoopback) {
        return ::socket(family, type, protocol);
    }
    return api.failure == Failure::Socket ? static_cast<SOCKET>(FailApi()) : api.created;
}

inline int FixtureClose(SOCKET socket) {
    api.closed.push_back(socket);
    if (liveLoopback) {
#if defined(_WIN32)
        return ::closesocket(socket);
#else
        return ::close(socket);
#endif
    }
    errno = kDiagnosticError;
    return 0;
}

#if defined(_WIN32)
inline int FixtureIoctl(SOCKET socket, long command, u_long* value) {
#else
inline int FixtureIoctl(SOCKET socket, unsigned long command, int* value) {
#endif
    Require(command == FIONBIO && *value == 1, "nonblocking ioctl arguments");
    ++api.ioctlCalls;
    if (liveLoopback) {
#if defined(_WIN32)
        return ::ioctlsocket(socket, command, value);
#else
        return ::ioctl(socket, command, value);
#endif
    }
    return api.failure == Failure::Ioctl ? FailApi() : 0;
}

#if defined(_WIN32)
inline int FixtureOption(SOCKET socket, int level, int option, const char* value, int length) {
#else
inline int FixtureOption(SOCKET socket, int level, int option, const void* value, socklen_t length) {
#endif
    ++api.options;
    if (liveLoopback) {
        return ::setsockopt(socket, level, option, value, length);
    }
    Require(length == static_cast<decltype(length)>(sizeof(int)), "original socket option integer width");
    int integer = 0;
    std::memcpy(&integer, value, sizeof(integer));
    if (level == SOL_SOCKET && (option == SO_BROADCAST || option == SO_REUSEADDR)) {
        Require(integer == 1, "broadcast/reuse option remains enabled");
    }
#if !defined(_WIN32)
    if (level == IPPROTO_IP && option == IP_TOS) {
        Require(integer == 16, "original low-delay TOS value");
    }
#endif
    if (level == IPPROTO_IP && option ==
#if defined(_WIN32)
        kEngineMulticastLoop
#else
        IP_MULTICAST_LOOP
#endif
    ) {
        api.loopOption = integer;
    }
    if ((level == SOL_SOCKET && option == SO_BROADCAST && api.failure == Failure::Broadcast)
        || (level == SOL_SOCKET && option == SO_REUSEADDR && api.failure == Failure::Reuse)
#if !defined(_WIN32)
        || (level == IPPROTO_IP && option == IP_TOS && api.failure == Failure::Tos)
#endif
        || (level == IPPROTO_IP &&
#if defined(_WIN32)
            option == kEngineMulticastLoop
#else
            option == IP_MULTICAST_LOOP
#endif
            && api.failure == Failure::Loop)) {
        return FailApi();
    }
    return 0;
}

#if defined(_WIN32)
inline int FixtureBind(SOCKET socket, const sockaddr* address, int length) {
#else
inline int FixtureBind(SOCKET socket, const sockaddr* address, socklen_t length) {
#endif
    Require(length == sizeof(sockaddr_in) && address->sa_family == AF_INET, "IPv4 bind length and family");
    ++api.bindCalls;
    std::memcpy(&api.boundAddress, address, sizeof(api.boundAddress));
    if (liveLoopback) {
        const auto* ipv4 = reinterpret_cast<const sockaddr_in*>(address);
        Require(ipv4->sin_addr.s_addr == htonl(INADDR_LOOPBACK), "live fixture binds only IPv4 loopback");
        return ::bind(socket, address, length);
    }
    return api.failure == Failure::Bind ? FailApi() : 0;
}

#if !defined(_WIN32)
inline int FixtureDuplicate(SOCKET socket, int command, int minimum) {
    Require(socket == 0 && command == F_DUPFD && minimum == 1, "duplicate only the owned zero descriptor");
    ++api.duplicateCalls;
    if (liveLoopback) {
        return ::fcntl(socket, command, minimum);
    }
    return api.failure == Failure::Duplicate ? FailApi() : api.duplicate;
}
#endif

#if defined(_WIN32)
using NativeCount = int;
using NativeLength = int;
using NativeAddressLength = int;
#else
using NativeCount = ssize_t;
using NativeLength = std::size_t;
using NativeAddressLength = socklen_t;
#endif

struct IoState {
    int sendCalls = 0;
    int receiveCalls = 0;
    bool forceSend = false;
    bool captureSend = true;
    NativeCount sendResult = 0;
#if defined(_WIN32)
    int sendError = WSAEWOULDBLOCK;
#else
    int sendError = EAGAIN;
#endif
    SOCKET sentSocket = 0;
    sockaddr_in sentAddress{};
    NativeCount receiveResult = -1;
    NativeAddressLength receiveAddressLength = sizeof(sockaddr_in);
    NativeLength requestedLength = 0;
    NativeAddressLength addressLength = 0;
    std::vector<std::vector<byte>> sent;
    std::vector<byte> receiveData;
};
inline IoState io;

#if defined(_WIN32)
inline int FixtureSendTo(SOCKET socket, const char* data, int length, int flags,
    const sockaddr* destination, int addressLength) {
#else
inline ssize_t FixtureSendTo(SOCKET socket, const void* data, std::size_t length, int flags,
    const sockaddr* destination, socklen_t addressLength) {
#endif
    ++io.sendCalls;
    io.requestedLength = length;
    io.addressLength = addressLength;
    io.sentSocket = socket;
    if (destination && addressLength == sizeof(sockaddr_in)) {
        std::memcpy(&io.sentAddress, destination, sizeof(io.sentAddress));
    }
    if (liveLoopback) {
        Require(destination && destination->sa_family == AF_INET
            && reinterpret_cast<const sockaddr_in*>(destination)->sin_addr.s_addr == htonl(INADDR_LOOPBACK),
            "live fixture sends only IPv4 loopback");
        return ::sendto(socket,
#if defined(_WIN32)
            data,
#else
            static_cast<const char*>(data),
#endif
            length, flags, destination, addressLength);
    }
    if (io.captureSend && length > 0) {
        constexpr NativeLength kMaximumCapturedBytes = 65536;
        Require(length <= kMaximumCapturedBytes, "controlled payload capture is bounded");
        const auto* bytes = reinterpret_cast<const byte*>(data);
        io.sent.emplace_back(bytes, bytes + length);
    }
    if (io.forceSend) {
        if (io.sendResult < 0) {
            errno = io.sendError;
#if defined(_WIN32)
            fixtureSocketError = io.sendError;
#endif
        }
        return io.sendResult;
    }
    return static_cast<NativeCount>(length);
}

#if defined(_WIN32)
inline int FixtureReceiveFrom(SOCKET socket, char* data, int capacity, int flags,
    sockaddr* source, int* addressLength) {
#else
inline ssize_t FixtureReceiveFrom(SOCKET socket, void* data, std::size_t capacity, int flags,
    sockaddr* source, socklen_t* addressLength) {
#endif
    ++io.receiveCalls;
    io.requestedLength = capacity;
    if (liveLoopback) {
        return ::recvfrom(socket,
#if defined(_WIN32)
            data,
#else
            static_cast<char*>(data),
#endif
            capacity, flags, source, addressLength);
    }
    if (io.receiveResult < 0) {
        errno = EAGAIN;
        return io.receiveResult;
    }
    const auto count = std::min({static_cast<std::size_t>(capacity), io.receiveData.size(),
        static_cast<std::size_t>(io.receiveResult)});
    if (count != 0) {
        std::memcpy(data, io.receiveData.data(), count);
    }
    if (source && addressLength && *addressLength >= static_cast<NativeAddressLength>(sizeof(sockaddr_in))) {
        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        std::memcpy(source, &address, sizeof(address));
    }
    if (addressLength) {
        *addressLength = io.receiveAddressLength;
    }
    return io.receiveResult;
}

inline int COM_CheckParm(const char* name) {
    return (std::strcmp(name, "-reuse") == 0 && api.reuse) || (std::strcmp(name, "-tos") == 0 && api.tos);
}

inline void RecordDiagnostic(const char* format, va_list arguments) {
    char text[512]{};
    std::vsnprintf(text, sizeof(text), format, arguments);
    api.diagnostics.emplace_back(text);
    errno = kDiagnosticError;
}

inline void Con_Printf(const char* format, ...) {
    va_list arguments;
    va_start(arguments, format);
    RecordDiagnostic(format, arguments);
    va_end(arguments);
}

inline void Con_DPrintf(const char* format, ...) {
    ++api.debugDiagnosticCalls;
    va_list arguments;
    va_start(arguments, format);
    RecordDiagnostic(format, arguments);
    va_end(arguments);
}

inline void Q_memcpy(void* to, const void* from, std::size_t length) {
    std::memcpy(to, from, length);
}
inline void Q_memset(void* data, int value, std::size_t length) {
    std::memset(data, value, length);
}
inline void Q_strncpy(char* to, const char* from, std::size_t length) {
    std::strncpy(to, from, length);
}
inline int Q_strcmp(const char* left, const char* right) {
    return std::strcmp(left, right);
}
inline int Q_strcasecmp(const char* left, const char* right) {
#if defined(_WIN32)
    return _stricmp(left, right);
#else
    return strcasecmp(left, right);
#endif
}

inline hostent* FixtureControlledHostname(const char* name) {
    // A synthetic host record keeps both production parser outcomes reachable without any DNS API.
    Require(!liveLoopback && std::strcmp(name, "fixture.invalid") == 0, "fixture forbids external DNS resolution");
    static char addressBytes[4]{127, 0, 0, 1};
    static char* addresses[2]{addressBytes, nullptr};
    static hostent host{};
    host.h_addr_list = addresses;
    return &host;
}

#define gethostbyname FixtureControlledHostname
#define socket FixtureSocket
#define ioctl FixtureIoctl
#define ioctlsocket FixtureIoctl
#define setsockopt FixtureOption
#define bind FixtureBind
#define close FixtureClose
#define closesocket FixtureClose
#if !defined(_WIN32)
#define fcntl FixtureDuplicate
#endif
#define sendto FixtureSendTo
#define recvfrom FixtureReceiveFrom
#if defined(_WIN32)
#define WSAGetLastError() fixtureSocketError
#endif
#include "actual_posix_helpers.h"
#include "actual_open.h"
#undef gethostbyname
#undef socket
#undef ioctl
#undef ioctlsocket
#undef setsockopt
#undef bind
#undef close
#undef closesocket
#undef fcntl
#undef WSAGetLastError
#undef sendto
#undef recvfrom

#if defined(CSE_TEST_SOCKET_OWNER)
int CseTestSocket(int family, int type, int protocol) {
    return FixtureSocket(family, type, protocol);
}
int CseTestClose(int descriptor) {
    return FixtureClose(descriptor);
}
int CseTestFcntl(int descriptor, int command, ...) {
    va_list args;
    va_start(args, command);
    const int value = va_arg(args, int);
    va_end(args);
    if (command == F_SETFD) {
        Require(value == FD_CLOEXEC, "additional exec protection on actual host-created descriptor");
        return liveLoopback ? ::fcntl(descriptor, command, value) : 0;
    }
    return FixtureDuplicate(descriptor, command, value);
}
int CseTestDlclose(void* image) {
    return ::dlclose(image);
}
int CseTestAtfork(void (*prepare)(), void (*parent)(), void (*child)()) {
    return ::pthread_atfork(prepare, parent, child);
}
inline void InitializeSocketFixture() {
    static bool initialized = false;
    if (initialized) {
        return;
    }
    Require(cse::socket_lifetime::InitializeProcess(), "actual executable process installer");
    Dl_info physical{};
    Require(dladdr(reinterpret_cast<const void*>(&cse::socket_lifetime::CreateUdp), &physical) != 0,
            "actual linked socket producer provenance");
    void* image = dlopen(physical.dli_fname, RTLD_NOW | RTLD_LOCAL | RTLD_NOLOAD);
    Require(image != nullptr, "linked fixture producer reference");
    struct Lifetime {
        cse::socket_lifetime::SocketImageOwner owner;
        ~Lifetime() {
            cse::socket_lifetime::CloseUdp(api.created);
            cse::socket_lifetime::CloseUdp(api.duplicate);
            Require(owner.Retire(), "linked transport fixture actual terminal cleanup");
        }
    };
    // Linked component evidence, not a cold-load or physical-unmap claim.
    static Lifetime lifetime;
    Require(lifetime.owner.Open(image, physical.dli_fname), "actual executable owner binds actual producer");
    Require(dlclose(image) == 0, "balance temporary lookup reference");
    initialized = true;
}
#endif
