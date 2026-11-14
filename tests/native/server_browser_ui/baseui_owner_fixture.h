namespace baseui_owner_fixture {
using OwnedPanel = panel_owner_fixture::OwnedPanel;
OwnedPanel *staticPanel = nullptr;
OwnedPanel *staticClientDLLPanel = nullptr;
OwnedPanel *staticGameUIPanel = nullptr;
void *staticCareerUI = nullptr;
void *staticGameConsole = nullptr;
void *mousecontrol = nullptr;
bool m_bConsoleShowing = false;
bool browserReady = true;
struct Client {
    void Shutdown() {
        ++shutdowns;
    }
    int shutdowns = 0;
} client;
Client *staticClient = &client;
Client gameUI;
Client *staticGameUIFuncs = &gameUI;
struct BaseUISurface {
    void SolveTraverse(::vgui2::VPANEL panel, bool force);
    void SetEmbeddedPanel(::vgui2::VPANEL panel);
    ::vgui2::VPANEL GetEmbeddedPanel();
    void InternalSchemeSettingsTraverse(::vgui2::VPANEL panel, bool) {
        Require(panel != 0, "actual engine surface traversed detached null root after dispatch");
    }
    void InternalThinkTraverse(::vgui2::VPANEL) {}
    void InternalSolveTraverse(::vgui2::VPANEL) {}
    ::vgui2::VPANEL _embeddedPanel = 0;
    void Shutdown() {
        Require(!staticPanel, "actual BaseUI retired surface before deleting engine panel tree");
        ++shutdowns;
    }
    int shutdowns = 0;
} surface;
BaseUISurface *staticSurface = &surface;
struct Functions {
    const char *(*pfnGetGameDirectory)() = [] { return "cstrike"; };
} gEngfuncs;
int Q_strcasecmp(const char *left, const char *right) {
    return std::strcmp(left, right);
}
int unloads = 0;
void Sys_UnloadModule(void *module) {
    if (!module) {
        return;
    }
    Require(!staticPanel, "actual BaseUI unloaded image while root code owners remained live");
    ++unloads;
}
namespace vgui2 {
using VPANEL = ::vgui2::VPANEL;
struct System {
    void SaveUserConfigFile() {}
} systemOwner;
System *system() {
    return &systemOwner;
}
::vgui2::IVGui *ivgui() {
    return ::vgui2::ivgui();
}
}
struct Controller {};
namespace cse {
namespace localization {
void UnbindRuntime() {}
}
bool NativeGameUIIsRetirementSafePoint() {
    return true;
}
namespace browser {
void BeginControllerShutdown(Controller &) {}
}
}
class CBaseUI {
public:
    bool TryRetireGameUI();
    bool TryShutdown();
    const ::cse::gameui_lifetime::RetirementV1 *m_nativeRetirement = nullptr;
    bool m_nativeGameUIInitialized = true;
    bool m_nativeControlsInitialized = true;
    bool m_nativeShutdownBusy = false;
    bool m_nativeRetirementBusy = false;
    bool m_nativeClientInitialized = true;
    bool m_nativeClientShutdown = false;
    bool m_nativeRootDeletionQueued = false;
    ::vgui2::DHANDLE<OwnedPanel> m_nativeRoot;
    std::unique_ptr<Controller> m_nativeChromeController;
    void *m_hVGuiModule = this;
    void *m_hStaticGameUIModule = this;
    void *m_hChromeModule = nullptr;
};
#include "baseui_surface_retirement.inc"
#include "baseui_shutdown.inc"
bool CSE_ABI_CALL BrowserReady() {
    return browserReady;
}
void Run(::vgui2::IVGui *gui) {
    const ::cse::gameui_lifetime::RetirementV1 retirement{sizeof(::cse::gameui_lifetime::RetirementV1),
                                                          ::cse::gameui_lifetime::ContractVersion, BrowserReady,
                                                          BrowserReady};
    CBaseUI owner;
    owner.m_nativeRetirement = &retirement;
    staticPanel = new OwnedPanel(nullptr, "Actual BaseUI engine root");
    staticClientDLLPanel = new OwnedPanel(staticPanel, "Client parent");
    staticGameUIPanel = new OwnedPanel(staticPanel, "GameUI parent");
    ::vgui2::DHANDLE<OwnedPanel> root;
    root = staticPanel;
    surface.SetEmbeddedPanel(staticPanel->GetVPanel());
    browserReady = false;
    Require(!owner.TryShutdown(), "browser refusal allowed engine tree retirement");
    Require(root.Get() && client.shutdowns == 0 && unloads == 0, "refusal retired dependent owners");
    browserReady = true;
    Require(!owner.TryShutdown(), "queued engine tree deletion claimed immediate completion");
    Require(root.Get() && client.shutdowns == 1 && unloads == 0, "client/root shutdown ordering failed");
    gui->RunFrame();
    Require(surface.GetEmbeddedPanel() == 0, "engine surface kept freed embedded root through dispatch");
    surface.SolveTraverse(surface.GetEmbeddedPanel(), false);
    Require(owner.TryShutdown(), "confirmed engine root deletion did not complete");
    Require(!root.Get() && client.shutdowns == 1 && surface.shutdowns == 1 && unloads == 2,
            "engine cleanup repeated or omitted a dependency");
    Require(owner.TryShutdown(), "repeat engine shutdown did not remain complete");
    const int completedClientShutdowns = client.shutdowns;
    const int completedGameUIShutdowns = gameUI.shutdowns;
    CBaseUI partial;
    partial.m_nativeGameUIInitialized = false;
    partial.m_nativeClientInitialized = false;
    partial.m_nativeControlsInitialized = false;
    partial.m_hStaticGameUIModule = nullptr;
    Require(partial.TryShutdown(), "VGUI-image-only partial initialization did not retire");
    Require(client.shutdowns == completedClientShutdowns && gameUI.shutdowns == completedGameUIShutdowns,
            "partial initialization invoked never-entered client/GameUI owners");
    // Reuse the same private owner after completed retirement, with the Start entry checkpoints restored.
    owner.m_hVGuiModule = &owner;
    owner.m_hStaticGameUIModule = &owner;
    owner.m_nativeRetirement = &retirement;
    owner.m_nativeGameUIInitialized = true;
    owner.m_nativeControlsInitialized = true;
    owner.m_nativeClientInitialized = true;
    owner.m_nativeClientShutdown = false;
    staticClient = &client;
    staticGameUIFuncs = &gameUI;
    staticSurface = &surface;
    staticPanel = new OwnedPanel(nullptr, "Restarted actual engine root");
    surface.SetEmbeddedPanel(staticPanel->GetVPanel());
    root = staticPanel;
    Require(!owner.TryShutdown(), "restarted root deletion was mistaken for synchronous retirement");
    gui->RunFrame();
    Require(owner.TryShutdown() && !root.Get(), "same owner could not retire a restarted panel tree");
    Require(client.shutdowns == completedClientShutdowns + 1 && gameUI.shutdowns == completedGameUIShutdowns + 1,
            "restart did not reset once-only owner checkpoints");
}
}
