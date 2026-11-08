#ifndef CSE_STEAM_PROVIDER_H
#define CSE_STEAM_PROVIDER_H

#include "native_module.h"
#include "../external/steamworks/steam_gameserver.h"

namespace cse_steam {
// Native SDK types remain within the selected platform/compiler ABI. Network data is unchanged.
struct Api {
    bool (S_CALLTYPE* initialize)();
    decltype(&SteamInternal_CreateInterface) createInterface;
    void (S_CALLTYPE* setBreakpadAppId)(uint32);
    void (S_CALLTYPE* useBreakpadCrashHandler)(const char*, const char*, const char*, bool, void*, PFNPreMinidumpCallback);
    void (S_CALLTYPE* setMiniDumpComment)(const char*);
    void (S_CALLTYPE* writeMiniDump)(uint32, void*, uint32);
    void (S_CALLTYPE* shutdown)();
    decltype(&SteamAPI_RunCallbacks) runCallbacks;
    decltype(&SteamAPI_GetHSteamUser) getUser;
    decltype(&SteamAPI_GetHSteamPipe) getPipe;
    decltype(&SteamInternal_ContextInit) initializeContext;
    decltype(&SteamInternal_FindOrCreateUserInterface) userInterface;
    decltype(&SteamInternal_FindOrCreateGameServerInterface) serverInterface;
    decltype(&SteamInternal_GameServer_Init) initializeServer;
    decltype(&SteamGameServer_Shutdown) shutdownServer;
    decltype(&SteamGameServer_RunCallbacks) runServerCallbacks;
    decltype(&SteamGameServer_GetHSteamUser) getServerUser;
    decltype(&SteamGameServer_GetHSteamPipe) getServerPipe;
    decltype(&SteamAPI_RegisterCallback) registerCallback;
    decltype(&SteamAPI_UnregisterCallback) unregisterCallback;
    decltype(&SteamAPI_RegisterCallResult) registerCallResult;
    decltype(&SteamAPI_UnregisterCallResult) unregisterCallResult;
};


namespace provider_detail {
template <typename Function> bool Bind(void* module, const char* name, Function& function, const char*& missing) {
    function = reinterpret_cast<Function>(cse_platform::GetSymbol(module, name));
    if (!function) {
        missing = name;
        return false;
    }
    return true;
}
} // namespace provider_detail

inline bool BindApi(void* module, Api& api, const char*& missing) {
    // Resolve everything before exposing even the first callable entry point.
    api = {};
    missing = nullptr;
    Api candidate{};
    if (!provider_detail::Bind(module, "SteamAPI_Init", candidate.initialize, missing)
        || !provider_detail::Bind(module, "SteamAPI_Shutdown", candidate.shutdown, missing)
        || !provider_detail::Bind(module, "SteamAPI_RunCallbacks", candidate.runCallbacks, missing)
        || !provider_detail::Bind(module, "SteamAPI_GetHSteamUser", candidate.getUser, missing)
        || !provider_detail::Bind(module, "SteamAPI_GetHSteamPipe", candidate.getPipe, missing)
        || !provider_detail::Bind(module, "SteamInternal_ContextInit", candidate.initializeContext, missing)
        || !provider_detail::Bind(module, "SteamInternal_FindOrCreateUserInterface", candidate.userInterface, missing)
        || !provider_detail::Bind(module, "SteamInternal_FindOrCreateGameServerInterface", candidate.serverInterface, missing)
        || !provider_detail::Bind(module, "SteamInternal_GameServer_Init", candidate.initializeServer, missing)
        || !provider_detail::Bind(module, "SteamGameServer_Shutdown", candidate.shutdownServer, missing)
        || !provider_detail::Bind(module, "SteamGameServer_RunCallbacks", candidate.runServerCallbacks, missing)
        || !provider_detail::Bind(module, "SteamGameServer_GetHSteamUser", candidate.getServerUser, missing)
        || !provider_detail::Bind(module, "SteamGameServer_GetHSteamPipe", candidate.getServerPipe, missing)
        || !provider_detail::Bind(module, "SteamAPI_RegisterCallback", candidate.registerCallback, missing)
        || !provider_detail::Bind(module, "SteamAPI_UnregisterCallback", candidate.unregisterCallback, missing)
        || !provider_detail::Bind(module, "SteamAPI_RegisterCallResult", candidate.registerCallResult, missing)
        || !provider_detail::Bind(module, "SteamAPI_UnregisterCallResult", candidate.unregisterCallResult, missing)
        || !provider_detail::Bind(module, "SteamInternal_CreateInterface", candidate.createInterface, missing)
        || !provider_detail::Bind(module, "SteamAPI_SetBreakpadAppID", candidate.setBreakpadAppId, missing)
        || !provider_detail::Bind(module, "SteamAPI_UseBreakpadCrashHandler", candidate.useBreakpadCrashHandler, missing)
        || !provider_detail::Bind(module, "SteamAPI_SetMiniDumpComment", candidate.setMiniDumpComment, missing)
        || !provider_detail::Bind(module, "SteamAPI_WriteMiniDump", candidate.writeMiniDump, missing)) {
        return false;
    }
    api = candidate;
    return true;
}

class Provider final {
public:
    Provider() = default;
    Provider(const Provider&) = delete;
    Provider& operator=(const Provider&) = delete;

    ~Provider() {
        Close();
    }

    bool Open(const char* path) {
        if (module_) {
            error_ = "Steam provider is already open";
            return false;
        }
        void* candidate = cse_platform::LoadModule(path, cse_platform::ModuleFallback::None);
        if (!candidate) {
            error_ = "Unable to load native Steam provider";
            return false;
        }
        if (!BindApi(candidate, api_, error_)) {
            cse_platform::UnloadModule(candidate);
            return false;
        }
        module_ = candidate;
        return true;
    }

    // The caller must shut down sessions and unregister callbacks before closing.
    // All borrowed SDK interfaces and table entries expire at this boundary.
    void Close() {
        api_ = {};
        cse_platform::UnloadModule(module_);
        module_ = nullptr;
    }

    const Api& Functions() const {
        return api_;
    }

    const char* LastError() const {
        return error_;
    }

private:
    void* module_ = nullptr;
    Api api_{};
    const char* error_ = nullptr;
};
} // namespace cse_steam
#endif
