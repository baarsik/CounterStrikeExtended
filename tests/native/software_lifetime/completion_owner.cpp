#include "native_vgui_completion.h"
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <memory>

static int releases = 0;
static int controllerDestructions = 0;
static bool processReady = false;
static bool providerReady = false;
static void Require(bool value, const char* text) {
    if (!value) {
        std::fprintf(stderr, "FAIL %s\n", text);
        std::exit(1);
    }
}
struct Panel {
    void SetVisible(bool) {}
    void MarkForDeletion() {}
};
struct RootHandle {
    Panel* panel = nullptr;
    Panel* Get() {
        return panel;
    }
    void operator=(Panel* value) {
        panel = value;
    }
};
struct Client {
    void Shutdown() {}
};
struct Surface {
    void SetEmbeddedPanel(std::uintptr_t) {}
    void Shutdown() {}
};
struct Controller {
    ~Controller() {
        ++controllerDestructions;
    }
    void Shutdown() {}
};
static Panel* staticPanel = nullptr;
static Panel* staticClientDLLPanel = nullptr;
static Panel* staticGameUIPanel = nullptr;
static Client* staticClient = nullptr;
static Client* staticGameUIFuncs = nullptr;
static void* staticCareerUI = nullptr;
static void* staticGameConsole = nullptr;
static void* mousecontrol = nullptr;
static Surface surface;
static Surface* staticSurface = &surface;
static void* g_InputInternal = nullptr;
static int gEngfuncs = 1;
namespace vgui2 {
struct Gui {
    void RunFrame() {}
};
struct System {
    void SaveUserConfigFile() {}
};
static Gui gui;
static System systemOwner;
static Gui* ivgui() {
    return &gui;
}
static System* system() {
    return &systemOwner;
}
static Surface* surface() {
    return staticSurface;
}
static void VGuiControls_Shutdown() {}
} // namespace vgui2
namespace cse::browser {
void BeginControllerShutdown(Controller&) {}
bool ControllerQuiescent(Controller&) {
    return processReady;
}
} // namespace cse::browser
static void Sys_UnloadModule(void* image) {
    if (image) {
        ++releases;
    }
}
static cse::vgui_completion::Result FinishProvider(const void*) noexcept {
    return providerReady ? cse::vgui_completion::Result::Complete : cse::vgui_completion::Result::Busy;
}
static const cse::vgui_completion::Api completion{sizeof(completion), cse::vgui_completion::Version, FinishProvider};
class CBaseUI {
public:
    bool TryShutdown();
    const cse::vgui_completion::Api* m_nativeCompletion = &completion;
    void* m_FactoryList[6]{};
    int m_iNumFactories = 6;
    bool TryRetireGameUI() {
        return true;
    }
    bool m_nativeShutdownBusy = false;
    bool m_nativeGameUIInitialized = false;
    bool m_nativeClientInitialized = false;
    bool m_nativeClientShutdown = false;
    bool m_nativeRootDeletionQueued = false;
    bool m_nativeControlsInitialized = true;
    bool m_bConsoleShowing = false;
    RootHandle m_nativeRoot;
    std::unique_ptr<Controller> m_nativeChromeController = std::make_unique<Controller>();
    void* m_hVGuiModule = this;
    void* m_hStaticGameUIModule = this;
    void* m_hChromeModule = nullptr;
    void* m_nativeRetirement = this;
};
#include "actual_completion.inc"
int main() {
    CBaseUI owner;
    Require(!owner.TryShutdown(), "failed browser process completion escaped actual BaseUI owner");
    Require(releases == 0 && controllerDestructions == 0 && owner.m_nativeChromeController,
            "pending browser completion released dependent image or process owner");
    processReady = true;
    Require(!owner.TryShutdown() && releases == 0 && controllerDestructions == 0,
            "pending provider completion released dependent image or controller");
    providerReady = true;
    Require(owner.TryShutdown(), "browser completion retry did not finish actual BaseUI owner");
    Require(releases == 2 && controllerDestructions == 1, "completion did not release each owner once");
    Require(owner.TryShutdown() && releases == 2 && controllerDestructions == 1, "repeat close advanced cleanup");
    std::puts("PASS actual BaseUI preserves browser process/image ownership on failed completion and retries");
}
