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
SDL_DisplayMode offered{};
SDL_DisplayMode lowerDensity{};
SDL_DisplayMode selected{};
bool desktopSelected = false;
bool windowed = false;
bool available = true;
bool accepted = true;
bool haveLowerDensity = false;
int modeCalls = 0;
int fullscreenCalls = 0;
int listCalls = 0;
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

extern "C" SDL_DisplayMode** SDL_GetFullscreenDisplayModes(SDL_DisplayID id, int* count) {
    ++listCalls;
    CHECK(id == FixtureDisplay);
    *count = haveLowerDensity ? 2 : 1;
    auto** modes = static_cast<SDL_DisplayMode**>(std::calloc(*count + 1, sizeof(SDL_DisplayMode*)));
    CHECK(modes);
    if (haveLowerDensity) {
        modes[0] = &lowerDensity;
    }
    modes[*count - 1] = &offered;
    return modes;
}

extern "C" void SDL_free(void* pointer) {
    std::free(pointer);
}

bool haveStandardMode = true;
int inclusiveQueries = 0;

extern "C" bool SDL_GetClosestFullscreenDisplayMode(SDL_DisplayID display, int, int, float,
    bool includeHighDensity, SDL_DisplayMode* result) {
    CHECK(display == FixtureDisplay);
    if (includeHighDensity) {
        ++inclusiveQueries;
        *result = offered;
        return available;
    }
    *result = lowerDensity;
    return haveStandardMode;
}

static SDL_DisplayMode FindRenderMode(int wide, int tall, int refresh) {
    SDL_DisplayMode requested{};
    SDL_DisplayMode mode{};
#include "render_mode.inc"
    return mode;
}

static void CheckRenderModes() {
    constexpr int renderWidth = 960;
    constexpr int renderHeight = 540;
    constexpr int requestedRefresh = 144;
    lowerDensity.w = renderWidth;
    lowerDensity.h = renderHeight;
    lowerDensity.refresh_rate = requestedRefresh;
    lowerDensity.pixel_density = 1.0f;
    offered = lowerDensity;
    offered.pixel_density = 2.0f;
    CHECK(FindRenderMode(renderWidth, renderHeight, requestedRefresh).pixel_density == 1.0f);
    CHECK(FindRenderMode(renderWidth, renderHeight, 0).pixel_density == 1.0f);
    CHECK(inclusiveQueries == 0);

    lowerDensity.w = 1024;
    lowerDensity.h = 576;
    CHECK(FindRenderMode(renderWidth, renderHeight, requestedRefresh).w == renderWidth);
    lowerDensity.w = renderWidth;
    lowerDensity.h = renderHeight;
    lowerDensity.refresh_rate = DesktopRate;
    CHECK(FindRenderMode(renderWidth, renderHeight, requestedRefresh).refresh_rate == requestedRefresh);
    haveStandardMode = false;
    CHECK(FindRenderMode(renderWidth, renderHeight, requestedRefresh).pixel_density == 2.0f);
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
        if (name == "render-mode") {
            CheckRenderModes();
            return 0;
        }
        desktop.w = 2560;
        desktop.h = 1440;
        desktop.refresh_rate = DesktopRate;
        desktop.pixel_density = 1.0f;
        if (name == "retina" || name == "retina-only" || name == "wrong-density") {
            desktop.pixel_density = 2.0f;
        }
        offered = desktop;
        offered.refresh_rate = RequestedRate;
        if (name == "retina") {
            lowerDensity = offered;
            lowerDensity.pixel_density = 1.0f;
            haveLowerDensity = true;
        }
        if (name == "wrong-density") {
            offered.pixel_density = 1.0f;
        }
        if (name == "absent") {
            requested = false;
        }
        if (name == "unsupported") {
            offered.refresh_rate = 120.0f;
        }
        if (name == "resolution") {
            offered.w = 1920;
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
            CHECK(modeCalls == 0 && fullscreenCalls == 0 && listCalls == 0);
        } else if (bNeedsFullScreenModeSwitch) {
            CHECK(modeCalls == 0 && fullscreenCalls == 1 && listCalls == 0);
        } else if (name == "supported" || name == "retina" || name == "retina-only") {
            CHECK(!desktopSelected && selected.w == desktop.w && selected.h == desktop.h);
            CHECK(selected.pixel_density == desktop.pixel_density);
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
