#include "model_fixture.inc"
#include "check.h"
#include "matchmaking_fixture.inc"
#include "detail_provider_fixture.h"
#include "TextEntry.h"
#include "PropertySheet.h"
#include "ServerBrowserDialog.h"
#include "ServerBrowserModule.h"
#include "../../../src/GameUI/common/IVGuiModule.h"
#include "../../../src/GameUI/GameUI_Interface.h"
#include "wrect.h"
#include "cl_dll.h"
#include "controls.h"
#include "../../../src/engine/controls.h"
#include "filesystem.h"
#include "vgui/IVGui.h"
#include "vgui/IScheme.h"
#include "vgui/ISystem.h"
#include "vgui/ISurface.h"
#include "vgui/IPanel.h"
#include "vgui/KeyValues.h"
#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_stdinc.h>
#include "geometry_fixture.h"
#include "layout_fixture.h"
#include <dlfcn.h>
#include <filesystem>
#include <fstream>
#include <exception>

class CCallbackMgr {
public:
    static void Register(CCallbackBase &callback, int identifier) {
        callback.m_nCallbackFlags |= CCallbackBase::k_ECallbackFlagsRegistered;
        callback.m_iCallback = identifier;
    }

    static void Unregister(CCallbackBase &callback) {
        callback.m_nCallbackFlags &= ~CCallbackBase::k_ECallbackFlagsRegistered;
    }
};

namespace {
CCallbackBase *favoritesCallback = nullptr;
constexpr size_t LargeListRows = 10000;
constexpr int MaximumTurns = 2000;
std::string preferenceRoot;
std::string currentGame = "cstrike";
std::string password = "previous";
std::vector<std::string> commands;
int preferenceCalls = 0;
bool mainThread = true;
bool safePoint = false;
ISteamMatchmakingServers *provider = nullptr;
Matchmaking matchmaking;
bool CSE_ABI_CALL MainThread() {
    return mainThread;
}
bool CSE_ABI_CALL SafePoint() {
    return safePoint;
}
cse::gameui_lifetime::HostV1 host{sizeof(cse::gameui_lifetime::HostV1), cse::gameui_lifetime::ContractVersion,
                                  MainThread, SafePoint};
void *HostFactory(const char *name, int *) {
    return std::strcmp(name, cse::gameui_lifetime::HostInterface) == 0 ? &host : nullptr;
}
const char *GameDirectory() {
    return currentGame.c_str();
}
char *GetPassword(char *name) {
    static char empty[] = "";
    return std::strcmp(name, "password") == 0 ? password.data() : empty;
}
void SetPassword(char *, char *value) {
    password = value;
}
int ClientCommand(char *value) {
    commands.emplace_back(value);
    return 1;
}
}
cl_enginefunc_t gEngfuncs{};
cl_enginefunc_t *engine = &gEngfuncs;
extern "C" void CseSteamModuleInitialize() {}
extern "C" HSteamUser SteamAPI_GetHSteamUser() {
    return 1;
}
extern "C" void *SteamInternal_FindOrCreateUserInterface(HSteamUser, const char *version) {
    if (std::strcmp(version, STEAMMATCHMAKINGSERVERS_INTERFACE_VERSION) == 0) {
        return provider;
    }
    return std::strcmp(version, STEAMMATCHMAKING_INTERFACE_VERSION) == 0 ? &matchmaking : nullptr;
}
extern "C" void S_CALLTYPE SteamAPI_RegisterCallback(CCallbackBase *callback, int identifier) {
    Require(!favoritesCallback && identifier == FavoritesListChanged_t::k_iCallback,
            "unexpected or duplicate Steam callback registration");
    CCallbackMgr::Register(*callback, identifier);
    favoritesCallback = callback;
}
extern "C" void S_CALLTYPE SteamAPI_UnregisterCallback(CCallbackBase *callback) {
    Require(callback == favoritesCallback, "unregistered an unknown Steam callback");
    CCallbackMgr::Unregister(*callback);
    favoritesCallback = nullptr;
}
extern "C" char *SDLCALL SDL_GetPrefPath(const char *, const char *) {
    Require(!preferenceRoot.empty(), "module attempted preferences before fixture scope admission");
    ++preferenceCalls;
    return SDL_strdup(preferenceRoot.c_str());
}
namespace cse::server_browser {
// The surrounding GameUI tree is tested independently with its extracted actual owner and real panels.
bool TryRetireGameUIPanels() {
    return TryRetireServerBrowser();
}
bool AreGameUIPanelsRetired() {
    return IsServerBrowserRetired();
}
}
#include "accepted_connection_fixture.h"

int main(int argc, char **argv) {
    if (argc != 6) {
        return 2;
    }
    return RunCase([&] {
        const std::string testCase = argv[5];
        const auto factory = ServerBrowserFactory();
        Require(factory("VGuiModuleServerBrowser002", nullptr) == nullptr, "factory admitted wrong module ABI");
        auto *module = static_cast<IVGuiModule *>(factory(ModuleInterface, nullptr));
        Require(module && !module->IsValid() && preferenceCalls == 0, "static factory eagerly created owners");
        const auto *retirement = cse::gameui_lifetime::AdmitRetirement(Sys_GetFactoryThis());
        Require(retirement && retirement->IsRetired(), "inert module retirement contract missing");
        CreateInterfaceFn hostOnly[]{HostFactory};
        --host.structBytes;
        Require(!module->Initialize(hostOnly, 1) && preferenceCalls == 0, "invalid host shape acquired owners");
        ++host.structBytes;
        mainThread = false;
        Require(!module->Initialize(hostOnly, 1), "wrong thread acquired module owners");
        mainThread = true;
        Require(!module->Initialize(hostOnly, 1) && preferenceCalls == 0, "missing engine functions acquired owners");
        void *filesystemImage = dlopen(argv[2], RTLD_NOW | RTLD_LOCAL);
        void *guiImage = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
        const char *error = dlerror();
        Require(guiImage && filesystemImage, error ? error : "actual dependency load failed");
        auto guiFactory = reinterpret_cast<CreateInterfaceFn>(dlsym(guiImage, "CreateInterface"));
        auto filesystemFactory = reinterpret_cast<CreateInterfaceFn>(dlsym(filesystemImage, "CreateInterface"));
        auto *filesystem = static_cast<IFileSystem *>(filesystemFactory("VFileSystem009", nullptr));
        filesystem->Mount();
        filesystem->AddSearchPath(argv[3], "PLATFORM");
        const bool hdLayout = testCase == "hd-layout";
        geometry_fixture::BaseUISurface geometry(
            static_cast<vgui2::ISurface *>(guiFactory("VGUI_Surface026", nullptr)));
        geometry_fixture::surface = hdLayout ? &geometry : nullptr;
        geometry.forceWindowFocus = hdLayout;
        CreateInterfaceFn factories[]{HostFactory, geometry_fixture::Factory, guiFactory, filesystemFactory};
        Require(vgui2::VGuiControls_Init("MODULE_BROWSER_TEST", factories, 4), "actual controls initialization failed");
        auto *gui = vgui2::ivgui();
        gui->SetSleep(false);
        gui->Start();
        auto *hdParent = hdLayout ? geometry_fixture::CreateParent(geometry) : nullptr;
        vgui2::scheme()->LoadSchemeFromFile("resource/TrackerScheme.res", nullptr);
        preferenceRoot = argv[4];
        std::filesystem::create_directories(preferenceRoot);
        gEngfuncs.pfnGetGameDirectory = GameDirectory;
        gEngfuncs.pfnGetCvarString = GetPassword;
        gEngfuncs.Cvar_Set = SetPassword;
        gEngfuncs.pfnClientCmd = ClientCommand;
        if (testCase == "unsupported-game") {
            currentGame = "unsupported";
            Require(!module->Initialize(factories, 4) && preferenceCalls == 0,
                    "unsupported current game acquired a Half-Life browser session");
        } else {
            if (testCase == "steam-list-changes") {
                const auto settingsPath = std::filesystem::path(preferenceRoot) / "server-browser/settings.cfg";
                std::filesystem::create_directories(settingsPath.parent_path());
                std::ofstream settings(settingsPath);
                settings << "master.selected=steam\n";
                Require(settings.good(), "could not configure the controlled Steam directory");
            }
            for (const char *game : {"cstrike", "valve"}) {
                currentGame = game;
                matchmaking.records.clear();
                DetailProvider detailProvider;
                // A failed assertion must retire the actual static module while its injected provider lives.
                struct FailureRetirement {
                    const cse::gameui_lifetime::RetirementV1 *retirement;
                    vgui2::IVGui *gui;
                    ~FailureRetirement() {
                        if (std::uncaught_exceptions() == 0) {
                            return;
                        }
                        safePoint = true;
                        for (int turn = 0; turn < MaximumTurns && !retirement->IsRetired(); ++turn) {
                            retirement->TryRetire();
                            gui->RunFrame();
                        }
                        safePoint = false;
                    }
                } failureRetirement{retirement, gui};
                auto &typedProvider = detailProvider.lists;
                provider = testCase == "unavailable" ? nullptr : &detailProvider;
                for (size_t index = 0; index < LargeListRows; ++index) {
                    auto row = Server(0x7f000001U + static_cast<uint32_t>(index));
                    row.m_bPassword = testCase == "deactivate";
                    row.m_nAppID = currentGame == "cstrike" ? 10 : 70;
                    std::strcpy(row.m_szGameDir, game);
                    typedProvider.nextRows.push_back(row);
                }
                typedProvider.cancelCallbacks = true;
                typedProvider.duringRequest = [&] {
                    safePoint = true;
                    Require(!retirement->TryRetire(), "provider reentry retired active browser stack");
                    safePoint = false;
                };
                Require(module->Initialize(factories, 4), "actual module initialization failed");
                Require(!module->Initialize(factories, 4), "duplicate initialization replaced a live session");
                Require(module->PostInitialize(factories, 4), "actual module post-initialization failed");
                if (testCase == "inactive-initialize") {
                    for (int turn = 0; turn < 8; ++turn) {
                        module->RunFrame();
                        gui->RunFrame();
                    }
                    Require(typedProvider.requests.empty(), "initialized module started a list before activation");
                }
                if (hdParent) {
                    module->SetParent(hdParent->GetVPanel());
                    Require(vgui2::ipanel()->GetParent(module->GetPanel()) == hdParent->GetVPanel(),
                            "actual IVGuiModule SetParent did not attach to HD Taskbar context");
                }
                Require(module->Activate(), "actual module activation failed");
                module->RunFrame();
                if (provider) {
                    typedProvider.Complete();
                }
                constexpr int PopulationTurns = 500;
                for (int turn = 0; turn < PopulationTurns; ++turn) {
                    module->RunFrame();
                    gui->RunFrame();
                    vgui2::surface()->SolveTraverse(module->GetPanel(), false);
                }
                if (testCase == "steam-list-changes") {
                    Require(favoritesCallback && favoritesCallback->GetCallbackSizeBytes() == sizeof(FavoritesListChanged_t),
                            "Steam did not receive a typed FavoritesListChanged callback");
                    auto *dialog = dynamic_cast<ServerBrowserDialog *>(
                        vgui2::ipanel()->GetPanel(module->GetPanel(), "MODULE_BROWSER_TEST"));
                    auto *tabs = static_cast<vgui2::PropertySheet *>(dialog->FindChildByName("GameTabs", false));
                    const auto selectTab = [&](int index) {
                        tabs->SetActivePage(tabs->GetPage(index));
                        for (int turn = 0; turn < PopulationTurns; ++turn) {
                            module->RunFrame();
                            gui->RunFrame();
                        }
                        typedProvider.Complete();
                    };
                    constexpr int FavoritesTab = 1;
                    constexpr int HistoryTab = 2;
                    const AppId_t app = typedProvider.lastApp;
                    selectTab(FavoritesTab);
                    FavoritesListChanged_t event{};
                    event.m_nIP = 0x7f000001U;
                    event.m_nAppID = app;
                    event.m_nFlags = k_unFavoriteFlagFavorite;
                    const auto lastFavoritesRequest = typedProvider.serial;
                    favoritesCallback->Run(&event);
                    favoritesCallback->Run(&event);
                    Require(typedProvider.serial == lastFavoritesRequest, "Steam callback began a request reentrantly");
                    module->RunFrame();
                    for (int turn = 0; turn < PopulationTurns; ++turn) {
                        module->RunFrame();
                    }
                    Require(typedProvider.serial == lastFavoritesRequest + 1 &&
                                typedProvider.lastKind == ListKind::Favorites,
                            "Favorites changes did not coalesce into one refresh");
                    typedProvider.Complete();
                    selectTab(HistoryTab);
                    const auto lastHistoryRequest = typedProvider.serial;
                    event.m_nAppID = app + 1;
                    event.m_nFlags = k_unFavoriteFlagHistory;
                    favoritesCallback->Run(&event);
                    module->RunFrame();
                    Require(typedProvider.serial == lastHistoryRequest, "other app's History change caused a refresh");
                    event.m_nIP = 0;
                    event.m_nAppID = 0;
                    favoritesCallback->Run(&event);
                    for (int turn = 0; turn < PopulationTurns; ++turn) {
                        module->RunFrame();
                    }
                    Require(typedProvider.serial == lastHistoryRequest + 1 && typedProvider.lastKind == ListKind::History,
                            "Steam's full-list notification did not update History");
                }
                if (hdLayout) {
                    auto *dialog = dynamic_cast<ServerBrowserDialog *>(
                        vgui2::ipanel()->GetPanel(module->GetPanel(), "MODULE_BROWSER_TEST"));
                    Require(dialog != nullptr, "actual module browser panel missing after attachment");
                    auto *label = static_cast<vgui2::Label *>(layout_fixture::Child(*dialog, "NameLabel"));
                    Require(label->IsHDProportional() && vgui2::surface()->GetFontTall(label->GetFont()) >= 24,
                            "module fixture omitted real HD scheme context");
                    layout_fixture::Check(*dialog, 1600, 1200);
                }
                if (testCase == "deactivate") {
                    auto *dialog = dynamic_cast<ServerBrowserDialog *>(
                        vgui2::ipanel()->GetPanel(module->GetPanel(), "MODULE_BROWSER_TEST"));
                    dialog->SelectVisible(0);
                    auto *detailsCommand = new KeyValues("Command", "command", "Details");
                    dialog->OnMessage(detailsCommand, 0);
                    detailsCommand->deleteThis();
                    for (size_t index = 0; index < LargeListRows; ++index) {
                        detailProvider.rules->RulesResponded("pending", "value");
                    }
                    dialog->ConnectSelection();
                    auto *prompt = dialog->FindChildByName("DialogServerPassword", true);
                    auto *entry = dynamic_cast<vgui2::TextEntry *>(prompt->FindChildByName("PasswordEntry", false));
                    entry->SetText("unsent");
                    entry->SaveUndoState();
                    module->Deactivate();
                    Require(entry->GetTextLength() == 0, "module deactivation retained unsent password");
                    entry->Undo();
                    Require(entry->GetTextLength() == 0, "module deactivation retained secret undo storage");
                    gui->PostMessage(prompt->GetVPanel(), new KeyValues("Command", "command", "Connect"), 0, 0);
                    gui->PostMessage(dialog->GetVPanel(), new KeyValues("Command", "command", "Favorite"), 0, 0);
                    module->Activate();
                    Require(!dialog->IsVisible(), "module reopened before child retirement");
                    int turns = 0;
                    while (!dialog->IsVisible() && turns < MaximumTurns) {
                        module->RunFrame();
                        gui->RunFrame();
                        ++turns;
                    }
                    Require(dialog->IsVisible() && turns >= static_cast<int>(LargeListRows / DetailFrameBudget),
                            "hidden retirement did not preserve bounded progress and eventual reopen");
                    Require(detailProvider.canceled == 2 && typedProvider.canceled == 1 && commands.empty() &&
                                matchmaking.records.empty(),
                            "deactivated queries/actions were not retired exactly once");
                    module->RunFrame();
                    typedProvider.Complete();
                    for (int turn = 0; turn < PopulationTurns; ++turn) {
                        module->RunFrame();
                        gui->RunFrame();
                    }
                    // Issue a connection after reopening, then hide while its history/cvar ownership remains pending.
                    dialog->SelectVisible(0);
                    dialog->ConnectSelection();
                    prompt = dialog->FindChildByName("DialogServerPassword", true);
                    entry = dynamic_cast<vgui2::TextEntry *>(prompt->FindChildByName("PasswordEntry", false));
                    entry->SetText("issued");
                    gui->PostMessage(prompt->GetVPanel(), new KeyValues("Command", "command", "Connect"), 0, 0);
                    gui->RunFrame();
                    module->Deactivate();
                    module->RunFrame();
                    Require(password == "issued" && matchmaking.records.empty(),
                            "hide retired an issued connection prematurely");
                    accepted_connection_fixture::CGameUI hook;
                    hook.ConnectToServer(game, 0x0100007f, 27015);
                    Require(matchmaking.records.size() == 1 && password == "previous",
                            "inactive accepted connection lost secret/history ownership");
                    hook.DisconnectFromServer();
                    commands.clear();
                }
                if (provider && testCase != "deactivate" && testCase != "steam-list-changes") {
                    auto *dialog = dynamic_cast<ServerBrowserDialog *>(
                        vgui2::ipanel()->GetPanel(module->GetPanel(), "MODULE_BROWSER_TEST"));
                    Require(dialog != nullptr, "actual module browser panel absent");
                    dialog->SelectVisible(0);
                    dialog->ConnectSelection();
                    Require(matchmaking.records.empty(), "connection attempt wrote history before acceptance");
                    gui->PostMessage(module->GetPanel(), new KeyValues("ConnectedToGame", "ip", 0x0100007f), 0, 0);
                    gui->RunFrame();
                    Require(matchmaking.records.empty(), "delayed initial module replay wrote history");
                    accepted_connection_fixture::CGameUI actualHook;
                    actualHook.ConnectToServer(game, 0x0100007f, 27015);
                    Require(matchmaking.records.size() == 1 && matchmaking.records.front().address.queryPort == 27016 &&
                                matchmaking.records.front().app == (currentGame == "cstrike" ? 10U : 70U) &&
                                matchmaking.records.front().lastPlayed > 0,
                            "actual accepted owner/module route lost current game, timestamp or distinct query port");
                    actualHook.ConnectToServer(game, 0x0100007f, 27015);
                    Require(matchmaking.records.size() == 1, "replayed accepted hook duplicated history");
                    actualHook.DisconnectFromServer();
                    Require(password == "previous", "actual disconnect route retained owned password");
                }
                Require(!retirement->TryRetire() && module->GetPanel(), "unsafe host point released active module");
                safePoint = true;
                int turns = 0;
                while (!retirement->TryRetire() && turns < MaximumTurns) {
                    ++turns;
                    gui->RunFrame();
                }
                Require(retirement->IsRetired() && !module->GetPanel(), "actual module failed complete retirement");
                Require(!favoritesCallback, "module retirement left a registered Steam callback");
                if (provider) {
                    Require(turns > static_cast<int>(LargeListRows / BrowserFrameBudget),
                            "module destroyed populated model without bounded storage retirement");
                    Require(typedProvider.canceled == static_cast<int>(typedProvider.serial) &&
                                typedProvider.released == static_cast<int>(typedProvider.serial) &&
                                typedProvider.requests.empty(),
                            "module provider handle retirement was not once-only");
                }
                module->Shutdown();
                Require(retirement->IsRetired(), "repeated shutdown recreated an owner");
                safePoint = false;
            }
        }
        if (hdParent) {
            geometry.Root(0);
            delete geometry_fixture::embeddedRoot;
        }
        gui->Shutdown();
        vgui2::system()->Shutdown();
        vgui2::scheme()->Shutdown(true);
        vgui2::surface()->Shutdown();
        vgui2::VGuiControls_Shutdown();
        filesystem->Unmount();
        dlclose(guiImage);
        dlclose(filesystemImage);
    });
}
