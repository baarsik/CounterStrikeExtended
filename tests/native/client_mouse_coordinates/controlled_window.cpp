#include "controlled_window.h"

ControlledWindow controlled;
SDL_Window *ownedWindow = reinterpret_cast<SDL_Window *>(static_cast<std::uintptr_t>(0x12345678));
IVideoMode *videomode = reinterpret_cast<IVideoMode *>(static_cast<std::uintptr_t>(0x23456789));
viddef_t vid{};
static int checks = 0;

void Check(bool condition, const char *expression, int line) {
    ++checks;
    if (!condition) {
        std::fprintf(stderr, "coordinate-check:%d:%s\n", line, expression);
        std::abort();
    }
}

int CheckCount() {
    return checks;
}

void ResetWindow() {
    controlled = ControlledWindow{};
    videomode = reinterpret_cast<IVideoMode *>(static_cast<std::uintptr_t>(0x23456789));
    vid.width = 1;
    vid.height = 3;
}

extern "C" bool SDLCALL SDL_GetWindowSize(SDL_Window *window, int *width, int *height) {
    CHECK(window == ownedWindow);
    ++controlled.sizeReads;
    *width = controlled.windowWidth;
    *height = controlled.windowHeight;
    return controlled.sizeSucceeds;
}

extern "C" bool SDLCALL SDL_GetWindowSizeInPixels(SDL_Window *window, int *width, int *height) {
    CHECK(window == ownedWindow);
    ++controlled.pixelReads;
    *width = controlled.pixelWidth;
    *height = controlled.pixelHeight;
    return controlled.pixelsSucceed;
}

extern "C" SDL_MouseButtonFlags SDLCALL SDL_GetMouseState(float *x, float *y) {
    ++controlled.mouseReads;
    *x = controlled.mouseX;
    *y = controlled.mouseY;
    return 0;
}

extern "C" int VideoMode_IsWindowed() {
    ++controlled.windowedReads;
    CHECK(videomode != nullptr);
    return controlled.windowed ? 1 : 0;
}

extern "C" void VideoMode_GetCurrentVideoMode(int *width, int *height, int *bits) {
    ++controlled.modeReads;
    CHECK(videomode != nullptr && bits == nullptr);
    if (controlled.modeWritesWidth) {
        *width = controlled.renderWidth;
    }
    if (controlled.modeWritesHeight) {
        *height = controlled.renderHeight;
    }
}

extern "C" float GetXMouseAspectRatioAdjustment() {
    ++controlled.aspectXReads;
    return controlled.aspectX;
}

extern "C" float GetYMouseAspectRatioAdjustment() {
    ++controlled.aspectYReads;
    return controlled.aspectY;
}

extern "C" bool SDLCALL SDL_GetWindowPosition(SDL_Window *, int *x, int *y) {
    ++controlled.forbiddenCalls;
    *x = controlled.desktopX;
    *y = controlled.desktopY;
    return true;
}

extern "C" SDL_MouseButtonFlags SDLCALL SDL_GetRelativeMouseState(float *, float *) {
    ++controlled.forbiddenCalls;
    return 0;
}

extern "C" void SDLCALL SDL_PumpEvents() {
    ++controlled.forbiddenCalls;
}

extern "C" void SDLCALL SDL_WarpMouseInWindow(SDL_Window *, float, float) {
    ++controlled.forbiddenCalls;
}

#if INTPTR_MAX > INT32_MAX
namespace cse {
bool TrySampleNativeMousePosition(SDL_Window *window, int width, int height, float *x, float *y) {
    ++controlled.sampleRequests;
    if (!x || !y || window != ownedWindow || !controlled.sampleReady || !controlled.sampleSucceeds ||
        width <= 0 || height <= 0 || width != controlled.windowWidth || height != controlled.windowHeight) {
        return false;
    }
    float resultX = 0.0f;
    float resultY = 0.0f;
    SDL_GetMouseState(&resultX, &resultY);
    if (!std::isfinite(resultX) || !std::isfinite(resultY)) {
        return false;
    }
    *x = resultX;
    *y = resultY;
    return true;
}
} // namespace cse
#endif
