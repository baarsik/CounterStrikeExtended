#pragma once

#include <cstddef>
#include <cstdio>
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
using TestSocket = SOCKET;
using TestTransfer = int;
using TestLength = int;
using TestData = char;
#else
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
using TestSocket = int;
using TestTransfer = ssize_t;
using TestLength = socklen_t;
using TestData = void;
#endif

TestSocket CseSocket(int family, int type, int protocol);
TestTransfer CseRecvFrom(TestSocket socket, TestData* data,
#ifdef _WIN32
    int length,
#else
    std::size_t length,
#endif
    int flags, sockaddr* address, TestLength* addressLength);
TestTransfer CseSendTo(TestSocket socket, const TestData* data,
#ifdef _WIN32
    int length,
#else
    std::size_t length,
#endif
    int flags, const sockaddr* address, TestLength addressLength);
int CseSetSocketOption(TestSocket socket, int level, int option,
#ifdef _WIN32
    const char* value,
#else
    const void* value,
#endif
    TestLength length);
int CseBind(TestSocket socket, const sockaddr* address, TestLength length);
int CseShutdown(TestSocket socket, int how);
int CseCloseSocket(TestSocket socket);
void CsePerror(const char* label);
#ifdef _WIN32
int CseIoctlSocket(SOCKET socket, long command, u_long* value);
int CseGetHostname(char* name, int length);
int CseStartup(WORD version, LPWSADATA data);
int CseCleanup();
#else
int CseIoctlSocket(int socket, unsigned long command, ...);
int CseGetHostname(char* name, std::size_t length);
#endif
hostent* CseGetHostByName(const char* name);

#define socket CseSocket
#define recvfrom CseRecvFrom
#define sendto CseSendTo
#define setsockopt CseSetSocketOption
#define bind CseBind
#define shutdown CseShutdown
#define perror CsePerror
#define gethostname CseGetHostname
#define gethostbyname CseGetHostByName
#ifdef _WIN32
#define ioctlsocket CseIoctlSocket
#define closesocket CseCloseSocket
#define WSAStartup CseStartup
#define WSACleanup CseCleanup
#else
#define ioctl CseIoctlSocket
#define close CseCloseSocket
#endif
