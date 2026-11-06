#include "../../GoldSrc-SDK/common/sdl3_input.h"
#include <cassert>

int main() {
    const SDL_GamepadButton expected[] = {
        SDL_GAMEPAD_BUTTON_SOUTH,         SDL_GAMEPAD_BUTTON_EAST,           SDL_GAMEPAD_BUTTON_WEST,
        SDL_GAMEPAD_BUTTON_NORTH,         SDL_GAMEPAD_BUTTON_BACK,           SDL_GAMEPAD_BUTTON_GUIDE,
        SDL_GAMEPAD_BUTTON_START,         SDL_GAMEPAD_BUTTON_LEFT_STICK,     SDL_GAMEPAD_BUTTON_RIGHT_STICK,
        SDL_GAMEPAD_BUTTON_LEFT_SHOULDER, SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER, SDL_GAMEPAD_BUTTON_DPAD_UP,
        SDL_GAMEPAD_BUTTON_DPAD_DOWN,     SDL_GAMEPAD_BUTTON_DPAD_LEFT,      SDL_GAMEPAD_BUTTON_DPAD_RIGHT,
        SDL_GAMEPAD_BUTTON_MISC1,         SDL_GAMEPAD_BUTTON_RIGHT_PADDLE1,  SDL_GAMEPAD_BUTTON_LEFT_PADDLE1,
        SDL_GAMEPAD_BUTTON_RIGHT_PADDLE2, SDL_GAMEPAD_BUTTON_LEFT_PADDLE2,   SDL_GAMEPAD_BUTTON_TOUCHPAD};
    for (int binding = 0; binding < cse::LegacyGamepadButtonCount; ++binding) {
        assert(cse::LegacyGamepadButton(binding) == expected[binding]);
    }
    assert(cse::LegacyGamepadButton(-1) == SDL_GAMEPAD_BUTTON_INVALID);
    for (int binding = cse::LegacyGamepadButtonCount; binding < SDL_GAMEPAD_BUTTON_COUNT; ++binding) {
        assert(cse::LegacyGamepadButton(binding) == SDL_GAMEPAD_BUTTON_INVALID);
    }
    assert(cse::IsWindowLifecycleEvent(SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED));
    assert(cse::IsWindowLifecycleEvent(SDL_EVENT_WINDOW_FOCUS_LOST));
    assert(cse::IsWindowLifecycleEvent(SDL_EVENT_WINDOW_CLOSE_REQUESTED));
    assert(!cse::IsWindowLifecycleEvent(SDL_EVENT_KEY_UP));
    assert(!cse::IsWindowLifecycleEvent(SDL_EVENT_TEXT_INPUT));
    assert(!cse::IsWindowLifecycleEvent(SDL_EVENT_GAMEPAD_BUTTON_DOWN));
    assert(!cse::IsWindowLifecycleEvent(SDL_EVENT_MOUSE_WHEEL));
    cse::FractionalMotion wheel;
    assert(cse::WheelSteps(wheel, 0.25f, SDL_MOUSEWHEEL_NORMAL) == 0);
    assert(cse::WheelSteps(wheel, 0.75f, SDL_MOUSEWHEEL_NORMAL) == 1);
    assert(cse::WheelSteps(wheel, 0.0f, SDL_MOUSEWHEEL_NORMAL) == 0);
    assert(cse::WheelSteps(wheel, 2.0f, SDL_MOUSEWHEEL_FLIPPED) == -2);
    return 0;
}
