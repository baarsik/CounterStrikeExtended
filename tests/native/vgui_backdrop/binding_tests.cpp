#include "../public/vgui_background_controls.h"
#include <clocale>
#include <cstdio>
#include <cstring>

// Only unrelated GUI services are substituted. The service API, query, binding storage and
// both modules' actual initialization/shutdown functions are compiled from production inputs.
namespace cse {
namespace localization {
void BindControls(CreateInterfaceFn*, int) {}
void ClearControls() {}
} // namespace localization
} // namespace cse

struct IKeyValues {
    void RegisterSizeofKeyValues(int) {}
};
struct KeyValues {};
IKeyValues keys;
IKeyValues* g_pKeyValuesInterface = nullptr;

namespace vgui2 {
struct IInput {};
struct ISchemeManager {};
struct ISurface {};
struct ISystem {};
struct IPanel {};
struct IFileSystem {};
struct ILocalize {};
struct IVGui {
    bool Init(CreateInterfaceFn*, int) {
        return true;
    }
};
IInput* g_pInputInterface;
ISchemeManager* g_pSchemeInterface;
ISurface* g_pSurfaceInterface;
ISystem* g_pSystemInterface;
IVGui* g_pVGuiInterface;
IPanel* g_pPanelInterface;
IFileSystem* g_pFileSystemInterface;
ILocalize* g_pLocalizeInterface;
char g_szControlsModuleName[256];
void VGuiControls_Shutdown();
#include CSE_BINDINGS_FILE
} // namespace vgui2

bool CSE_ABI_CALL Draw(std::uintptr_t, const cse::ui::BackdropStyle*) {
    return true;
}
cse::ui::BackgroundEffectsApi effects{sizeof(effects), cse::ui::BackgroundEffectsVersion, Draw};
int selectedStyle = 1;
int CSE_ABI_CALL CurrentStyle() { return selectedStyle; }
bool CSE_ABI_CALL DrawSurface(const cse::ui::BackdropRect*, const cse::ui::BackdropRect*,
    const cse::ui::BackdropStyle*, float) { return true; }
cse::ui::StyleApi style{sizeof(style), cse::ui::StyleApiVersion, CurrentStyle, DrawSurface};

void* Factory(const char* name, int*) {
    static vgui2::IVGui gui;
    if (std::strcmp(name, cse::ui::BackgroundEffectsInterface) == 0) {
        return &effects;
    }
    if (std::strcmp(name, cse::ui::StyleInterface) == 0) {
        return &style;
    }
    if (std::strcmp(name, "KeyValues003") == 0) {
        return &keys;
    }
    return &gui;
}

int main() {
    CreateInterfaceFn factories[] = {nullptr, Factory};
    if (!vgui2::VGuiControls_Init("backdrop-fixture", factories, 2) ||
        cse::ui::ControlsBackgroundEffects() != &effects) {
        std::fputs("Controls initialization did not bind the optional background service\n", stderr);
        return 1;
    }
    if (!cse::ui::ControlsSmokedGlass() || cse::ui::ControlsStyleChanged(&keys)) {
        return 2;
    }
    cse::ui::RememberControlStyle(&keys);
    selectedStyle = 0;
    if (cse::ui::ControlsSmokedGlass() || !cse::ui::ControlsStyleChanged(&keys)) {
        return 3;
    }
    cse::ui::RememberControlStyle(&keys);
    if (cse::ui::ControlsStyleChanged(&keys)) {
        return 4;
    }
    vgui2::VGuiControls_Shutdown();
    if (cse::ui::ControlsBackgroundEffects() != nullptr) {
        std::fputs("Controls shutdown retained a borrowed engine service\n", stderr);
        return 1;
    }
    std::puts("Actual controls lifecycle binds and clears background effects");
    return 0;
}
