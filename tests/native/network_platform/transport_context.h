#pragma once
#include "fixture_context.h"

inline SOCKET ip_sockets[NET_SOCKET_SOURCES]{};
inline int net_sleepforever = 0;
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
#include "actual_transport.h"
#undef select
#undef sendto
#if !defined(_WIN32)
#undef _snprintf
#endif
