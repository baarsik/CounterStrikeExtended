#include "NativeKeyState.h"
#include "vgui/IInputInternal.h"
#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <SDL3/SDL_keyboard.h>
#endif
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <utility>
#include <vector>

using namespace vgui2;
namespace keys = cse_vgui2_input;
void UnexpectedInputCallback();
namespace {
void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", message);
        std::exit(1);
    }
}
std::uint16_t rawStates[256]{};
int queries = 0;
#if !defined(_WIN32)
bool keyboard[SDL_SCANCODE_COUNT]{};
SDL_Keymod modifiers = SDL_KMOD_NONE;
int keyboardCalls = 0;
int modifierCalls = 0;
int keyboardCount = SDL_SCANCODE_COUNT;
bool missingKeyboard = false;
#endif
}
short System_GetKeyState(int code) {
    ++queries;
    short result;
    static_assert(sizeof(result) == sizeof(rawStates[0]), "Actual Windows packed short width");
    std::memcpy(&result, &rawStates[code], sizeof(result));
    return result;
}
#if !defined(_WIN32)
extern "C" const bool* SDLCALL SDL_GetKeyboardState(int* count) {
    ++keyboardCalls;
    *count = keyboardCount;
    return missingKeyboard ? nullptr : keyboard;
}
extern "C" SDL_Keymod SDLCALL SDL_GetModState() {
    ++modifierCalls;
    return modifiers;
}
#include "native_boundary.inc"
#endif
#include "toggle_table.inc"

// Controlled method bindings only. Shared bodies are imported verbatim; this
// object is deliberately not a substitute CInput layout/vtable/runtime provider.
class CursorBinding {
public:
    bool available = true;
    int setCalls = 0;
    int getCalls = 0;
    int x = 17;
    int y = -23;
    bool HasCursorPosFunctions() {
        return available;
    }
    void SurfaceSetCursorPos(int newX, int newY) {
        ++setCalls;
        x = newX;
        y = newY;
    }
    void SurfaceGetCursorPos(int& outX, int& outY) {
        ++getCalls;
        outX = x;
        outY = y;
    }
    void GetAbsoluteWindowBounds(int&, int&, int&, int&) {
        UnexpectedInputCallback(); // Separate aborting binding; no OS/global fallback executes.
    }
};
CursorBinding cursor;
CursorBinding* g_pSurface = &cursor;
class CInputWin32 {
public:
    HInputContext m_hContext = DEFAULT_INPUT_CONTEXT;
    bool _updateToggleButtonState = true;
    bool down[KEY_LAST]{};
    const char* _keyTrans[KEY_LAST]{};
    std::vector<int> transitions;
    CInputWin32() {
#include "translation.inc"
    }
    bool IsKeyDown(KeyCode code) {
        return down[code];
    }
    void InternalKeyCodePressed(KeyCode code) {
        transitions.push_back(static_cast<int>(code) * 2 + 1);
        down[code] = true;
        UpdateToggleButtonState(); // Actual pressed method's reentrant ordering.
    }
    void InternalKeyCodeReleased(KeyCode code) {
        transitions.push_back(static_cast<int>(code) * 2);
        down[code] = false;
    }
    void SetCursorPos(int, int);
    void GetCursorPos(int&, int&);
    void GetKeyCodeText(KeyCode, char*, int);
    void UpdateToggleButtonState();
};
#include "methods.inc"
namespace {
constexpr KeyCode StateCodes[] = {KEY_CAPSLOCKTOGGLE, KEY_NUMLOCKTOGGLE, KEY_SCROLLLOCKTOGGLE,
    KEY_LSHIFT, KEY_RSHIFT, KEY_LCONTROL, KEY_RCONTROL, KEY_LALT, KEY_RALT};
void SetSnapshot(unsigned int state) {
    const bool leftShift = (state & 1U) != 0;
    const bool rightShift = (state & 2U) != 0;
    const bool leftControl = (state & 4U) != 0;
    const bool rightControl = (state & 8U) != 0;
    const bool leftAlt = (state & 16U) != 0;
    const bool rightAlt = (state & 32U) != 0;
    const bool caps = (state & 64U) != 0;
    const bool num = (state & 128U) != 0;
    const bool scroll = (state & 256U) != 0;
    // Independent fixed-protocol encoding for the actual Windows loop reference.
    const int codes[] = {0x14, 0x90, 0x91, 0xa0, 0xa1, 0xa2, 0xa3, 0xa4, 0xa5, 0x12, 0x10, 0x11};
    const bool downs[] = {false, false, false, leftShift, rightShift, leftControl, rightControl, leftAlt, rightAlt,
        leftAlt || rightAlt, leftShift || rightShift, leftControl || rightControl};
    const bool locks[] = {caps, num, scroll};
    for (int index = 0; index < 12; ++index) {
        rawStates[codes[index]] = static_cast<std::uint16_t>((downs[index] ? 0x8000U : 0U)
            | (index < 3 && locks[index] ? 1U : 0U));
    }
#if !defined(_WIN32)
    std::memset(keyboard, 0, sizeof(keyboard));
    keyboard[SDL_SCANCODE_LSHIFT] = leftShift;
    keyboard[SDL_SCANCODE_RSHIFT] = rightShift;
    keyboard[SDL_SCANCODE_LCTRL] = leftControl;
    keyboard[SDL_SCANCODE_RCTRL] = rightControl;
    keyboard[SDL_SCANCODE_LALT] = leftAlt;
    keyboard[SDL_SCANCODE_RALT] = rightAlt;
    modifiers = static_cast<SDL_Keymod>((caps ? SDL_KMOD_CAPS : 0U) | (num ? SDL_KMOD_NUM : 0U)
        | (scroll ? SDL_KMOD_SCROLL : 0U));
    keyboardCalls = 0;
    modifierCalls = 0;
#endif
    queries = 0;
}
void CursorAndText() {
    CInputWin32 input;
    input.SetCursorPos(-19, 240);
    int x = 0;
    int y = 0;
    input.GetCursorPos(x, y);
    Require(x == -19 && y == 240 && cursor.setCalls == 1 && cursor.getCalls == 1,
        "actual cursor method delegates exactly once without fallback/bounds access");
    char text[32]{};
    input.GetKeyCodeText(KEY_A, text, sizeof(text));
    Require(std::strcmp(text, "KEY_A") == 0, "actual constructor translation/name method");
    std::memset(text, '!', sizeof(text));
    input.GetKeyCodeText(KEY_F12, text, 3);
    Require(text[0] == 'K' && text[1] == 'E' && text[2] == 'Y' && text[3] == '!', "retained bounded nonterminating copy");
    char before[sizeof(text)];
    std::memcpy(before, text, sizeof(text));
    input.GetKeyCodeText(KEY_A, text, 0);
    Require(std::memcmp(before, text, sizeof(text)) == 0, "zero capacity preserves all storage");
    input.GetKeyCodeText(KEY_A, text, -1);
    Require(std::memcmp(before, text, sizeof(text)) == 0, "negative capacity preserves all storage");
    input.GetKeyCodeText(KEY_PAD_0, text, sizeof(text));
    Require(std::strcmp(text, "KEY_PAD_0") == 0, "embedded-zero translation prefix is preserved");
    input.GetKeyCodeText(KEY_A, nullptr, 10);
}
}
int main(int argc, char** argv) {
#if !defined(_WIN32)
    if (argc == 2) {
        CInputWin32 input;
        if (std::strcmp(argv[1], "missing-snapshot") == 0) {
            missingKeyboard = true;
            input.UpdateToggleButtonState();
        } else if (std::strcmp(argv[1], "short-snapshot") == 0) {
            keyboardCount = SDL_SCANCODE_RALT;
            input.UpdateToggleButtonState();
        } else if (std::strcmp(argv[1], "unsupported-set-cursor") == 0) {
            cursor.available = false;
            input.SetCursorPos(1, 2);
        } else if (std::strcmp(argv[1], "unsupported-get-cursor") == 0) {
            cursor.available = false;
            int x = 0;
            int y = 0;
            input.GetCursorPos(x, y);
        }
        return 99;
    }
#else
    (void)argc;
    (void)argv;
#endif
    CursorAndText();
    constexpr unsigned int Initial[] = {0, 0x1ff, 0xa8, 0x157};
    for (unsigned int state = 0; state < 512; ++state) {
        for (unsigned int initial : Initial) {
            for (int guard = 0; guard < 3; ++guard) {
                SetSnapshot(state);
                CInputWin32 input;
                input.m_hContext = guard == 2 ? 7 : DEFAULT_INPUT_CONTEXT;
                input._updateToggleButtonState = guard != 1;
                for (unsigned int index = 0; index < 9; ++index) {
                    input.down[StateCodes[index]] = (initial & (1U << index)) != 0;
                }
                input.UpdateToggleButtonState();
                input.UpdateToggleButtonState(); // Stale/default and alternate-context guards remain unchanged.
#if defined(_WIN32)
                Require(queries == (guard == 0 ? 12 : 0), "actual Windows query-before-ignore/once guard ordering");
#else
                Require(keyboardCalls == (guard == 0 ? 1 : 0) && modifierCalls == keyboardCalls && queries == 0,
                    "one native snapshot only for eligible default frame, including recursive/repeated calls");
#endif
                unsigned int mask = 0;
                for (unsigned int index = 0; index < 9; ++index) {
                    if (input.down[StateCodes[index]]) {
                        mask |= 1U << index;
                    }
                }
                std::printf("{\"state\":%u,\"initial\":%u,\"guard\":%d,\"enabled\":%d,\"down\":%u,\"events\":[",
                    state, initial, guard, input._updateToggleButtonState ? 1 : 0, mask);
                bool comma = false;
                for (int event : input.transitions) {
                    std::printf("%s%d", comma ? "," : "", event);
                    comma = true;
                }
                std::puts("]}");
            }
        }
    }
    return 0;
}
