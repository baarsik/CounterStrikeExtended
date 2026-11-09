#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <type_traits>

#include "hud.h"
#include "cse_vgui.h"

using HudFont = decltype(CHudVGUI2Print::m_hudfont);
static_assert(sizeof(HudFont) == sizeof(std::uint32_t), "HUD font registry storage retains 32 bits");
static_assert(std::is_unsigned<HudFont>::value, "font registry IDs remain unsigned");
#if defined(_WIN32)
static_assert(std::is_same<HudFont, unsigned long>::value, "Windows retains its exact field type");
#else
static_assert(std::is_same<HudFont, vgui2::HFont>::value, "portable HUD storage uses the actual font handle");
#endif

namespace {

vgui2::HFont receivedFont;

int DrawCharacter(int, int, int, unsigned int font) {
    receivedFont = font;
    return 1;
}

}

int main() {
    constexpr std::uint32_t FontIds[] = {0, 1, 0xffff, 0x10000, 0x7fffffff, 0x80000000, 0xfffffffe, UINT32_MAX};
    // Use the actual engine callback field type; no HUD object/vtable is fabricated.
    decltype(cl_enginefunc_t::pfnVGUI2DrawCharacter) callback = DrawCharacter;
    for (std::uint32_t expected : FontIds) {
        HudFont stored = expected;
        callback(0, 0, 0, stored);
        if (receivedFont != expected) {
            std::fprintf(stderr, "font ID changed during storage/callback exchange\n");
            return EXIT_FAILURE;
        }
    }
    std::printf("HUD font bytes=%zu; %zu encoded IDs preserved through actual callback type\n",
        sizeof(HudFont), sizeof(FontIds) / sizeof(FontIds[0]));
    return EXIT_SUCCESS;
}
