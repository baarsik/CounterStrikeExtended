#include "steam_service.h"
#include "isteamapps.h"
#include "isteamfriends.h"
#include "isteaminput.h"
#include "isteamuser.h"
#include "isteamutils.h"
#define STEAMNETWORKINGSOCKETS_STEAMAPI
#include "isteamnetworkingsockets.h"
#include "isteamnetworkingutils.h"

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <thread>

namespace {
void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "Owned Steam service fixture failed: %s\n", message);
        std::exit(1);
    }
}

class ClientCallbacks final {
public:
    ClientCallbacks() : connected_(this, &ClientCallbacks::Connected) {
    }

    void Signature(CheckFileSignature_t* result, bool ioFailure) {
        Require(result && !ioFailure, "real typed SDK call result");
        Require(result->m_eCheckFileSignature >= k_ECheckFileSignatureInvalidSignature
            && result->m_eCheckFileSignature <= k_ECheckFileSignatureNoSignaturesFoundForThisFile,
            "real SDK call-result payload layout");
        ++signatures;
    }

    void RequestSignature(const char* path) {
        const SteamAPICall_t call = SteamUtils()->CheckFileSignature(path);
        Require(call != k_uAPICallInvalid, "real asynchronous utility request");
        signature_.Set(call, this, &ClientCallbacks::Signature);
    }

    int signatures = 0;
    int connected = 0;

private:
    void Connected(SteamServersConnected_t* value) {
        Require(value != nullptr, "real typed SDK connected callback");
        ++connected;
    }

    CCallback<ClientCallbacks, SteamServersConnected_t> connected_;
    CCallResult<ClientCallbacks, CheckFileSignature_t> signature_;
};

struct LateGlobalSdkUser {
    ~LateGlobalSdkUser() {
        (void)SteamAPI_GetHSteamPipe();
        std::puts("late_global_sdk_access=1");
    }
};
LateGlobalSdkUser lateGlobalSdkUser;

// Matches the engine's callback registration during static construction, before initialization.
ClientCallbacks clientCallbacks;
} // namespace

#if defined(_WIN32)
extern "C" void CseSteamFixtureLateCleanup() {
#else
extern "C" __attribute__((visibility("hidden"))) void CseSteamFixtureLateCleanup() {
#endif
    (void)SteamAPI_GetHSteamPipe();
    std::puts("indirect_global_sdk_access=1");
}

#if defined(CSE_SERVICE_MODULE_FIXTURE)
#if defined(_WIN32)
extern "C" __declspec(dllexport) int CseSteamServiceFixtureRun(const char* executablePath) {
#else
extern "C" __attribute__((visibility("default"))) int CseSteamServiceFixtureRun(const char* executablePath) {
#endif
#else
int main(int argc, char** argv) {
    Require(argc == 1, "no runtime provider override; build selects an explicit pinned provider");
    const char* executablePath = argv[0];
#endif
#if UINTPTR_MAX > UINT32_MAX
    Require(reinterpret_cast<std::uintptr_t>(&clientCallbacks) > UINT32_MAX,
        "real SDK callbacks retain a native high-address owner");
#endif
    constexpr AppId_t CounterStrikeAppId = 10;
    constexpr AppId_t HalfLifeAppId = 70;
    const char* requestedApplication = std::getenv("SteamAppId");
    Require(requestedApplication && (std::strcmp(requestedApplication, "10") == 0
        || std::strcmp(requestedApplication, "70") == 0), "explicit CS or Half-Life fixture application");
    const AppId_t expectedApplication = std::strcmp(requestedApplication, "10") == 0
        ? CounterStrikeAppId : HalfLifeAppId;
#if defined(CSE_SERVICE_MODULE_FIXTURE)
    constexpr int ClientCycles = 1;
#else
    constexpr int ClientCycles = 2;
#endif
    constexpr int CallbackPumps = 500;
    constexpr int ResultSettlingPumps = 20;
    constexpr auto CallbackInterval = std::chrono::milliseconds(10);
    for (int cycle = 0; cycle < ClientCycles; ++cycle) {
        Require(SteamAPI_Init(), "real client session initialization through owned routing");
        Require(SteamAPI_GetHSteamUser() != 0 && SteamAPI_GetHSteamPipe() != 0, "real client handles");
        Require(SteamUser() && SteamApps() && SteamFriends() && SteamUtils() && SteamInput()
            && SteamNetworkingSockets_SteamAPI() && SteamNetworkingUtils_SteamAPI(), "seven exact SDK accessors");
        Require(SteamUser() == SteamUser(), "SDK cached accessor identity");
        Require(SteamUser()->BLoggedOn() && SteamApps()->BIsSubscribed(), "logged-in licensed session");
        Require(SteamUtils()->GetAppID() == expectedApplication, "actual requested game app identity");
        Require(SteamUser()->GetSteamID().IsValid(), "real user identity");
        const uint32 voiceRate = SteamUser()->GetVoiceOptimalSampleRate();
        Require(voiceRate > 0, "real decoder sample-rate query");
        const int signaturesBefore = clientCallbacks.signatures;
        clientCallbacks.RequestSignature(executablePath);
        int settlingPumps = 0;
        for (int pump = 0; pump < CallbackPumps; ++pump) {
            SteamAPI_RunCallbacks();
            if (clientCallbacks.signatures == signaturesBefore + 1) {
                ++settlingPumps;
                if (settlingPumps == ResultSettlingPumps) {
                    break;
                }
            }
            std::this_thread::sleep_for(CallbackInterval);
        }
        Require(clientCallbacks.signatures == signaturesBefore + 1, "exactly one actual SDK call-result delivery");
        SteamAPI_Shutdown();
        std::printf("client_cycle=%d application=%u licensed=1 sdk_call_result=1 voice_decoder_rate=%u\n",
            cycle + 1, expectedApplication, voiceRate);
    }
    std::printf("connected_notifications=%d\n", clientCallbacks.connected);
    std::puts("owned_steam_service_passed=1");
    return 0;
}
