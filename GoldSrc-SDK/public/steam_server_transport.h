#ifndef CSE_STEAM_SERVER_TRANSPORT_H
#define CSE_STEAM_SERVER_TRANSPORT_H

#include "steam_provider.h"

namespace cse_steam {
// A communication pipe keeps the SDK transport alive without creating or logging in a user.
// Retain/Close run on the Steam-owning thread with other application SDK calls quiescent.
class ServerTransport {
public:
    ServerTransport() = default;
    ServerTransport(const ServerTransport&) = delete;
    ServerTransport& operator=(const ServerTransport&) = delete;

    ~ServerTransport() {
        Close();
    }

    bool Retain(const Api& api, void* module) {
        if (pipe_) {
            return true;
        }
        Functions candidate{};
        error_ = nullptr;
        if (!provider_detail::Bind(module, "SteamAPI_ISteamClient_CreateSteamPipe", candidate.create, error_)
            || !provider_detail::Bind(module, "SteamAPI_ISteamClient_BReleaseSteamPipe", candidate.release, error_)
            || !provider_detail::Bind(module, "SteamAPI_ISteamClient_BShutdownIfAllPipesClosed", candidate.shutdown,
                error_)) {
            return false;
        }
        if (!api.createInterface) {
            error_ = "SteamInternal_CreateInterface";
            return false;
        }
        void* client = api.createInterface("SteamClient020");
        if (!client) {
            error_ = "SteamClient020";
            return false;
        }
        const HSteamPipe pipe = candidate.create(client);
        if (!pipe) {
            error_ = "SteamAPI_ISteamClient_CreateSteamPipe";
            return false;
        }
        functions_ = candidate;
        client_ = client;
        pipe_ = pipe;
        return true;
    }

    bool Close() {
        if (!pipe_) {
            return true;
        }
        if (!functions_.release(client_, pipe_)) {
            error_ = "SteamAPI_ISteamClient_BReleaseSteamPipe";
            return false;
        }
        pipe_ = 0;
        // Other consumer modules may still own pipes; false is then the documented result.
        functions_.shutdown(client_);
        client_ = nullptr;
        functions_ = {};
        error_ = nullptr;
        return true;
    }

    const char* LastError() const {
        return error_;
    }

private:
    // Signatures from pinned Valve steam_api_flat.h; these are C functions, not member pointers.
    struct Functions {
        HSteamPipe (S_CALLTYPE* create)(void*) = nullptr;
        bool (S_CALLTYPE* release)(void*, HSteamPipe) = nullptr;
        bool (S_CALLTYPE* shutdown)(void*) = nullptr;
    };
    Functions functions_{};
    void* client_ = nullptr;
    HSteamPipe pipe_ = 0;
    const char* error_ = nullptr;
};
} // namespace cse_steam
#endif
