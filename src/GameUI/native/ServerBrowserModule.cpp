#include "ServerBrowserModule.h"
#include "ServerBrowserDialog.h"
#include "../../external/steamworks/isteamapps.h"
#include "../../public/native_gameui_connection.h"
#include "../GameUI_Interface.h"
#include "../VGuiSystemModuleLoader.h"
#include "../common/IVGuiModule.h"
#include "vgui_controls/Controls.h"
#include "vgui/IVGui.h"
#include "wrect.h"
#include "cl_dll.h"
#include <ctime>
#include <cstring>

namespace cse::server_browser {
namespace {
const gameui_lifetime::HostV1 *host = nullptr;
const gameui_connection::HostV1 *connectionHost = nullptr;
class ConnectionEngine final : public IConnectionEngine {
public:
    const char *Password() override {
        char name[] = "password";
        return engine->pfnGetCvarString(name);
    }
    void SetPassword(const char *value) override {
        char name[] = "password";
        engine->Cvar_Set(name, const_cast<char *>(value));
    }
    void Connect(const char *numericCommand) override {
        engine->pfnClientCmd(const_cast<char *>(numericCommand));
    }
};
class ServerBrowserModule final : public IVGuiModule {
public:
    bool Initialize(CreateInterfaceFn *factories, int count) override {
        if (!BindServerBrowserHost(factories, count) || !host->IsMainThread() || session_ || dialog_.Get() || !engine ||
            !engine->pfnGetGameDirectory || !engine->pfnGetCvarString || !engine->Cvar_Set || !engine->pfnClientCmd) {
            return false;
        }
        const char *game = engine->pfnGetGameDirectory();
        if (!game || (std::strcmp(game, "cstrike") != 0 && std::strcmp(game, "valve") != 0)) {
            return false;
        }
        constexpr AppId_t CounterStrikeApp = 10;
        constexpr AppId_t HalfLifeApp = 70;
        const AppId_t app = std::strcmp(game, "cstrike") == 0 ? CounterStrikeApp : HalfLifeApp;
        const HSteamUser user = SteamAPI_GetHSteamUser();
        auto *servers = static_cast<ISteamMatchmakingServers *>(
            SteamInternal_FindOrCreateUserInterface(user, STEAMMATCHMAKINGSERVERS_INTERFACE_VERSION));
        auto *favorites = static_cast<ISteamMatchmaking *>(
            SteamInternal_FindOrCreateUserInterface(user, STEAMMATCHMAKING_INTERFACE_VERSION));
        session_.reset(new BrowserSession(servers, favorites, vgui2::filesystem(), connectionEngine_, app, game));
        session_->apps = static_cast<ISteamApps *>(
            SteamInternal_FindOrCreateUserInterface(user, STEAMAPPS_INTERFACE_VERSION));
        if (accepted_) {
            session_->connection.Accepted(acceptedGame_.c_str(), acceptedIP_, acceptedPort_, acceptedTime_);
        }
        dialog_ = new ServerBrowserDialog(*session_);
        retiring_ = false;
        initialized_ = true;
        return true;
    }
    bool PostInitialize(CreateInterfaceFn *, int) override {
        return initialized_ && dialog_.Get() && !retiring_;
    }
    bool Activate() override {
        if (!IsValid()) {
            return false;
        }
        BrowserUse use(*session_);
        dialog_->Activate();
        return true;
    }
    bool IsValid() override {
        return initialized_ && session_ && !retiring_ && dialog_.Get();
    }
    void Deactivate() override {
        if (session_ && dialog_.Get()) {
            BrowserUse use(*session_);
            dialog_->Deactivate();
        }
    }
    void Reactivate() override {
        Activate();
    }
    void Shutdown() override {
        TryRetire();
    }
    vgui2::VPANEL GetPanel() override {
        return dialog_.Get() ? dialog_->GetVPanel() : 0;
    }
    void SetParent(vgui2::VPANEL parent) override {
        if (dialog_.Get() && !retiring_) {
            dialog_->SetParent(parent);
        }
    }
    void RunFrame() override {
        if (IsValid()) {
            dialog_->Pump();
        }
    }
    bool TryRetire() {
        if (!session_) {
            return !dialog_.Get();
        }
        if (!host || !host->IsMainThread() || !host->IsRetirementSafePoint() || session_->useDepth || busy_) {
            return false;
        }
        busy_ = true;
        struct Guard {
            bool &busy;
            ~Guard() {
                busy = false;
            }
        } guard{busy_};
        if (!retiring_) {
            retiring_ = true;
            initialized_ = false;
            session_->closing = true;
            session_->connection.Disconnected();
            if (dialog_.Get()) {
                dialog_->SaveSettings();
            }
        }
        const bool modelClosed = session_->model.CloseStep(BrowserFrameBudget);
        const bool masterserversClosed = session_->masterserverProvider.BeginClose();
        const bool viewClosed = session_->presentation.BeginClose();
        session_->presentation.Pump(BrowserFrameBudget);
        const bool childrenClosed = !dialog_.Get() || dialog_->BeginClose();
        if (!modelClosed || !masterserversClosed || !viewClosed || !childrenClosed) {
            return false;
        }
        if (dialog_.Get()) {
            dialog_->MarkForDeletion();
            return false;
        }
        dialog_ = nullptr;
        session_.reset();
        return true;
    }
    bool IsRetired() const {
        // DHANDLE resolves against VGUI's real serial-number registry, not a queued-delete flag.
        return !session_;
    }
    void Accepted(const char *game, uint32_t rawIP, uint16_t port) {
        const std::time_t now = std::time(nullptr);
        accepted_ = game && *game && rawIP && port && now > 0 && static_cast<uint64_t>(now) <= UINT32_MAX;
        acceptedGame_ = accepted_ ? game : "";
        acceptedIP_ = rawIP;
        acceptedPort_ = port;
        acceptedTime_ = accepted_ ? static_cast<uint32_t>(now) : 0;
        if (session_ && !retiring_) {
            BrowserUse use(*session_);
            if (accepted_) {
                session_->connection.Accepted(acceptedGame_.c_str(), rawIP, port, acceptedTime_);
            } else {
                session_->connection.Disconnected();
            }
        }
    }
    void Disconnected() {
        accepted_ = false;
        acceptedGame_.clear();
        if (session_) {
            BrowserUse use(*session_);
            session_->connection.Disconnected(connectionHost ? connectionHost->PreparingAddress() : nullptr);
        }
    }

private:
    // Inert until Initialize: no model, preferences, Steam interface or VGUI panel in static construction.
    ConnectionEngine connectionEngine_;
    std::unique_ptr<BrowserSession> session_;
    vgui2::DHANDLE<ServerBrowserDialog> dialog_;
    bool accepted_ = false;
    std::string acceptedGame_;
    uint32_t acceptedIP_ = 0;
    uint16_t acceptedPort_ = 0;
    uint32_t acceptedTime_ = 0;
    bool initialized_ = false;
    bool retiring_ = false;
    bool busy_ = false;
};
ServerBrowserModule module;
void *BrowserFactory(const char *name, int *code) {
    const bool matches = name && std::strcmp(name, ModuleInterface) == 0;
    if (code) {
        *code = matches ? IFACE_OK : IFACE_FAILED;
    }
    return matches ? &module : nullptr;
}
bool CSE_ABI_CALL TryRetireGameUI() {
    if (!host || !host->IsMainThread() || !host->IsRetirementSafePoint()) {
        return AreGameUIPanelsRetired();
    }
    return TryRetireGameUIPanels();
}
bool CSE_ABI_CALL IsGameUIRetired() {
    return AreGameUIPanelsRetired();
}
const gameui_lifetime::RetirementV1 retirement{sizeof(gameui_lifetime::RetirementV1), gameui_lifetime::ContractVersion,
                                               TryRetireGameUI, IsGameUIRetired};
void *RetirementFactory() {
    return const_cast<gameui_lifetime::RetirementV1 *>(&retirement);
}
}
#if INTPTR_MAX > INT32_MAX
EXPOSE_INTERFACE_FN(RetirementFactory, CSEGameUIRetirement, gameui_lifetime::RetirementInterface);
#endif
CreateInterfaceFn ServerBrowserFactory() {
    return BrowserFactory;
}
bool BindServerBrowserHost(CreateInterfaceFn *factories, int count) {
    for (int index = 0; factories && index < count; ++index) {
        if (const auto *candidate = gameui_lifetime::AdmitHost(factories[index])) {
            if (!candidate->IsMainThread() || (host && candidate != host)) {
                return false;
            }
            host = candidate;
            connectionHost = gameui_connection::AdmitHost(factories[index]);
            return true;
        }
    }
    return false;
}
bool TryRetireServerBrowser() {
    return module.TryRetire();
}
bool IsServerBrowserRetired() {
    return module.IsRetired();
}
void ServerBrowserAcceptedConnection(const char *game, uint32_t rawIP, uint16_t connectionPort) {
    module.Accepted(game, rawIP, connectionPort);
}
void ServerBrowserDisconnected() {
    module.Disconnected();
}
}
