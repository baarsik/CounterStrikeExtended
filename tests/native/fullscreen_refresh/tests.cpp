// Complete production fullscreen transition; SDL display operations are injected leaves.
#include <SDL3/SDL.h>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <iostream>
#include <string>
#define CHECK(value) do { if (!(value)) { throw std::runtime_error(#value); } } while (false)
SDL_DisplayMode desktop{};
SDL_DisplayMode closest{};
SDL_DisplayMode selected{};
bool desktopSelected = false;
bool windowed = false;
bool available = true;
bool accepted = true;
int modeCalls = 0;
int fullscreenCalls = 0;
int closestCalls = 0;
int bNeedsFullScreenModeSwitch = 0;
int com_argc = 3;
constexpr SDL_DisplayID FixtureDisplay = 7;
constexpr float RequestedRate = 144.0f;
constexpr float DesktopRate = 60.0f;
char argument[] = "144";
char *com_argv[]{nullptr, nullptr, argument};
bool requested = true;

int COM_CheckParm(const char *name) {
    return requested && std::strcmp(name, "-freq") == 0 ? 1 : 0;
}

void Con_DPrintf(const char *, ...) {}

void Sys_Error(const char *, ...) {
    throw std::runtime_error("Sys_Error");
}

struct Game {
    void *GetMainWindow() {
        return reinterpret_cast<void *>(1);
    }
} gameObject;
Game *game = &gameObject;
struct CVideoMode_OpenGL {
    bool IsWindowedMode() {
        return windowed;
    }
    void ChangeDisplaySettingsToFullscreen();
};

extern "C" SDL_DisplayID SDL_GetDisplayForWindow(SDL_Window *) {
    return FixtureDisplay;
}

extern "C" SDL_DisplayID SDL_GetPrimaryDisplay() {
    return FixtureDisplay;
}

extern "C" const SDL_DisplayMode *SDL_GetDesktopDisplayMode(SDL_DisplayID id) {
    CHECK(id == FixtureDisplay);
    return available ? &desktop : nullptr;
}

extern "C" bool SDL_GetClosestFullscreenDisplayMode(SDL_DisplayID id, int width, int height,
                                                    float rate, bool, SDL_DisplayMode *output) {
    ++closestCalls;
    CHECK(id == FixtureDisplay && width == desktop.w && height == desktop.h && rate == RequestedRate);
    *output = closest;
    return available;
}

extern "C" bool SDL_SetWindowFullscreenMode(SDL_Window *, const SDL_DisplayMode *mode) {
    ++modeCalls;
    if (mode && !accepted) {
        return false;
    }
    desktopSelected = !mode;
    selected = mode ? *mode : desktop;
    return true;
}

extern "C" bool SDL_SetWindowFullscreen(SDL_Window *, bool value) {
    CHECK(value);
    ++fullscreenCalls;
    return true;
}

extern "C" const char *SDL_GetError() {
    return "injected failure";
}
#include "fullscreen.inc"

int main(int argc, char **argv) {
    try {
        CHECK(argc == 2);
        const std::string name = argv[1];
        desktop.w = 2560;
        desktop.h = 1440;
        desktop.refresh_rate = DesktopRate;
        closest = desktop;
        closest.refresh_rate = RequestedRate;
        if (name == "absent") {
            requested = false;
        }
        if (name == "unsupported") {
            closest.refresh_rate = 120.0f;
        }
        if (name == "resolution") {
            closest.w = 1920;
        }
        if (name == "unavailable") {
            available = false;
        }
        if (name == "rejected") {
            accepted = false;
        }
        if (name == "windowed") {
            windowed = true;
        }
        if (name == "fallback") {
            bNeedsFullScreenModeSwitch = 1;
        }
        CVideoMode_OpenGL{}.ChangeDisplaySettingsToFullscreen();
        if (windowed) {
            CHECK(modeCalls == 0 && fullscreenCalls == 0 && closestCalls == 0);
        } else if (bNeedsFullScreenModeSwitch) {
            CHECK(modeCalls == 0 && fullscreenCalls == 1 && closestCalls == 0);
        } else if (name == "supported") {
            CHECK(!desktopSelected && selected.w == desktop.w && selected.h == desktop.h);
            CHECK(selected.refresh_rate == RequestedRate && fullscreenCalls == 1);
        } else {
            CHECK(desktopSelected && selected.refresh_rate == DesktopRate && fullscreenCalls == 1);
        }
        std::cout << "PASS " << name << '\n';
    } catch (const std::exception &error) {
        std::cerr << "FAIL " << error.what() << '\n';
        return 1;
    }
}
