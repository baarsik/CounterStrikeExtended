// Native SDL3 Windows regression against the measured SDL2 2.0.20 layout/event baseline.
#define SDL_MAIN_HANDLED
#include <windows.h>
#include "../../GoldSrc-SDK/common/sdl3_keyboard.h"
#include <cstdio>
#include <cstring>

namespace {
constexpr unsigned nativeScanOne = 0x02;
constexpr unsigned nativeScanZero = 0x0b;
constexpr unsigned nativeScanQ = 0x10;
constexpr unsigned nativeScanA = 0x1e;
constexpr unsigned nativeScanCodeShift = 16;
constexpr unsigned long keyReleasedBits = 0xc0000000UL;

struct KeyCase {
    SDL_Scancode scancode;
    unsigned nativeScan;
    unsigned expected;
};

struct LayoutCase {
    const wchar_t *identifier;
    const char *name;
    KeyCase keys[4];
};

// The pinned SDL2 runtime preserves numeric rows and Latin fallback for non-Latin layouts.
const LayoutCase layouts[] = {{L"00000409",
                               "en-US",
                               {{SDL_SCANCODE_1, nativeScanOne, '1'},
                                {SDL_SCANCODE_0, nativeScanZero, '0'},
                                {SDL_SCANCODE_Q, nativeScanQ, 'q'},
                                {SDL_SCANCODE_A, nativeScanA, 'a'}}},
                              {L"0000040c",
                               "fr-FR",
                               {{SDL_SCANCODE_1, nativeScanOne, '1'},
                                {SDL_SCANCODE_0, nativeScanZero, '0'},
                                {SDL_SCANCODE_Q, nativeScanQ, 'a'},
                                {SDL_SCANCODE_A, nativeScanA, 'q'}}},
                              {L"00000419",
                               "ru-RU",
                               {{SDL_SCANCODE_1, nativeScanOne, '1'},
                                {SDL_SCANCODE_0, nativeScanZero, '0'},
                                {SDL_SCANCODE_Q, nativeScanQ, 'q'},
                                {SDL_SCANCODE_A, nativeScanA, 'a'}}}};

void DrainEvents() {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
    }
}

unsigned SendNativeKey(HWND window, HKL layout, const KeyCase &key) {
    DrainEvents();
    const WPARAM virtualKey = MapVirtualKeyExW(key.nativeScan, MAPVK_VSC_TO_VK, layout);
    const LPARAM scanBits = static_cast<LPARAM>(key.nativeScan << nativeScanCodeShift);
    SendMessageW(window, WM_KEYDOWN, virtualKey, scanBits | 1);
    SendMessageW(window, WM_KEYUP, virtualKey, scanBits | static_cast<LPARAM>(keyReleasedBits) | 1);
    SDL_PumpEvents();
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_EVENT_KEY_DOWN && event.key.scancode == key.scancode) {
            return static_cast<unsigned>(event.key.key);
        }
    }
    return 0;
}
} // namespace

int main(int argc, char **argv) {
    const bool useBindingPolicy = !(argc > 1 && std::strcmp(argv[1], "--before") == 0);
    const HKL originalLayout = GetKeyboardLayout(0);
    BYTE originalKeyboardState[256] = {};
    GetKeyboardState(originalKeyboardState);
    HKL originalLayouts[32] = {};
    const int originalLayoutCount = GetKeyboardLayoutList(32, originalLayouts);
    if (useBindingPolicy && !cse::ConfigureKeyboardMapping()) {
        std::fprintf(stderr, "SDL keyboard policy failed: %s\n", SDL_GetError());
        return 2;
    }
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        std::fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 2;
    }
    SDL_Window *window = SDL_CreateWindow("SDL keyboard regression", 320, 200, 0);
    if (!window) {
        std::fprintf(stderr, "SDL window failed: %s\n", SDL_GetError());
        SDL_Quit();
        return 2;
    }
    HWND nativeWindow = static_cast<HWND>(
        SDL_GetPointerProperty(SDL_GetWindowProperties(window), SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr));
    std::printf("SDL runtime=%d revision=%s\n", SDL_GetVersion(), SDL_GetRevision());
    SDL_RaiseWindow(window);
    SetForegroundWindow(nativeWindow);
    SetFocus(nativeWindow);
    SDL_PumpEvents();
    int failures = 0;
    for (const LayoutCase &layoutCase : layouts) {
        HKL layout = LoadKeyboardLayoutW(layoutCase.identifier, KLF_ACTIVATE);
        if (!layout || !ActivateKeyboardLayout(layout, 0)) {
            std::fprintf(stderr, "Could not activate %s, error=%lu\n", layoutCase.name, GetLastError());
            ++failures;
            continue;
        }
        SendMessageW(nativeWindow, WM_INPUTLANGCHANGE, 0, reinterpret_cast<LPARAM>(layout));
        BYTE clearedKeyboardState[256] = {};
        SetKeyboardState(clearedKeyboardState);
        SDL_SetModState(SDL_KMOD_NONE);
        SDL_PumpEvents();
        for (const KeyCase &key : layoutCase.keys) {
            const unsigned label =
                static_cast<unsigned>(useBindingPolicy ? cse::KeycodeForBindingLabel(key.scancode)
                                                       : SDL_GetKeyFromScancode(key.scancode, SDL_KMOD_NONE, false));
            const unsigned event = SendNativeKey(nativeWindow, layout, key);
            const unsigned virtualKey = MapVirtualKeyExW(key.nativeScan, MAPVK_VSC_TO_VK, layout);
            std::printf("native layout=%s VK=%04X charA=%04X charW=%04X\n", layoutCase.name, virtualKey,
                        MapVirtualKeyExA(virtualKey, MAPVK_VK_TO_CHAR, layout),
                        MapVirtualKeyExW(virtualKey, MAPVK_VK_TO_CHAR, layout));
            const bool passed = label == key.expected && event == key.expected;
            std::printf("%s scan=%u expected=U+%04X label=U+%04X event=U+%04X %s\n", layoutCase.name,
                        static_cast<unsigned>(key.scancode), key.expected, label, event, passed ? "PASS" : "FAIL");
            if (!passed) {
                ++failures;
            }
        }
        ActivateKeyboardLayout(originalLayout, 0);
        SendMessageW(nativeWindow, WM_INPUTLANGCHANGE, 0, reinterpret_cast<LPARAM>(originalLayout));
        bool wasLoaded = false;
        for (int index = 0; index < originalLayoutCount; ++index) {
            wasLoaded = wasLoaded || originalLayouts[index] == layout;
        }
        if (!wasLoaded) {
            UnloadKeyboardLayout(layout);
        }
    }
    ActivateKeyboardLayout(originalLayout, 0);
    SetKeyboardState(originalKeyboardState);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return failures == 0 ? 0 : 1;
}
