#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <limits>
#include <mutex>
#include <set>
#include "../../../src/engine/utlmemory.h"
#define S_API
#define S_CALLTYPE
#define CSE_STEAM_PROVIDER_PATH "controlled SDK leaf"
using SteamAPICall_t = std::uint64_t;
struct CCallbackBase {
    virtual void Run(void*) {}
    virtual void Run(void*, bool, SteamAPICall_t) {}
    virtual int GetCallbackSizeBytes() {
        return 0;
    }
    int m_nCallbackFlags = 0;
    int m_iCallback = 0;
};
using uint32 = std::uint32_t;
using uint16 = std::uint16_t;
enum EServerMode { ServerMode };
static bool retirement = false;
namespace cse::software_lifetime {
bool RetirementInProgress() noexcept {
    return retirement;
}
} // namespace cse::software_lifetime
static bool Reenter();
static unsigned int registrations = 0;
static unsigned int unregistrations = 0;
static unsigned int pumps = 0;
static unsigned int sessions = 0;
static unsigned int providerCloses = 0;
static unsigned int transportCloses = 0;
static bool initializeReady = true;
static bool providerCloseReady = false;
struct FakeApi {
    bool initialize() const {
        ++sessions;
        assert(!Reenter());
        return initializeReady;
    }
    bool initializeServer(uint32, uint16, uint16, uint16, EServerMode, const char*) const {
        ++sessions;
        assert(!Reenter());
        return initializeReady;
    }
    void shutdown() const {
        ++sessions;
        assert(!Reenter());
    }
    void shutdownServer() const {
        ++sessions;
        assert(!Reenter());
    }
    void registerCallback(CCallbackBase*, int) const {
        ++registrations;
    }
    void unregisterCallback(CCallbackBase*) const {
        ++unregistrations;
    }
    void registerCallResult(CCallbackBase*, SteamAPICall_t) const {
        ++registrations;
    }
    void unregisterCallResult(CCallbackBase*, SteamAPICall_t) const {
        ++unregistrations;
    }
    void runCallbacks() const {
        ++pumps;
        assert(!Reenter());
    }
    void runServerCallbacks() const {
        ++pumps;
        assert(!Reenter());
    }
};
namespace cse_steam {
using Api = FakeApi;
struct Provider {
    FakeApi api;
    void* module = nullptr;
    bool Open(const char*) {
        module = this;
        return true;
    }
    const FakeApi& Functions() {
        assert(module);
        return api;
    }
    const char* LastError() {
        return "controlled SDK close pending";
    }
    void* NativeModule() const {
        return module;
    }
    bool CloseChecked() {
        ++providerCloses;
        if (!providerCloseReady) {
            return false;
        }
        module = nullptr;
        return true;
    }
};
struct ServerTransport {
    bool ready = true;
    bool retained = false;
    bool Close() {
        ++transportCloses;
        if (ready) {
            retained = false;
        }
        return ready;
    }
    bool Retain(const Api&, void*) {
        retained = ready;
        return ready;
    }
    bool IsRetained() {
        return retained;
    }
    const char* LastError() {
        return "controlled transport close pending";
    }
};
} // namespace cse_steam
#include "steam_callbacks.inc"
#include "download_callbacks.inc"

struct DownloadResponse {
    static constexpr int k_iCallback = 1;
};

struct DownloadSink {
    int completions = 0;
    void Completed(DownloadResponse*, bool) {
        ++completions;
    }
};

static void CheckDeliveredDownloadCalls() {
    constexpr SteamAPICall_t outstanding = 71;
    constexpr SteamAPICall_t delivered = 72;
    DownloadSink sink;
    {
        CMultipleCallResults<DownloadSink, DownloadResponse> calls(&sink, &DownloadSink::Completed);
        calls.Track(outstanding);
        calls.Track(delivered);
        DownloadResponse response;
        calls.Run(&response, false, delivered);
        assert(sink.completions == 1);
        assert(Owner().callResults.count({&calls, delivered}) == 0);
        assert(Owner().callResults.count({&calls, outstanding}) == 1);
        assert(unregistrations == 0);
        calls.Run(&response, false, delivered);
        assert(sink.completions == 1);
    }
    assert(Owner().callResults.empty() && unregistrations == 1);
    registrations = 0;
    unregistrations = 0;
}
static bool Retire() {
#ifdef CSE_STEAM_BASELINE
    return CseSteamRetireServerTransport();
#else
    return CseSteamRetireModuleCallbacks();
#endif
}
static bool Reenter() {
    retirement = true;
    const bool result = Retire();
    retirement = false;
    return result;
}
int main() {
    CheckDeliveredDownloadCalls();
    auto& owner = Owner();
    CCallbackBase callback;
    SteamAPI_RegisterCallback(&callback, 1);
    SteamAPI_RegisterCallResult(&callback, 7);
    assert(registrations == 2 && unregistrations == 0);
    assert(SteamAPI_Init());
    retirement = true;
    assert(!Retire() && unregistrations == 0 && providerCloses == 0);
    retirement = false;
    SteamAPI_Shutdown();
    assert(SteamInternal_GameServer_Init(0, 0, 0, 0, ServerMode, "fixture"));
    retirement = true;
    assert(!Retire() && unregistrations == 0 && providerCloses == 0);
    retirement = false;
    SteamGameServer_Shutdown();
    // A successfully initialized SDK session is balanced if its transport owner cannot be retained.
    owner.serverTransport.ready = false;
    const auto beforePartialSession = sessions;
    assert(!SteamInternal_GameServer_Init(0, 0, 0, 0, ServerMode, "partial"));
    assert(sessions == beforePartialSession + 2);
    assert(!owner.serverActive && owner.sessionCalls == 0);
    owner.serverTransport.ready = true;
    // Failed partial initialization cannot publish an active session or permit nested retirement.
    initializeReady = false;
    assert(!SteamAPI_Init());
    assert(!SteamInternal_GameServer_Init(0, 0, 0, 0, ServerMode, "fixture"));
    SteamAPI_RunCallbacks();
    SteamGameServer_RunCallbacks();
    assert(pumps == 2 && unregistrations == 0 && !Retire());
    retirement = true;
    owner.serverTransport.ready = false;
    assert(!Retire() && unregistrations == 0 && providerCloses == 0);
    owner.serverTransport.ready = true;
    assert(!Retire() && unregistrations == 2 && owner.provider.NativeModule());
    const auto previousSessions = sessions;
    SteamAPI_RunCallbacks();
    SteamGameServer_RunCallbacks();
    assert(!SteamAPI_Init());
    assert(!SteamInternal_GameServer_Init(0, 0, 0, 0, ServerMode, "late"));
    SteamAPI_Shutdown();
    SteamGameServer_Shutdown();
    SteamAPI_RegisterCallback(&callback, 1);
    SteamAPI_RegisterCallResult(&callback, 9);
    SteamAPI_UnregisterCallback(&callback);
    SteamAPI_UnregisterCallResult(&callback, 7);
    assert(pumps == 2 && registrations == 2 && unregistrations == 2 && sessions == previousSessions);
    providerCloseReady = true;
    assert(Retire() && !owner.provider.NativeModule());
    const auto previousTransport = transportCloses;
    const auto previousProvider = providerCloses;
    FinalizeServerTransport();
    SteamAPI_UnregisterCallback(&callback);
    SteamAPI_UnregisterCallResult(&callback, 7);
    assert(Retire() && providerCloses == previousProvider && transportCloses == previousTransport);
    std::puts("PASS actual SDK owner: completed HTTP calls, active/partial sessions, complete pumps, "
              "close failure/retry and late callbacks/atexit");
}
