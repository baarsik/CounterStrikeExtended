#include "production_boundary.h"
#if defined(CSE_REAL_AUTH_PROVIDER)
#include "steam_service.h"
#include "isteamuser.h"
#include "isteamapps.h"
#include "isteamutils.h"
#else
using uint64 = std::uint64_t;
using uint32 = std::uint32_t;
using uint16 = std::uint16_t;
struct CSteamID {
    uint64 value;
    explicit CSteamID(uint64 identity = 0) : value(identity) {}
};
static FakeSteamUserState fakeSteamUser;
struct MockUser {
    int InitiateGameConnection_DEPRECATED(void* output, int capacity, CSteamID server,
        uint32 ip, uint16 port, bool secure) {
        ++fakeSteamUser.initiations;
        fakeSteamUser.lastTicketBuffer = reinterpret_cast<std::uintptr_t>(output);
        fakeSteamUser.lastIP = ip;
        fakeSteamUser.lastPort = port;
        fakeSteamUser.lastServer = server.value;
        fakeSteamUser.lastSecure = secure;
        if (fakeSteamUser.ticketSize > 0 && fakeSteamUser.ticketSize <= capacity) {
            std::memset(output, 0xa5, static_cast<std::size_t>(fakeSteamUser.ticketSize));
        }
        return fakeSteamUser.ticketSize;
    }
    void TerminateGameConnection_DEPRECATED(uint32, uint16) {
        ++fakeSteamUser.terminations;
    }
    void AdvertiseGame(CSteamID, uint32, uint16) {}
};
static MockUser mockUser;
static MockUser* SteamUser() {
    return &mockUser;
}
FakeSteamUserState& FakeSteamUser() {
    return fakeSteamUser;
}
#endif
#include <cstdarg>
#include <cstdlib>
#include <cctype>
#if !defined(_WIN32)
#include <strings.h>
#define _snprintf snprintf
#endif
#define Q_memcpy std::memcpy
#define Q_memset std::memset
#define Q_strncpy std::strncpy
#define Q_strstr std::strstr
#define Q_strcmp std::strcmp
#define Q_strncmp std::strncmp
#if defined(_WIN32)
#define Q_strcasecmp _stricmp
#else
#define Q_strcasecmp strcasecmp
#endif
static int Q_strlen(const char* value) {
    return static_cast<int>(std::strlen(value));
}
// This fixture supplies ASCII userinfo. UTF-8 validation is not qualified here.
static bool Q_UnicodeValidate(const char* value) {
    for (; *value; ++value) {
        if (static_cast<unsigned char>(*value) >= 0x80) {
            return false;
        }
    }
    return true;
}
static constexpr char com_empty_string[] = "";
static constexpr int MAX_INFO_KEY = 128;
static constexpr int MAX_INFO_VALUE = 128;
static constexpr int INFO_VALUE_BUFFERS = 4;
static void Con_Printf(const char* format, ...) {
    va_list arguments;
    va_start(arguments, format);
    std::vfprintf(stderr, format, arguments);
    va_end(arguments);
}
static char* va(const char* format, ...) {
    static char formatted[128];
    va_list arguments;
    va_start(arguments, format);
    std::vsnprintf(formatted, sizeof(formatted), format, arguments);
    va_end(arguments);
    return formatted;
}
void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "fixture_failure=%s\n", message);
        std::exit(1);
    }
}
static short BigShort(short value) {
    return static_cast<short>(htons(static_cast<unsigned short>(value)));
}
static int BigLong(int value) {
    return static_cast<int>(htonl(static_cast<std::uint32_t>(value)));
}
static bool NET_IsLocalAddress(netadr_t address) {
    return address.ip[0] == 127;
}
static uint32 g_nFakeIP;
static unsigned short g_arFakePorts[1];
static uint64 g_currentGameConnectionSteamID;
static uint32 g_nCurrentGameConnectionPort;
static uint32 g_nCurrentGameConnectionIP;
static char g_szRichPresenceNiceMap[1];
static char g_szRichPresenceOverride[1];
static char g_szRichPresenceGameMode[1];
static constexpr int GAME_CONNECTION_BLOB_BYTES = 1024;
class CSteam3Client {
public:
    int InitiateGameConnection(void*, int, uint64, uint32, uint16, bool);
    void TerminateConnection();
};
#define ASSERT_AT(condition, line) Require(condition, "production assertion at " #line)
#include "actual_steam_connection.h"
static CSteam3Client steamClient;
void TerminateFixtureConnection() {
    steamClient.TerminateConnection();
}
static int Steam_GSInitiateGameConnection(void* data, int size, uint64 server, uint32 ip, uint16 port, bool secure) {
    return steamClient.InitiateGameConnection(data, size, server, ip, port, secure);
}
struct MD5Context_t {
    std::uint32_t buf[4];
    std::uint32_t bits[2];
    byte in[64];
};
static constexpr int MD5_DIGEST_BYTES = 16;
void MD5Transform(std::uint32_t*, const byte*);
#include "actual_md5.h"
// Host identity is a deterministic fixture input, not an account/license credential.
static void Sys_GetCDKey(char* output, int* length, int* dedicated) {
    constexpr char FixtureHostIdentity[] = "127.0.0.1";
    std::memcpy(output, FixtureHostIdentity, sizeof(FixtureHostIdentity));
    *length = sizeof(FixtureHostIdentity) - 1;
    *dedicated = 0;
}
#include "actual_info.h"
ConnectionState cls;
std::vector<byte> capturedPacket;
bool fixtureDisconnected;
void (*fixturePacketSink)(int, const void*);
static struct { float value; } fs_startup_timings;
static double realtime;
static netadr_t g_GameServerAddress;
static void ContinueLoadingProgressBar(const char*, int, float) {}
static void SetupStartupTimings() {}
static void CL_Disconnect_f() {
    fixtureDisconnected = true;
}
static bool NET_StringToAdr(const char*, netadr_t* output) {
    *output = {};
    output->type = NA_IP;
    NET_NetAdr_SetIP(output, LoopbackHostIP);
    NET_NetAdr_SetPort(output, FixtureServerPort);
    return true;
}
static void NET_SendPacket(int, int length, const void* data, netadr_t) {
    const auto* bytes = static_cast<const byte*>(data);
    capturedPacket.assign(bytes, bytes + length);
    if (fixturePacketSink) {
        fixturePacketSink(length, data);
    }
}
#include "actual_connection.h"
void ResetFixture() {
    cls = {};
    std::strcpy(cls.servername, "127.0.0.1:27438");
    std::strcpy(cls.userinfo,
        "\\name\\CSE Native Auth Fixture\\rate\\25000\\cl_updaterate\\20\\cl_lw\\1\\cl_lc\\1");
    fixtureDisconnected = false;
    capturedPacket.clear();
    fixturePacketSink = nullptr;
#if !defined(CSE_REAL_AUTH_PROVIDER)
    fakeSteamUser = {};
#endif
}
