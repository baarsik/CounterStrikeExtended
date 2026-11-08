#if defined(CSE_OWNED_STEAM_PROVIDER)
#include "../public/steam_service.h"
#include "../public/steam_provider.h"
#if defined(__APPLE__)
#include "../public/steam_server_transport.h"
#endif

#include <cstdio>
#include <cstdlib>

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
#if defined(__APPLE__)
    cse_steam::ServerTransport serverTransport;

    ~NativeProvider() {
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

NativeProvider& Owner() {
    // Each consuming module keeps its provider reference until its own callbacks have been destroyed.
    static NativeProvider owner;
    return owner;
}

const cse_steam::Api& Api() {
    return Owner().provider.Functions();
}
} // namespace

S_API void S_CALLTYPE CseSteamModuleInitialize() {
    (void)Api();
}

S_API bool S_CALLTYPE SteamAPI_Init() {
    return Api().initialize();
}

S_API void S_CALLTYPE SteamAPI_Shutdown() {
    Api().shutdown();
}

S_API void S_CALLTYPE SteamAPI_RunCallbacks() {
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
    if (!Api().initializeServer(ip, legacySteamPort, gamePort, queryPort, mode, version)) {
        return false;
    }
#if defined(__APPLE__)
    // Keep the anonymous backend for this module's lifetime, across server map/session restarts.
    // No client login or unauthenticated game user is created.
    NativeProvider& owner = Owner();
    if (!owner.serverTransport.Retain(owner.provider.Functions(), owner.provider.NativeModule())) {
        std::fprintf(stderr, "Unable to retain native Steam server transport: %s\n",
            owner.serverTransport.LastError());
        Api().shutdownServer();
        return false;
    }
#endif
    return true;
}

S_API void S_CALLTYPE SteamGameServer_Shutdown() {
    Api().shutdownServer();
}

S_API void S_CALLTYPE SteamGameServer_RunCallbacks() {
    Api().runServerCallbacks();
}

S_API HSteamUser S_CALLTYPE SteamGameServer_GetHSteamUser() {
    return Api().getServerUser();
}

S_API HSteamPipe S_CALLTYPE SteamGameServer_GetHSteamPipe() {
    return Api().getServerPipe();
}

S_API void S_CALLTYPE SteamAPI_RegisterCallback(CCallbackBase* callback, int identifier) {
    Api().registerCallback(callback, identifier);
}

S_API void S_CALLTYPE SteamAPI_UnregisterCallback(CCallbackBase* callback) {
    Api().unregisterCallback(callback);
}

S_API void S_CALLTYPE SteamAPI_RegisterCallResult(CCallbackBase* callback, SteamAPICall_t call) {
    Api().registerCallResult(callback, call);
}

S_API void S_CALLTYPE SteamAPI_UnregisterCallResult(CCallbackBase* callback, SteamAPICall_t call) {
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
