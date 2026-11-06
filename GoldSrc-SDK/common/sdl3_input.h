#pragma once

#include "sdl3_policy.h"
#include <SDL3/SDL.h>

namespace cse {
inline void GetIntegerMousePosition(int *x, int *y) {
    float mouseX = 0.0f;
    float mouseY = 0.0f;
    SDL_GetMouseState(&mouseX, &mouseY);
    *x = static_cast<int>(mouseX);
    *y = static_cast<int>(mouseY);
}

inline void ReadRelativeMotion(FractionalMotion &remainder, int *x, int *y) {
    float deltaX = 0.0f;
    float deltaY = 0.0f;
    SDL_GetRelativeMouseState(&deltaX, &deltaY);
    *x = remainder.TakeX(deltaX);
    *y = remainder.TakeY(deltaY);
}

// Preserve SDL2's 21 engine binding slots, including its paddle order.
constexpr SDL_GamepadButton LegacyGamepadButtons[LegacyGamepadButtonCount] = {
    SDL_GAMEPAD_BUTTON_SOUTH,         SDL_GAMEPAD_BUTTON_EAST,           SDL_GAMEPAD_BUTTON_WEST,
    SDL_GAMEPAD_BUTTON_NORTH,         SDL_GAMEPAD_BUTTON_BACK,           SDL_GAMEPAD_BUTTON_GUIDE,
    SDL_GAMEPAD_BUTTON_START,         SDL_GAMEPAD_BUTTON_LEFT_STICK,     SDL_GAMEPAD_BUTTON_RIGHT_STICK,
    SDL_GAMEPAD_BUTTON_LEFT_SHOULDER, SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER, SDL_GAMEPAD_BUTTON_DPAD_UP,
    SDL_GAMEPAD_BUTTON_DPAD_DOWN,     SDL_GAMEPAD_BUTTON_DPAD_LEFT,      SDL_GAMEPAD_BUTTON_DPAD_RIGHT,
    SDL_GAMEPAD_BUTTON_MISC1,         SDL_GAMEPAD_BUTTON_RIGHT_PADDLE1,  SDL_GAMEPAD_BUTTON_LEFT_PADDLE1,
    SDL_GAMEPAD_BUTTON_RIGHT_PADDLE2, SDL_GAMEPAD_BUTTON_LEFT_PADDLE2,   SDL_GAMEPAD_BUTTON_TOUCHPAD};
inline SDL_GamepadButton LegacyGamepadButton(int binding) {
    return binding >= 0 && binding < LegacyGamepadButtonCount ? LegacyGamepadButtons[binding]
                                                              : SDL_GAMEPAD_BUTTON_INVALID;
}

inline bool IsWindowLifecycleEvent(Uint32 type) {
    switch (type) {
    case SDL_EVENT_WINDOW_SHOWN:
    case SDL_EVENT_WINDOW_HIDDEN:
    case SDL_EVENT_WINDOW_FOCUS_GAINED:
    case SDL_EVENT_WINDOW_FOCUS_LOST:
    case SDL_EVENT_WINDOW_MINIMIZED:
    case SDL_EVENT_WINDOW_RESTORED:
    case SDL_EVENT_WINDOW_MOVED:
    case SDL_EVENT_WINDOW_RESIZED:
    case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
    case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
        return true;
    default:
        return false;
    }
}

} // namespace cse
