#pragma once
#include "fixture_context.h"
#include <mutex>
#include <stdexcept>

inline SOCKET ip_sockets[NET_SOCKET_SOURCES]{};
inline int net_sleepforever = 0;
inline int use_thread = 0;
inline qboolean net_thread_initialized = 0;
inline std::recursive_mutex net_thread_mutex;
inline loopback_t loopbacks[LOOPBACK_CHANNELS]{};
// Only fields consumed by the exact extracted owners; no client ABI/layout claim.
constexpr std::size_t kFixtureServerNameBytes = 260;
struct FixtureClient {
    cactive_t state = ca_connecting;
    char servername[kFixtureServerNameBytes]{};
    int authprotocol = 0;
    qboolean passive = 0;
    int challenge = 0;
    double connect_time = 0;
    int connect_retry = 0;
    struct {
        netsrc_t sock = NS_CLIENT;
    } netchan;
    netadr_t connect_stream{};
};
inline FixtureClient cls;

[[noreturn]] inline void Sys_Error(const char* format, ...) {
    va_list arguments;
    va_start(arguments, format);
    constexpr std::size_t kFatalDiagnosticBytes = 512;
    char text[kFatalDiagnosticBytes]{};
    std::vsnprintf(text, sizeof(text), format, arguments);
    va_end(arguments);
    throw std::runtime_error(text);
}
struct FixtureCvar {
    float value = 0;
};
inline FixtureCvar sys_ticrate{100};
inline FixtureCvar net_showpackets{};
inline double fixtureClock = 0;
inline double Sys_FloatTime() {
    return fixtureClock;
}
inline bool fakeRoute = false;
inline int fakeSendCalls = 0;

inline qboolean NET_IsSteamFakeIP(netsrc_t, const sockaddr*) {
    return fakeRoute;
}
inline int NET_SteamFakeIPSendTo(netsrc_t, SOCKET, char*, int length, int, const sockaddr*, int) {
    ++fakeSendCalls;
    return length;
}

struct SelectCall {
    int descriptorCount = 0;
    bool hasRead = false;
    fd_set read{};
    bool hasTimeout = false;
    long long seconds = 0;
    long long microseconds = 0;
};
inline std::vector<SelectCall> selectCalls;
inline bool realDescriptorSelect = false;
inline int controlledSelectResult = 0;

#if defined(_WIN32)
inline int FixtureSelect(int count, fd_set* read, fd_set* write, fd_set* error, const timeval* timeout) {
#else
inline int FixtureSelect(int count, fd_set* read, fd_set* write, fd_set* error, timeval* timeout) {
#endif
    Require(!write && !error, "read-only select contract");
    SelectCall call;
    call.descriptorCount = count;
    call.hasRead = read != nullptr;
    if (read) {
        call.read = *read;
    }
    call.hasTimeout = timeout != nullptr;
    if (timeout) {
        call.seconds = timeout->tv_sec;
        call.microseconds = timeout->tv_usec;
    }
    selectCalls.push_back(call);
    if (realDescriptorSelect || liveLoopback) {
        return ::select(count, read, write, error, timeout);
    }
    if (controlledSelectResult < 0) {
        errno = EINTR;
    }
    return controlledSelectResult;
}

#if !defined(_WIN32)
#define _snprintf snprintf
#endif
#define sendto FixtureSendTo
#define select FixtureSelect
#if defined(_WIN32)
#define WSAGetLastError() fixtureSocketError
#endif
// Select only the production owner branch after loading the real host SDK headers.
#if defined(CSE_NETWORK_NON_APPLE_CONTROL)
#pragma push_macro("__APPLE__")
#undef __APPLE__
#endif
#include "actual_transport.h"
#if defined(CSE_NETWORK_NON_APPLE_CONTROL)
#pragma pop_macro("__APPLE__")
#endif
#if defined(_WIN32)
#undef WSAGetLastError
#endif
#undef select
#undef sendto
#if !defined(_WIN32)
#undef _snprintf
#endif
