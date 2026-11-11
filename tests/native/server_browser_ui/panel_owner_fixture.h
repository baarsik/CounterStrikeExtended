// Actual owner statements with provider/module leaf seams and actual VGUI panel dispatch.
namespace panel_owner_fixture {
int destructions = 0;
int demoStops = 0;
bool dependenciesAlive = true;
bool browserReady = true;
class OwnedPanel : public vgui2::Panel {
public:
    OwnedPanel(vgui2::Panel *parent, const char *name) : Panel(parent, name) {}
    ~OwnedPanel() override {
        Require(dependenciesAlive && vgui2::surface() && vgui2::filesystem(),
                "panel destructor ran after a required service retired");
        ++destructions;
    }
    void ShutdownDemoPlayer() {
        ++demoStops;
    }
};
using CBasePanel = OwnedPanel;
using CGameConsoleDialog = OwnedPanel;
class CGameUI {
public:
    void Shutdown();
};
class CGameConsole {
public:
    bool TryRetireNativePanel();
    bool IsNativePanelRetired() const;
    bool m_bInitialized = false;
    CGameConsoleDialog *m_pConsole = nullptr;
} g_GameConsole;
struct Loader {
    bool TryRetireNativeModules() {
        return browserReady;
    }
    void ShutdownPlatformModules() {
        ++shutdowns;
    }
    void UnloadPlatformModules() {
        ++unloads;
    }
    int shutdowns = 0;
    int unloads = 0;
} g_VModuleLoader;
struct Info {
    void FreeModInfo() {}
} info;
Info *ModInfo() {
    return &info;
}
OwnedPanel *staticPanel = nullptr;
OwnedPanel *g_pTaskbar = nullptr;
void *g_pSystemWrapper = nullptr;
namespace cse::server_browser {
bool TryRetireServerBrowser() {
    return browserReady;
}
bool IsServerBrowserRetired() {
    return browserReady;
}
bool TryRetireGameUIPanels();
bool AreGameUIPanelsRetired();
}
#include "panel_state.inc"
#include "gameui_panels.inc"
#define CSE_NATIVE_SYSTEM_MODULE 1
#include "gameui_shutdown.inc"
#undef CSE_NATIVE_SYSTEM_MODULE
void Run(vgui2::IVGui *gui) {
    for (const bool withConsole : {true, false, true}) {
        nativeDemoStopped = false;
        nativeGameUIShutdownComplete = false;
        const int priorDestructions = destructions;
        const int priorStops = demoStops;
        const int priorUnloads = g_VModuleLoader.unloads;
        auto *engineRoot = new OwnedPanel(nullptr, "Engine-owned parent");
        staticPanel = new OwnedPanel(engineRoot, "GameUI root");
        g_pTaskbar = new OwnedPanel(staticPanel, "Taskbar");
        vgui2::DHANDLE<OwnedPanel> root;
        root = staticPanel;
        vgui2::DHANDLE<OwnedPanel> console;
        if (withConsole) {
            g_GameConsole.m_pConsole = new OwnedPanel(g_pTaskbar, "Console");
            g_GameConsole.m_pConsole->SetParent(engineRoot);
            g_GameConsole.m_bInitialized = true;
            console = g_GameConsole.m_pConsole;
        }
        CGameUI owner;
        browserReady = false;
        Require(!cse::server_browser::TryRetireGameUIPanels(), "provider refusal discarded panels");
        owner.Shutdown();
        Require(root.Get() && g_VModuleLoader.unloads == priorUnloads, "refusal unloaded live widget code");
        browserReady = true;
        Require(!cse::server_browser::TryRetireGameUIPanels(), "queued deletion claimed immediate completion");
        Require(!cse::server_browser::TryRetireGameUIPanels(), "retry without dispatch claimed completion");
        Require(demoStops == priorStops + 1 && root.Get(), "retry repeated demo shutdown or lost root");
        gui->RunFrame();
        if (withConsole) {
            Require(!console.Get() && root.Get(), "separate reparented console owner did not retire first");
            Require(!cse::server_browser::TryRetireGameUIPanels(), "root queue was mistaken for deletion");
            gui->RunFrame();
        }
        Require(cse::server_browser::TryRetireGameUIPanels(), "confirmed panel deletion did not complete");
        Require(cse::server_browser::AreGameUIPanelsRetired() && !root.Get() && !g_pTaskbar,
                "retirement left real root/taskbar handles alive");
        owner.Shutdown();
        owner.Shutdown();
        Require(g_VModuleLoader.unloads == priorUnloads + 1, "repeat shutdown repeated module unload");
        Require(destructions == priorDestructions + (withConsole ? 3 : 2), "child ownership leaked a panel");
        delete engineRoot;
    }
}
}
