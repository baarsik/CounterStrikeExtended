#include "html_key_codes.h"
#include "vgui_key_translation.h"
#include <SDL3/SDL_keycode.h>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <limits>

struct Case {
    vgui2::KeyCode vguiKey;
    SDL_Keycode input;
    int browserKey;
    bool keypad;
};

// Literal Win32 VK expectations are independent of the production mapper.
// Toggle keys are state-query codes and have no separate SDL key event.
constexpr Case Cases[] = {
    {vgui2::KEY_NONE, SDLK_UNKNOWN, 0x0, false},
    {vgui2::KEY_0, SDLK_0, 0x30, false},
    {vgui2::KEY_1, SDLK_1, 0x31, false},
    {vgui2::KEY_2, SDLK_2, 0x32, false},
    {vgui2::KEY_3, SDLK_3, 0x33, false},
    {vgui2::KEY_4, SDLK_4, 0x34, false},
    {vgui2::KEY_5, SDLK_5, 0x35, false},
    {vgui2::KEY_6, SDLK_6, 0x36, false},
    {vgui2::KEY_7, SDLK_7, 0x37, false},
    {vgui2::KEY_8, SDLK_8, 0x38, false},
    {vgui2::KEY_9, SDLK_9, 0x39, false},
    {vgui2::KEY_A, SDLK_A, 0x41, false},
    {vgui2::KEY_B, SDLK_B, 0x42, false},
    {vgui2::KEY_C, SDLK_C, 0x43, false},
    {vgui2::KEY_D, SDLK_D, 0x44, false},
    {vgui2::KEY_E, SDLK_E, 0x45, false},
    {vgui2::KEY_F, SDLK_F, 0x46, false},
    {vgui2::KEY_G, SDLK_G, 0x47, false},
    {vgui2::KEY_H, SDLK_H, 0x48, false},
    {vgui2::KEY_I, SDLK_I, 0x49, false},
    {vgui2::KEY_J, SDLK_J, 0x4a, false},
    {vgui2::KEY_K, SDLK_K, 0x4b, false},
    {vgui2::KEY_L, SDLK_L, 0x4c, false},
    {vgui2::KEY_M, SDLK_M, 0x4d, false},
    {vgui2::KEY_N, SDLK_N, 0x4e, false},
    {vgui2::KEY_O, SDLK_O, 0x4f, false},
    {vgui2::KEY_P, SDLK_P, 0x50, false},
    {vgui2::KEY_Q, SDLK_Q, 0x51, false},
    {vgui2::KEY_R, SDLK_R, 0x52, false},
    {vgui2::KEY_S, SDLK_S, 0x53, false},
    {vgui2::KEY_T, SDLK_T, 0x54, false},
    {vgui2::KEY_U, SDLK_U, 0x55, false},
    {vgui2::KEY_V, SDLK_V, 0x56, false},
    {vgui2::KEY_W, SDLK_W, 0x57, false},
    {vgui2::KEY_X, SDLK_X, 0x58, false},
    {vgui2::KEY_Y, SDLK_Y, 0x59, false},
    {vgui2::KEY_Z, SDLK_Z, 0x5a, false},
    {vgui2::KEY_PAD_0, SDLK_KP_0, 0x60, true},
    {vgui2::KEY_PAD_1, SDLK_KP_1, 0x61, true},
    {vgui2::KEY_PAD_2, SDLK_KP_2, 0x62, true},
    {vgui2::KEY_PAD_3, SDLK_KP_3, 0x63, true},
    {vgui2::KEY_PAD_4, SDLK_KP_4, 0x64, true},
    {vgui2::KEY_PAD_5, SDLK_KP_5, 0x65, true},
    {vgui2::KEY_PAD_6, SDLK_KP_6, 0x66, true},
    {vgui2::KEY_PAD_7, SDLK_KP_7, 0x67, true},
    {vgui2::KEY_PAD_8, SDLK_KP_8, 0x68, true},
    {vgui2::KEY_PAD_9, SDLK_KP_9, 0x69, true},
    {vgui2::KEY_PAD_DIVIDE, SDLK_KP_DIVIDE, 0x6f, true},
    {vgui2::KEY_PAD_MULTIPLY, SDLK_KP_MULTIPLY, 0x6a, true},
    {vgui2::KEY_PAD_MINUS, SDLK_KP_MINUS, 0x6d, true},
    {vgui2::KEY_PAD_PLUS, SDLK_KP_PLUS, 0x6b, true},
    {vgui2::KEY_PAD_ENTER, SDLK_KP_ENTER, 0xd, true},
    {vgui2::KEY_PAD_DECIMAL, SDLK_KP_DECIMAL, 0x6e, true},
    {vgui2::KEY_LBRACKET, SDLK_LEFTBRACKET, 0xdb, false},
    {vgui2::KEY_RBRACKET, SDLK_RIGHTBRACKET, 0xdd, false},
    {vgui2::KEY_SEMICOLON, SDLK_SEMICOLON, 0xba, false},
    {vgui2::KEY_APOSTROPHE, SDLK_APOSTROPHE, 0xde, false},
    {vgui2::KEY_BACKQUOTE, SDLK_GRAVE, 0xc0, false},
    {vgui2::KEY_COMMA, SDLK_COMMA, 0xbc, false},
    {vgui2::KEY_PERIOD, SDLK_PERIOD, 0xbe, false},
    {vgui2::KEY_SLASH, SDLK_SLASH, 0xbf, false},
    {vgui2::KEY_BACKSLASH, SDLK_BACKSLASH, 0xdc, false},
    {vgui2::KEY_MINUS, SDLK_MINUS, 0xbd, false},
    {vgui2::KEY_EQUAL, SDLK_EQUALS, 0xbb, false},
    {vgui2::KEY_ENTER, SDLK_RETURN, 0xd, false},
    {vgui2::KEY_SPACE, SDLK_SPACE, 0x20, false},
    {vgui2::KEY_BACKSPACE, SDLK_BACKSPACE, 0x8, false},
    {vgui2::KEY_TAB, SDLK_TAB, 0x9, false},
    {vgui2::KEY_CAPSLOCK, SDLK_CAPSLOCK, 0x14, false},
    {vgui2::KEY_NUMLOCK, SDLK_NUMLOCKCLEAR, 0x90, false},
    {vgui2::KEY_ESCAPE, SDLK_ESCAPE, 0x1b, false},
    {vgui2::KEY_SCROLLLOCK, SDLK_SCROLLLOCK, 0x91, false},
    {vgui2::KEY_INSERT, SDLK_INSERT, 0x2d, false},
    {vgui2::KEY_DELETE, SDLK_DELETE, 0x2e, false},
    {vgui2::KEY_HOME, SDLK_HOME, 0x24, false},
    {vgui2::KEY_END, SDLK_END, 0x23, false},
    {vgui2::KEY_PAGEUP, SDLK_PAGEUP, 0x21, false},
    {vgui2::KEY_PAGEDOWN, SDLK_PAGEDOWN, 0x22, false},
    {vgui2::KEY_BREAK, SDLK_PAUSE, 0x13, false},
    {vgui2::KEY_LSHIFT, SDLK_LSHIFT, 0xa0, false},
    {vgui2::KEY_RSHIFT, SDLK_RSHIFT, 0xa1, false},
    {vgui2::KEY_LALT, SDLK_LALT, 0xa4, false},
    {vgui2::KEY_RALT, SDLK_RALT, 0xa5, false},
    {vgui2::KEY_LCONTROL, SDLK_LCTRL, 0xa2, false},
    {vgui2::KEY_RCONTROL, SDLK_RCTRL, 0xa3, false},
    {vgui2::KEY_LWIN, SDLK_LGUI, 0x5b, false},
    {vgui2::KEY_RWIN, SDLK_RGUI, 0x5c, false},
    {vgui2::KEY_APP, SDLK_APPLICATION, 0x5d, false},
    {vgui2::KEY_UP, SDLK_UP, 0x26, false},
    {vgui2::KEY_LEFT, SDLK_LEFT, 0x25, false},
    {vgui2::KEY_DOWN, SDLK_DOWN, 0x28, false},
    {vgui2::KEY_RIGHT, SDLK_RIGHT, 0x27, false},
    {vgui2::KEY_F1, SDLK_F1, 0x70, false},
    {vgui2::KEY_F2, SDLK_F2, 0x71, false},
    {vgui2::KEY_F3, SDLK_F3, 0x72, false},
    {vgui2::KEY_F4, SDLK_F4, 0x73, false},
    {vgui2::KEY_F5, SDLK_F5, 0x74, false},
    {vgui2::KEY_F6, SDLK_F6, 0x75, false},
    {vgui2::KEY_F7, SDLK_F7, 0x76, false},
    {vgui2::KEY_F8, SDLK_F8, 0x77, false},
    {vgui2::KEY_F9, SDLK_F9, 0x78, false},
    {vgui2::KEY_F10, SDLK_F10, 0x79, false},
    {vgui2::KEY_F11, SDLK_F11, 0x7a, false},
    {vgui2::KEY_F12, SDLK_F12, 0x7b, false},
    {vgui2::KEY_CAPSLOCKTOGGLE, SDLK_UNKNOWN, 0x14, false},
    {vgui2::KEY_NUMLOCKTOGGLE, SDLK_UNKNOWN, 0x90, false},
    {vgui2::KEY_SCROLLLOCKTOGGLE, SDLK_UNKNOWN, 0x91, false},
};

constexpr bool AllMappingsAreConstantExpressions() {
    for (const Case& item : Cases) {
        if (cse::html::WindowsVirtualKey(item.vguiKey) != item.browserKey) {
            return false;
        }
    }
    return true;
}

static_assert(AllMappingsAreConstantExpressions());
static_assert(std::size(Cases) == static_cast<std::size_t>(vgui2::KEY_LAST));
static_assert(cse::html::KeyModifiers(vgui2::KEY_PAD_ENTER, 7) == 15);
static_assert(cse::html::KeyModifiers(vgui2::KEY_ENTER, 7) == 7);

int main() {
    std::array<bool, vgui2::KEY_LAST> covered = {};
    std::size_t sdlCases = 0;
    for (const Case& item : Cases) {
        const int code = static_cast<int>(item.vguiKey);
        if (code < 0 || code >= vgui2::KEY_LAST || covered[code]) {
            std::fprintf(stderr, "Duplicate or invalid VGUI fixture key %d\n", code);
            return EXIT_FAILURE;
        }
        covered[code] = true;
        if (cse::html::WindowsVirtualKey(code) != item.browserKey) {
            std::fprintf(stderr, "Browser virtual-key translation failed for VGUI key %d\n", code);
            return EXIT_FAILURE;
        }
        for (int modifiers = 0; modifiers <= 7; ++modifiers) {
            const int expectedModifiers = modifiers | (item.keypad ? 8 : 0);
            if (cse::html::KeyModifiers(code, modifiers) != expectedModifiers) {
                std::fprintf(stderr, "Browser keypad modifier failed for VGUI key %d\n", code);
                return EXIT_FAILURE;
            }
        }
        if (item.input != SDLK_UNKNOWN) {
            const auto translated = KeyCode_VirtualKeyToVGUI(static_cast<int>(item.input));
            if (translated != item.vguiKey || cse::html::WindowsVirtualKey(translated) != item.browserKey) {
                std::fprintf(stderr, "Browser key translation failed for SDL key %u\n", item.input);
                return EXIT_FAILURE;
            }
            ++sdlCases;
        }
    }
    for (bool present : covered) {
        if (!present) {
            std::fputs("VGUI key missing from browser mapping fixtures\n", stderr);
            return EXIT_FAILURE;
        }
    }
    const int invalidCodes[] = {-1, vgui2::KEY_LAST, vgui2::KEY_LAST + 1,
        std::numeric_limits<int>::min(), std::numeric_limits<int>::max()};
    for (int code : invalidCodes) {
        if (cse::html::WindowsVirtualKey(code) != 0 || cse::html::KeyModifiers(code, 7) != 7) {
            std::fprintf(stderr, "Invalid VGUI key %d changed the browser event\n", code);
            return EXIT_FAILURE;
        }
    }
    const auto keypadPeriod = KeyCode_VirtualKeyToVGUI(static_cast<int>(SDLK_KP_PERIOD));
    if (keypadPeriod != vgui2::KEY_PAD_DECIMAL || cse::html::WindowsVirtualKey(keypadPeriod) != 0x6e
        || KeyCode_VirtualKeyToVGUI(static_cast<int>(SDLK_UNKNOWN)) != vgui2::KEY_NONE) {
        return EXIT_FAILURE;
    }
    std::printf("All %zu VGUI key codes, %zu SDL inputs and 8 modifier combinations passed\n",
        std::size(Cases), sdlCases);
    return EXIT_SUCCESS;
}
