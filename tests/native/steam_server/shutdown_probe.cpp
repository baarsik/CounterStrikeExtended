#include "steam_service.h"
#include <cstdio>
#include <cstring>

static int RunShutdownProbe(int argc, char **argv) {
    const bool serverOnly = argc == 2 && std::strcmp(argv[1], "--run-isolated-server-shutdown") == 0;
    if (!serverOnly && (argc != 2 || std::strcmp(argv[1], "--run-isolated-client-server-shutdown") != 0)) {
        return 2;
    }
    if (!serverOnly && !SteamAPI_Init()) {
        std::fputs("client initialization failed\n", stderr);
        return 1;
    }
    constexpr uint32 LoopbackHostIP = 0x7f000001;
    constexpr uint16 FixtureGamePort = 27460;
    constexpr char LegacyServerVersion[] = "1.1.2.7";
    for (int cycle = 0; cycle < (serverOnly ? 2 : 1); ++cycle) {
        if (!SteamInternal_GameServer_Init(LoopbackHostIP, 0, FixtureGamePort, STEAMGAMESERVER_QUERY_PORT_SHARED,
                                           eServerModeAuthenticationAndSecure, LegacyServerVersion)) {
            if (!serverOnly) {
                SteamAPI_Shutdown();
            }
            std::fputs("server initialization failed\n", stderr);
            return 1;
        }
        SteamGameServer_Shutdown();
    }
    // Mirror the real listen-server exit: stop its server, then its client,
    // then destroy the module's transport/provider owner at process teardown.
    if (!serverOnly) {
        SteamAPI_Shutdown();
    }
    std::puts(serverOnly ? "native_server_shutdown_returned=1" : "native_client_server_shutdown_returned=1");
    return 0;
}

#if defined(CSE_SHUTDOWN_MODULE_FIXTURE)
#if defined(_WIN32)
extern "C" __declspec(dllexport)
#else
extern "C" __attribute__((visibility("default")))
#endif
int CseSteamShutdownFixture(const char *mode) {
    char *arguments[] = {const_cast<char *>("shutdown"), const_cast<char *>(mode)};
    return RunShutdownProbe(2, arguments);
}
#if defined(__APPLE__)
extern "C" __attribute__((visibility("default"))) bool CseSteamShutdownFixtureRetire() {
    return CseSteamRetireServerTransport();
}
#endif

#else
int main(int argc, char **argv) {
    return RunShutdownProbe(argc, argv);
}
#endif
