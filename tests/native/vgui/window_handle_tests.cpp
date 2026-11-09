#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <array>
#include <cstdint>
#include <cstdio>
#include "../../../GoldSrc-SDK/vgui2/src/window_panel_handle.h"

int main() {
    HWND window = CreateWindowExA(0, "STATIC", "CSE owned panel handle fixture", 0,
                                 0, 0, 1, 1, HWND_MESSAGE, nullptr, GetModuleHandleA(nullptr), nullptr);
    if (!window) {
        std::fprintf(stderr, "Creating owned message-only window failed: %lu\n", GetLastError());
        return 1;
    }

    constexpr std::array<vgui2::HPanel, 5> handles = {0, 1, 0x7fffffffu, 0x80000001u, UINT32_MAX};
    bool passed = true;
    for (const auto handle : handles) {
        vgui2::WriteWindowPanelHandle(window, handle);
        if (vgui2::ReadWindowPanelHandle(window) != handle) {
            std::fprintf(stderr, "Panel handle lost generation/index bits: %08x\n", handle);
            passed = false;
        }
    }
    if (!DestroyWindow(window)) {
        passed = false;
    }
    return passed ? 0 : 1;
}
