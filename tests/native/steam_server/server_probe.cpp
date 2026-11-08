#include "steam_service.h"
#include "native_module.h"
#include "isteamgameserver.h"
#include "isteamuser.h"
#include "isteamapps.h"
#include "isteamutils.h"
#include <memory>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <thread>

namespace {
constexpr std::uint32_t LoopbackHostIP = 0x7f000001;
constexpr std::uint16_t FixtureGamePort = 27450;
constexpr int SessionCycles = 2;
constexpr int FixtureMaxPlayers = 4;
constexpr std::uint32_t CounterStrikeAppId = 10;
constexpr std::uint32_t HalfLifeAppId = 70;
constexpr char LegacyServerVersion[] = "1.1.2.7";
constexpr auto CallbackInterval = std::chrono::milliseconds(10);
constexpr auto LoginTimeout = std::chrono::seconds(30);
constexpr auto LogoffTimeout = std::chrono::seconds(5);
constexpr std::size_t MaxSdkPacketBytes = 65536;

void Check(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

class ServerCallbacks {
public:
    int connections = 0;
    int policies = 0;
    int failures = 0;
    int terminalFailures = 0;
    int securePolicies = 0;

    ServerCallbacks() : connected_(this, &ServerCallbacks::Connected),
        policy_(this, &ServerCallbacks::Policy), failed_(this, &ServerCallbacks::Failed) {}

private:
    void Connected(SteamServersConnected_t* value) {
        Check(value != nullptr, "typed server connected callback");
        ++connections;
    }
    void Policy(GSPolicyResponse_t* value) {
        Check(value != nullptr && value->m_bSecure <= 1, "typed server security-policy payload");
        ++policies;
        securePolicies += value->m_bSecure;
    }
    void Failed(SteamServerConnectFailure_t* value) {
        Check(value != nullptr, "typed server failure callback");
        ++failures;
        if (!value->m_bStillRetrying) {
            ++terminalFailures;
        }
        std::fprintf(stderr, "server_failure_reason=%d retrying=%d\n",
            static_cast<int>(value->m_eResult), value->m_bStillRetrying ? 1 : 0);
    }
    CCallback<ServerCallbacks, SteamServersConnected_t, true> connected_;
    CCallback<ServerCallbacks, GSPolicyResponse_t, true> policy_;
    CCallback<ServerCallbacks, SteamServerConnectFailure_t, true> failed_;
};

// Experimental transport owner; SDK flat signatures are pinned to Valve's public header.
struct AnonymousTransport {
    using CreatePipe = HSteamPipe (S_CALLTYPE*)(void*);
    using ReleasePipe = bool (S_CALLTYPE*)(void*, HSteamPipe);
    using Shutdown = bool (S_CALLTYPE*)(void*);
    void* client = nullptr;
    HSteamPipe pipe = 0;
    ReleasePipe release = nullptr;
    Shutdown shutdown = nullptr;

    void Retain() {
        if (pipe) {
            return;
        }
        void* module = cse_platform::FindLoadedModule(CSE_SERVER_PROVIDER_SOURCE_PATH);
#if defined(CSE_SERVER_PROVIDER_STAGED_PATH)
        if (!module) {
            module = cse_platform::FindLoadedModule(CSE_SERVER_PROVIDER_STAGED_PATH);
        }
#endif
        Check(module != nullptr, "borrow the actual loaded provider for pipe functions");
        const auto create = reinterpret_cast<CreatePipe>(
            cse_platform::GetSymbol(module, "SteamAPI_ISteamClient_CreateSteamPipe"));
        release = reinterpret_cast<ReleasePipe>(
            cse_platform::GetSymbol(module, "SteamAPI_ISteamClient_BReleaseSteamPipe"));
        shutdown = reinterpret_cast<Shutdown>(
            cse_platform::GetSymbol(module, "SteamAPI_ISteamClient_BShutdownIfAllPipesClosed"));
        Check(create && release && shutdown, "exact three pinned SDK pipe functions");
        client = SteamInternal_CreateInterface("SteamClient020");
        Check(client != nullptr, "exact global SteamClient020 interface");
        pipe = create(client);
        Check(pipe != 0, "create an anonymous transport without connecting a user");
        Check(SteamAPI_GetHSteamUser() == 0 && SteamAPI_GetHSteamPipe() == 0,
            "anonymous transport does not initialize a logged-in client session");
        std::puts("anonymous_transport_retained=1 client_session=0");
    }
    void Close() {
        if (pipe) {
            const HSteamPipe oldPipe = pipe;
            pipe = 0;
            Check(release(client, oldPipe), "release the owned anonymous transport");
            const bool stopped = shutdown(client);
            std::printf("anonymous_transport_released=1 global_transport_stopped=%d\n", stopped ? 1 : 0);
        }
    }
    ~AnonymousTransport() {
        if (pipe) {
            release(client, pipe);
            shutdown(client);
        }
    }
};

struct ServerSession {
    bool initialized = false;
    ~ServerSession() {
        if (initialized) {
            if (SteamGameServer()) {
                SteamGameServer()->SetAdvertiseServerActive(false);
                SteamGameServer()->LogOff();
            }
            SteamGameServer_Shutdown();
        }
    }
};
}

static int RunServerFixture(int argc, const char* const* argv) {
    const bool singleSessionDiagnostic = argc == 4 && std::strcmp(argv[3], "--single-session-diagnostic") == 0;
    const bool listenContext = argc == 4 && std::strcmp(argv[3], "--listen-server-context") == 0;
    const bool retainAnonymousTransport = argc == 4 && std::strcmp(argv[3], "--retain-anonymous-transport") == 0;
    if ((argc != 3 && !singleSessionDiagnostic && !listenContext && !retainAnonymousTransport)
        || std::strcmp(argv[1], "--run-isolated-server-session") != 0
        || (std::strcmp(argv[2], "10") != 0 && std::strcmp(argv[2], "70") != 0)) {
        std::fputs("Explicit isolated server provider invocation required\n", stderr);
        return 2;
    }
    const bool halfLife = std::strcmp(argv[2], "70") == 0;
    const std::uint32_t application = halfLife ? HalfLifeAppId : CounterStrikeAppId;
    struct ClientSession {
        bool initialized = false;
        ~ClientSession() {
            if (initialized) {
                SteamAPI_Shutdown();
            }
        }
    } clientSession;
    try {
        AnonymousTransport anonymousTransport;
        if (listenContext) {
            Check(SteamAPI_Init(), "listen-server real client initialization");
            clientSession.initialized = true;
            Check(SteamUser() && SteamApps() && SteamUtils(), "listen-server client interfaces");
            Check(SteamUser()->BLoggedOn() && SteamApps()->BIsSubscribed(), "listen-server licensed real user");
            Check(SteamUtils()->GetAppID() == application, "listen-server client application identity");
        }
        for (int cycle = 0; cycle < (singleSessionDiagnostic ? 1 : SessionCycles); ++cycle) {
            // Steam_Activate allocates the owner before Init; Steam_Shutdown
            // shuts down the SDK and deletes it before process/library teardown.
            auto callbacks = std::make_unique<ServerCallbacks>();
            Check(reinterpret_cast<std::uintptr_t>(callbacks.get()) > UINT32_MAX,
                "actual server callback owner above 4 GiB");
            ServerSession session;
            Check(SteamInternal_GameServer_Init(LoopbackHostIP, 0, FixtureGamePort,
                STEAMGAMESERVER_QUERY_PORT_SHARED, eServerModeAuthenticationAndSecure, LegacyServerVersion),
                "real owned native server initialization");
            session.initialized = true;
            if (retainAnonymousTransport) {
                anonymousTransport.Retain();
            }
            if (!listenContext) {
                Check(SteamAPI_GetHSteamUser() == 0 && SteamAPI_GetHSteamPipe() == 0,
                    "standalone server has no logged-in client session");
            }
            ISteamGameServer* server = SteamGameServer();
            Check(server != nullptr && reinterpret_cast<std::uintptr_t>(server) > UINT32_MAX,
                "exact SteamGameServer015 native interface above 4 GiB");
            Check(SteamGameServer_GetHSteamUser() != 0 && SteamGameServer_GetHSteamPipe() != 0,
                "native server user and pipe handles");
            Check(SteamGameServer() == server, "server SDK accessor identity");
            ISteamUtils* utilities = SteamGameServerUtils();
            Check(utilities != nullptr && reinterpret_cast<std::uintptr_t>(utilities) > UINT32_MAX,
                "exact SteamUtils010 server interface above 4 GiB");
            Check(utilities->GetAppID() == application, "actual requested CS/HL server app identity");
            server->SetProduct(halfLife ? "valve" : "cstrike");
            server->SetGameDescription(halfLife ? "Half-Life" : "Counter-Strike");
            server->SetModDir(halfLife ? "valve" : "cstrike");
            server->SetDedicatedServer(true);
            server->SetServerName("CSE isolated native provider fixture");
            server->SetMapName(halfLife ? "crossfire" : "de_dust2");
            server->SetMaxPlayerCount(FixtureMaxPlayers);
            server->SetBotPlayerCount(0);
            server->SetPasswordProtected(true);
            server->SetAdvertiseServerActive(false);
            server->ClearAllKeyValues();
            server->SetKeyValue("cse_provider_fixture", "1");
            const int previousConnections = callbacks->connections;
            const int previousSecurePolicies = callbacks->securePolicies;
            const int previousTerminalFailures = callbacks->terminalFailures;
            server->LogOnAnonymous();
            const auto deadline = std::chrono::steady_clock::now() + LoginTimeout;
            while (std::chrono::steady_clock::now() < deadline) {
                SteamGameServer_RunCallbacks();
                if (listenContext) {
                    SteamAPI_RunCallbacks();
                }
                Check(callbacks->terminalFailures == previousTerminalFailures, "Steam server login without terminal failure");
                if (callbacks->connections > previousConnections && callbacks->securePolicies > previousSecurePolicies
                    && server->BLoggedOn() && server->BSecure()) {
                    break;
                }
                std::this_thread::sleep_for(CallbackInterval);
            }
            Check(callbacks->connections > previousConnections && callbacks->securePolicies > previousSecurePolicies,
                "real server connected and secure-policy callbacks before deadline");
            Check(server->BLoggedOn() && server->BSecure() && server->GetSteamID().IsValid(),
                "logged-in secure native server with real identity");
            server->SetAdvertiseServerActive(false);
            unsigned char pendingPacket[MaxSdkPacketBytes];
            uint32 pendingIP = 0;
            uint16 pendingPort = 0;
            const int pendingBytes = server->GetNextOutgoingPacket(
                pendingPacket, sizeof(pendingPacket), &pendingIP, &pendingPort);
            Check(pendingBytes == 0, "non-advertised fixture requires no unimplemented UDP forwarding");
            std::printf("server_pending_sdk_packet_bytes=%d\n", pendingBytes);
            std::fflush(stdout);
            server->LogOff();
            const auto logoffDeadline = std::chrono::steady_clock::now() + LogoffTimeout;
            do {
                SteamGameServer_RunCallbacks();
                if (!server->BLoggedOn()) {
                    break;
                }
                std::this_thread::sleep_for(CallbackInterval);
            } while (std::chrono::steady_clock::now() < logoffDeadline);
            Check(!server->BLoggedOn(), "server logoff completes before SDK shutdown");
            SteamGameServer_Shutdown();
            session.initialized = false;
            // The SDK does not promise zero internal handles after shutdown. Record the
            // implementation's state and require a complete fresh login/policy cycle next.
            std::printf("server_post_shutdown_user=%d pipe=%d\n",
                SteamGameServer_GetHSteamUser(), SteamGameServer_GetHSteamPipe());
            std::printf("server_cycle=%d application=%u connected_callback=1 secure_policy_callback=1 secure=1 shutdown=1\n",
                cycle + 1, application);
            std::fflush(stdout);
        }
        anonymousTransport.Close();
        std::puts("native_steam_server_provider_passed=1");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "fixture_failure=%s\n", error.what());
        return 1;
    }
}

#if defined(CSE_SERVER_MODULE_FIXTURE)
#if defined(_WIN32)
extern "C" __declspec(dllexport) int CseSteamServerFixtureRun(const char* application, const char* mode) {
#else
extern "C" __attribute__((visibility("default"))) int CseSteamServerFixtureRun(
    const char* application, const char* mode) {
#endif
    const char* arguments[] = {"steam_server_probe", "--run-isolated-server-session", application, mode};
    return RunServerFixture(mode ? 4 : 3, arguments);
}
#else
int main(int argc, char** argv) {
    return RunServerFixture(argc, argv);
}
#endif
