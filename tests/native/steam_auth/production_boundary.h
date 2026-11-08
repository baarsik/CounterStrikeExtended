#ifndef CSE_STEAM_AUTH_PRODUCTION_BOUNDARY_H
#define CSE_STEAM_AUTH_PRODUCTION_BOUNDARY_H
#if defined(_WIN32)
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
constexpr int MAX_PATH = 260;
#endif
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

using byte = unsigned char;
using qboolean = int;
#ifndef TRUE
constexpr qboolean TRUE = 1;
constexpr qboolean FALSE = 0;
#endif
constexpr int PROTOCOL_VERSION = 48;
constexpr int PROTOCOL_STEAM = 3;
constexpr int PROTOCOL_HASHEDCDKEY = 2;
constexpr int PORT_SERVER = 27015;
constexpr unsigned short FixtureServerPort = 27438;
constexpr std::uint32_t LoopbackHostIP = 0x7f000001;
enum { NA_LOOPBACK, NA_IP };
enum { NS_CLIENT };
enum { ca_disconnected };
struct netadr_t {
    int type;
    byte ip[4];
    unsigned short port;
};
struct ConnectionState {
    char servername[260];
    char userinfo[256];
    double connect_time;
    int connect_retry;
    int state;
    int challenge;
    byte authprotocol;
    std::uint64_t GameServerSteamID;
    bool isVAC2Secure;
};
extern ConnectionState cls;
extern std::vector<byte> capturedPacket;
extern bool fixtureDisconnected;
extern void (*fixturePacketSink)(int, const void*);
void ResetFixture();
void CL_SendConnectPacket();
void TerminateFixtureConnection();
void Require(bool condition, const char* message);

#if !defined(CSE_REAL_AUTH_PROVIDER)
struct FakeSteamUserState {
    int ticketSize = 0;
    int initiations = 0;
    int terminations = 0;
    std::uint32_t lastIP = 0;
    unsigned short lastPort = 0;
    std::uint64_t lastServer = 0;
    bool lastSecure = false;
    std::uintptr_t lastTicketBuffer = 0;
};
FakeSteamUserState& FakeSteamUser();
#endif
#endif
