#include "steam_provider.h"
#include "isteamapps.h"
#include "isteamfriends.h"
#include "isteamgameserver.h"
#include "isteaminput.h"
#include "isteamuser.h"
#include "isteamutils.h"
#define STEAMNETWORKINGSOCKETS_STEAMAPI
#include "isteamnetworkingsockets.h"
#include "isteamnetworkingutils.h"
#include "steam_gameserver.h"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <thread>

namespace {
void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "Steam provider probe failed: %s\n", message);
        std::exit(1);
    }
}

using cse_steam::Api;

struct Context {
    void (S_CALLTYPE* initialize)(void*);
    std::uintptr_t counter;
    void* value;
};
static_assert(sizeof(Context) == 3 * sizeof(void*), "Steam context uses three pointer-aligned fields");

const Api* contextApi = nullptr;

void S_CALLTYPE InitializeUserContext(void* storage) {
    *static_cast<void**>(storage) = contextApi->userInterface(contextApi->getUser(), STEAMUSER_INTERFACE_VERSION);
}

struct Callback final : CCallbackBase {
    int deliveries = 0;
    bool malformed = false;

    void Run(void* value) override {
        malformed = malformed || value == nullptr;
        ++deliveries;
    }
    void Run(void*, bool, SteamAPICall_t) override {
        malformed = true;
    }
    int GetCallbackSizeBytes() override {
        return sizeof(SteamServersConnected_t);
    }
};

struct SignatureResult final : CCallbackBase {
    SteamAPICall_t expectedCall = k_uAPICallInvalid;
    int deliveries = 0;
    bool malformed = false;

    SignatureResult() {
        m_iCallback = CheckFileSignature_t::k_iCallback;
    }
    void Run(void*) override {
        malformed = true;
    }
    void Run(void* value, bool ioFailure, SteamAPICall_t call) override {
        malformed = malformed || !value || ioFailure || call != expectedCall;
        if (value) {
            const auto result = static_cast<CheckFileSignature_t*>(value)->m_eCheckFileSignature;
            malformed = malformed || result < k_ECheckFileSignatureInvalidSignature
                || result > k_ECheckFileSignatureNoSignaturesFoundForThisFile;
        }
        ++deliveries;
    }
    int GetCallbackSizeBytes() override {
        return sizeof(CheckFileSignature_t);
    }
};

void CheckClient(const Api& api, const char* probePath) {
    Callback connected;
    // Production registers callbacks during static construction, before SteamAPI_Init.
    api.registerCallback(&connected, SteamServersConnected_t::k_iCallback);
    Require(api.initialize(), "real SteamAPI_Init with an installed, logged-in client");
    const HSteamUser user = api.getUser();
    Require(user != 0 && api.getPipe() != 0, "real client user and pipe handles");
    const char* versions[] = {
        STEAMUSER_INTERFACE_VERSION, STEAMAPPS_INTERFACE_VERSION, STEAMFRIENDS_INTERFACE_VERSION,
        STEAMUTILS_INTERFACE_VERSION, STEAMINPUT_INTERFACE_VERSION,
        STEAMNETWORKINGSOCKETS_INTERFACE_VERSION, STEAMNETWORKINGUTILS_INTERFACE_VERSION
    };
    for (const char* version : versions) {
        void* resolvedInterface = api.userInterface(user, version);
        std::printf("client_interface=%s available=%d\n", version, resolvedInterface != nullptr);
        Require(resolvedInterface != nullptr, version);
    }
    auto* steamUser = static_cast<ISteamUser*>(api.userInterface(user, STEAMUSER_INTERFACE_VERSION));
    contextApi = &api;
    Context context = {InitializeUserContext, 0, nullptr};
    Require(api.initializeContext(&context) == &context.value && context.value == steamUser,
        "actual SDK pointer-width context layout and initialization");
    Require(api.initializeContext(&context) == &context.value && context.value == steamUser,
        "cached SDK context retains interface identity");
    contextApi = nullptr;
    auto* apps = static_cast<ISteamApps*>(api.userInterface(user, STEAMAPPS_INTERFACE_VERSION));
    auto* utils = static_cast<ISteamUtils*>(api.userInterface(0, STEAMUTILS_INTERFACE_VERSION));
    Require(steamUser->BLoggedOn() && apps->BIsSubscribed(), "real logged-in and licensed application session");
    constexpr AppId_t HalfLifeAppId = 70;
    Require(utils && utils->GetAppID() == HalfLifeAppId, "actual Half-Life application identity");
    Require(steamUser->GetSteamID().IsValid(), "real user identity without exposing account identifiers");
    const uint32 voiceRate = steamUser->GetVoiceOptimalSampleRate();
    Require(voiceRate > 0, "voice interface supplies a real decoder sample rate");
    std::printf("licensed_session=1 application=%u voice_decoder_rate=%u\n", utils->GetAppID(), voiceRate);
    SignatureResult signature;
    signature.expectedCall = utils->CheckFileSignature(probePath);
    Require(signature.expectedCall != k_uAPICallInvalid, "real asynchronous utility request");
    api.registerCallResult(&signature, signature.expectedCall);
    constexpr int CallbackPumps = 500;
    constexpr auto CallbackInterval = std::chrono::milliseconds(10);
    for (int index = 0; index < CallbackPumps; ++index) {
        api.runCallbacks();
        std::this_thread::sleep_for(CallbackInterval);
    }
    api.unregisterCallResult(&signature, signature.expectedCall);
    Require(signature.deliveries == 1 && !signature.malformed, "actual native call-result delivery and layout");
    std::printf("signature_call_result_deliveries=%d\n", signature.deliveries);
    api.unregisterCallback(&connected);
    Require(!connected.malformed, "native callback ABI receives correctly shaped calls");
    std::printf("connected_callback_deliveries=%d\n", connected.deliveries);
    api.shutdown();
}
} // namespace

int main(int argc, char** argv) {
    Require(argc == 3, "explicit provider path and --bind or --client mode required");
    Require(std::strcmp(argv[2], "--bind") == 0 || std::strcmp(argv[2], "--client") == 0, "known explicit mode");
    cse_steam::Provider provider;
    if (!provider.Open(argv[1])) {
        std::fprintf(stderr, "Steam provider binding failed: %s\n", provider.LastError());
        return 1;
    }
    const Api& api = provider.Functions();
    Require(!provider.Open(argv[1]) && api.initialize,
        "duplicate open rejected without invalidating the already bound provider");
    std::puts("required_exports_bound=22");
    if (std::strcmp(argv[2], "--client") == 0) {
        constexpr int ClientLifecycleCycles = 2;
        for (int cycle = 0; cycle < ClientLifecycleCycles; ++cycle) {
            CheckClient(api, argv[0]);
            std::printf("client_lifecycle_cycle=%d passed=1\n", cycle + 1);
        }
    }
    provider.Close();
    Require(!api.initialize, "closing the owner invalidates its function table");
    std::puts("provider_probe_passed=1");
    return 0;
}
