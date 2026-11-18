#if defined(CSE_OWNED_STEAM_PROVIDER)
#include "../public/steam_service.h"
#include "../public/steam_provider.h"
#if defined(__APPLE__)
#include "../public/steam_server_transport.h"
#endif

#include <cstdio>
#include <cstdlib>
#if defined(__APPLE__) && defined(SW_RENDERER)
#include "../engine/native_software_lifetime.h"
#include <limits>
#include <set>
#include <mutex>
#endif

#ifndef CSE_STEAM_PROVIDER_PATH
#if defined(_M_ARM64) && defined(_WIN32)
#define CSE_STEAM_PROVIDER_PATH "steam_api_arm64.dll"
#elif defined(_WIN32)
#define CSE_STEAM_PROVIDER_PATH "steam_api64.dll"
#elif defined(__APPLE__)
#define CSE_STEAM_PROVIDER_PATH "libsteam_api.dylib"
#else
#define CSE_STEAM_PROVIDER_PATH "libsteam_api.so"
#endif
#endif

namespace {
struct NativeProvider {
    cse_steam::Provider provider;
#if defined(__APPLE__) && defined(SW_RENDERER)
    std::recursive_mutex callbacksMutex;
    std::set<CCallbackBase*> callbacks;
    std::set<std::pair<CCallbackBase*, SteamAPICall_t>> callResults;
    unsigned int callbackPumps = 0;
    unsigned int sessionCalls = 0;
    bool clientActive = false;
    bool serverActive = false;
    bool callbacksClosed = false;
#endif
#if defined(__APPLE__)
    cse_steam::ServerTransport serverTransport;

    ~NativeProvider() {
#if defined(SW_RENDERER)
        if (callbacksClosed && !provider.NativeModule()) {
            return;
        }
#endif
        if (!serverTransport.Close()) {
            std::fprintf(stderr, "Unable to release native Steam server transport: %s\n",
                serverTransport.LastError());
        }
    }
#endif

    NativeProvider() {
        if (!provider.Open(CSE_STEAM_PROVIDER_PATH)) {
            std::fprintf(stderr, "Unable to bind native Steam provider '%s': %s\n",
                CSE_STEAM_PROVIDER_PATH, provider.LastError());
            std::fflush(stderr);
            // Startup can fail during static callback construction. Do not re-enter partial global destructors.
            std::_Exit(EXIT_FAILURE);
        }
    }
};

#if defined(__APPLE__) && defined(SW_RENDERER)
struct SessionScope {
    explicit SessionScope(NativeProvider& owner) : calls(owner.sessionCalls) {
        ++calls;
    }
    ~SessionScope() {
        --calls;
    }
    unsigned int& calls;
};
#endif

NativeProvider& Owner() {
    // Retain until callback destruction, or explicit whole-software withdrawal under its real host anchor.
    static NativeProvider owner;
    return owner;
}

#if defined(__APPLE__)
void FinalizeServerTransport() {
    NativeProvider& owner = Owner();
#if defined(SW_RENDERER)
    if (owner.callbacksClosed && !owner.provider.NativeModule()) {
        return;
    }
#endif
    if (!owner.serverTransport.Close()) {
        std::fprintf(stderr, "Unable to retire native Steam server transport at module exit: %s\n",
            owner.serverTransport.LastError());
        std::abort();
    }
}
#endif

const cse_steam::Api& Api() {
    return Owner().provider.Functions();
}
} // namespace

#if defined(__APPLE__)
extern "C" __attribute__((visibility("hidden"))) bool CseSteamRetireServerTransport() {
    NativeProvider& owner = Owner();
    if (!owner.serverTransport.Close()) {
        std::fprintf(stderr, "Unable to retire native Steam transport before module unload: %s\n",
            owner.serverTransport.LastError());
        return false;
    }
    return true;
}
#endif

#if defined(__APPLE__) && defined(SW_RENDERER)
extern "C" __attribute__((visibility("hidden"))) void CseSteamCallResultDelivered(CCallbackBase* callback,
    SteamAPICall_t call) {
    auto& owner = Owner();
    std::lock_guard<std::recursive_mutex> lock(owner.callbacksMutex);
    // Steam already unregistered the completed call before delivering it to the consumer.
    owner.callResults.erase(std::make_pair(callback, call));
}

extern "C" __attribute__((visibility("hidden"))) bool CseSteamRetireModuleCallbacks() {
    auto& owner = Owner();
    std::lock_guard<std::recursive_mutex> lock(owner.callbacksMutex);
    if (!cse::software_lifetime::RetirementInProgress() || owner.callbackPumps != 0 || owner.sessionCalls != 0
        || owner.clientActive || owner.serverActive) {
        return false;
    }
    if (owner.callbacksClosed && !owner.provider.NativeModule()) {
        return true;
    }
    if (!owner.serverTransport.Close()) {
        return false;
    }
    owner.callbacksClosed = true;
    // Registration is now closed; retained global CCallback destructors become harmless no-ops.
    for (auto* callback : owner.callbacks) {
        owner.provider.Functions().unregisterCallback(callback);
    }
    owner.callbacks.clear();
    for (const auto& call : owner.callResults) {
        owner.provider.Functions().unregisterCallResult(call.first, call.second);
    }
    owner.callResults.clear();
    // Release the SDK reference now, while the executable's software anchor is held.
    // Deferring this edge to the DSO destructor can unload its own return address.
    return owner.provider.CloseChecked();
}
#endif

S_API void S_CALLTYPE CseSteamModuleInitialize() {
    (void)Api();
}

S_API bool S_CALLTYPE SteamAPI_Init() {
#if defined(__APPLE__) && defined(SW_RENDERER)
    auto& owner = Owner();
    std::lock_guard<std::recursive_mutex> lock(owner.callbacksMutex);
    if (owner.callbacksClosed || owner.sessionCalls == (std::numeric_limits<unsigned int>::max)()) {
        return false;
    }
    SessionScope session(owner);
    const bool initialized = Api().initialize();
    owner.clientActive = owner.clientActive || initialized;
    return initialized;
#else
    return Api().initialize();
#endif
}

S_API void S_CALLTYPE SteamAPI_Shutdown() {
#if defined(__APPLE__) && defined(SW_RENDERER)
    auto& nativeOwner = Owner();
    std::lock_guard<std::recursive_mutex> lock(nativeOwner.callbacksMutex);
    if (nativeOwner.callbacksClosed || nativeOwner.sessionCalls == (std::numeric_limits<unsigned int>::max)()) {
        return;
    }
    SessionScope session(nativeOwner);
#endif
#if defined(__APPLE__)
    NativeProvider& owner = Owner();
    // SteamAPI_Shutdown invalidates the core client used by the flat pipe API.
    // Balance our anonymous pipe while that client still exists, rather than
    // calling it later from the module/global destructor with a stale pointer.
    if (!owner.serverTransport.Close()) {
        std::fprintf(stderr, "Unable to retire native Steam transport before client shutdown: %s\n",
            owner.serverTransport.LastError());
        std::fflush(stderr);
        std::_Exit(EXIT_FAILURE);
    }
    owner.provider.Functions().shutdown();
#if defined(SW_RENDERER)
    owner.clientActive = false;
#endif
#else
    Api().shutdown();
#endif
}

S_API void S_CALLTYPE SteamAPI_RunCallbacks() {
#if defined(__APPLE__) && defined(SW_RENDERER)
    auto& owner = Owner();
    std::lock_guard<std::recursive_mutex> lock(owner.callbacksMutex);
    if (owner.callbacksClosed || owner.callbackPumps == (std::numeric_limits<unsigned int>::max)()) {
        return;
    }
    ++owner.callbackPumps;
    struct PumpScope {
        unsigned int& depth;
        ~PumpScope() {
            --depth;
        }
    } pump{owner.callbackPumps};
#endif
    Api().runCallbacks();
}

S_API HSteamUser S_CALLTYPE SteamAPI_GetHSteamUser() {
    return Api().getUser();
}

S_API HSteamPipe S_CALLTYPE SteamAPI_GetHSteamPipe() {
    return Api().getPipe();
}

S_API void* S_CALLTYPE SteamInternal_ContextInit(void* context) {
    return Api().initializeContext(context);
}

S_API void* S_CALLTYPE SteamInternal_FindOrCreateUserInterface(HSteamUser user, const char* version) {
    return Api().userInterface(user, version);
}

S_API void* S_CALLTYPE SteamInternal_FindOrCreateGameServerInterface(HSteamUser user, const char* version) {
    return Api().serverInterface(user, version);
}

S_API bool S_CALLTYPE SteamInternal_GameServer_Init(uint32 ip, uint16 legacySteamPort, uint16 gamePort,
    uint16 queryPort, EServerMode mode, const char* version) {
#if defined(__APPLE__) && defined(SW_RENDERER)
    auto& nativeOwner = Owner();
    std::lock_guard<std::recursive_mutex> lock(nativeOwner.callbacksMutex);
    if (nativeOwner.callbacksClosed || nativeOwner.sessionCalls == (std::numeric_limits<unsigned int>::max)()) {
        return false;
    }
    SessionScope session(nativeOwner);
#endif
    if (!Api().initializeServer(ip, legacySteamPort, gamePort, queryPort, mode, version)) {
        return false;
    }
#if defined(__APPLE__) && defined(SW_RENDERER)
    nativeOwner.serverActive = true;
#endif
#if defined(__APPLE__)
    // Keep the anonymous backend across map/session restarts. Register final
    // retirement after the SDK has registered its own globals, so our pipe is
    // closed while SteamClient is still alive. Darwin atexit binds this callback
    // to its owning image and runs it on dlclose as well as process exit.
    // No client login or unauthenticated game user is created.
    NativeProvider& owner = Owner();
    const bool alreadyRetained = owner.serverTransport.IsRetained();
    if (!owner.serverTransport.Retain(owner.provider.Functions(), owner.provider.NativeModule())) {
        std::fprintf(stderr, "Unable to retain native Steam server transport: %s\n",
            owner.serverTransport.LastError());
        Api().shutdownServer();
#if defined(SW_RENDERER)
        nativeOwner.serverActive = false;
#endif
        return false;
    }
    if (!alreadyRetained && std::atexit(FinalizeServerTransport) != 0) {
        std::fputs("Unable to register native Steam transport retirement\n", stderr);
        if (!owner.serverTransport.Close()) {
            std::abort();
        }
        Api().shutdownServer();
#if defined(SW_RENDERER)
        nativeOwner.serverActive = false;
#endif
        return false;
    }
#endif
    return true;
}

S_API void S_CALLTYPE SteamGameServer_Shutdown() {
#if defined(__APPLE__) && defined(SW_RENDERER)
    auto& nativeOwner = Owner();
    std::lock_guard<std::recursive_mutex> lock(nativeOwner.callbacksMutex);
    if (nativeOwner.callbacksClosed || nativeOwner.sessionCalls == (std::numeric_limits<unsigned int>::max)()) {
        return;
    }
    SessionScope session(nativeOwner);
#endif
    Api().shutdownServer();
#if defined(__APPLE__) && defined(SW_RENDERER)
    nativeOwner.serverActive = false;
#endif
}

S_API void S_CALLTYPE SteamGameServer_RunCallbacks() {
#if defined(__APPLE__) && defined(SW_RENDERER)
    auto& owner = Owner();
    std::lock_guard<std::recursive_mutex> lock(owner.callbacksMutex);
    if (owner.callbacksClosed || owner.callbackPumps == (std::numeric_limits<unsigned int>::max)()) {
        return;
    }
    ++owner.callbackPumps;
    struct PumpScope {
        unsigned int& depth;
        ~PumpScope() {
            --depth;
        }
    } pump{owner.callbackPumps};
#endif
    Api().runServerCallbacks();
}

S_API HSteamUser S_CALLTYPE SteamGameServer_GetHSteamUser() {
    return Api().getServerUser();
}

S_API HSteamPipe S_CALLTYPE SteamGameServer_GetHSteamPipe() {
    return Api().getServerPipe();
}

S_API void S_CALLTYPE SteamAPI_RegisterCallback(CCallbackBase* callback, int identifier) {
#if defined(__APPLE__) && defined(SW_RENDERER)
    auto& owner = Owner();
    std::lock_guard<std::recursive_mutex> lock(owner.callbacksMutex);
    if (owner.callbacksClosed) {
        return;
    }
    owner.callbacks.insert(callback);
#endif
    Api().registerCallback(callback, identifier);
}

S_API void S_CALLTYPE SteamAPI_UnregisterCallback(CCallbackBase* callback) {
#if defined(__APPLE__) && defined(SW_RENDERER)
    auto& owner = Owner();
    std::lock_guard<std::recursive_mutex> lock(owner.callbacksMutex);
    if (owner.callbacks.erase(callback) == 0) {
        return;
    }
#endif
    Api().unregisterCallback(callback);
}

S_API void S_CALLTYPE SteamAPI_RegisterCallResult(CCallbackBase* callback, SteamAPICall_t call) {
#if defined(__APPLE__) && defined(SW_RENDERER)
    auto& owner = Owner();
    std::lock_guard<std::recursive_mutex> lock(owner.callbacksMutex);
    if (owner.callbacksClosed) {
        return;
    }
    owner.callResults.insert(std::make_pair(callback, call));
#endif
    Api().registerCallResult(callback, call);
}

S_API void S_CALLTYPE SteamAPI_UnregisterCallResult(CCallbackBase* callback, SteamAPICall_t call) {
#if defined(__APPLE__) && defined(SW_RENDERER)
    auto& owner = Owner();
    std::lock_guard<std::recursive_mutex> lock(owner.callbacksMutex);
    if (owner.callResults.erase(std::make_pair(callback, call)) == 0) {
        return;
    }
#endif
    Api().unregisterCallResult(callback, call);
}

S_API void* S_CALLTYPE SteamInternal_CreateInterface(const char* version) {
    return Api().createInterface(version);
}

S_API void S_CALLTYPE SteamAPI_SetBreakpadAppID(uint32 application) {
    Api().setBreakpadAppId(application);
}

S_API void S_CALLTYPE SteamAPI_UseBreakpadCrashHandler(const char* version, const char* date, const char* time,
    bool fullMemoryDumps, void* context, PFNPreMinidumpCallback preMinidumpCallback) {
    Api().useBreakpadCrashHandler(version, date, time, fullMemoryDumps, context, preMinidumpCallback);
}

S_API void S_CALLTYPE SteamAPI_SetMiniDumpComment(const char* comment) {
    Api().setMiniDumpComment(comment);
}

S_API void S_CALLTYPE SteamAPI_WriteMiniDump(uint32 exceptionCode, void* exceptionInfo, uint32 build) {
    Api().writeMiniDump(exceptionCode, exceptionInfo, build);
}
#else
// The maintained Win32 reference continues to use its original Steam import library.
static_assert(sizeof(void*) != 0, "External Steam provider build");
#endif
