#include "native_mouse_window.h"
#include "sdl3_native_cocoa_mouse.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <limits>
#include <type_traits>

namespace {
uint32_t result = CseSdlCocoaMouseNotReady;
unsigned calls = 0;
void *expectedWindow = nullptr;
int expectedWidth = 0;
int expectedHeight = 0;
float *expectedX = nullptr;
float *expectedY = nullptr;

void Check(bool value, const char *message) {
    if (!value) {
        constexpr int failure = 17;
        std::fprintf(stderr, "cocoa-adapter:%s\n", message);
        std::exit(failure);
    }
}

void Arguments(uint32_t version, void *window) {
    Check(version == CSE_SDL_COCOA_MOUSE_CONTRACT_V1, "exact version forwarded");
    Check(window == expectedWindow, "borrowed window forwarded without narrowing");
    ++calls;
}
} // namespace

extern "C" uint32_t CSE_SDL_NATIVE_MOUSE_CALL CseSdl_CheckCocoaMouseOwnedV1(uint32_t version, void *window) {
    Arguments(version, window);
    return result;
}

extern "C" uint32_t CSE_SDL_NATIVE_MOUSE_CALL
CseSdl_CheckCocoaRelativeFocusV1(uint32_t version, void *window, int32_t width, int32_t height) {
    Arguments(version, window);
    Check(width == expectedWidth && height == expectedHeight, "captured extent forwarded exactly");
    return result;
}

extern "C" uint32_t CSE_SDL_NATIVE_MOUSE_CALL
CseSdl_ReadCocoaMouseAbsoluteV1(uint32_t version, void *window, int32_t width, int32_t height, float *x, float *y) {
    Arguments(version, window);
    Check(width == expectedWidth && height == expectedHeight, "absolute captured extent forwarded exactly");
    Check(x == expectedX && y == expectedY, "caller buffers forwarded without retaining or truncating");
    if (result == CseSdlCocoaMouseReady) {
        *x = -31.5f;
        *y = 701.25f;
    }
    return result;
}

int main() {
    using Absolute = uint32_t (CSE_SDL_NATIVE_MOUSE_CALL *)(uint32_t, void *, int32_t, int32_t, float *, float *);
    static_assert(std::is_same<decltype(&CseSdl_ReadCocoaMouseAbsoluteV1), Absolute>::value,
                  "fixed-width C function prototype");
    const cse::NativeMousePlatform platform = cse::NativeMouseDefaultPlatform();
    Check(std::strcmp(platform.windowProperty, SDL_PROP_WINDOW_COCOA_WINDOW_POINTER) == 0, "Cocoa property retained");
    int token = 0;
    expectedWindow = &token;
    Check(reinterpret_cast<uintptr_t>(expectedWindow) > UINT32_MAX, "native high-address borrowed pointer");
    expectedWidth = (std::numeric_limits<int>::max)();
    expectedHeight = 599;
    struct Guarded {
        uint32_t before = 0xa5a5a5a5U;
        float x = 123.5f;
        float y = 456.5f;
        uint32_t after = 0x5a5a5a5aU;
    } output;
    expectedX = &output.x;
    expectedY = &output.y;
    Check(reinterpret_cast<uintptr_t>(expectedX) > UINT32_MAX, "native high-address caller buffer");
    for (uint32_t status : {0U, 1U, 2U, 3U, 4U, 5U, 6U, 7U, 8U, UINT32_MAX}) {
        result = status;
        calls = 0;
        output.x = 123.5f;
        output.y = 456.5f;
        const bool ready = status == CseSdlCocoaMouseReady;
        Check(platform.ownedWindow(expectedWindow) == ready, "only Ready admits an owned window");
        Check(platform.focusedWindow(expectedWindow, expectedWidth, expectedHeight) == ready,
              "only Ready admits relative focus");
        Check(platform.absolutePosition(expectedWindow, expectedWidth, expectedHeight, expectedX, expectedY) == ready,
              "only Ready admits absolute output");
        Check(calls == 3, "each C entry called exactly once");
        Check(output.x == (ready ? -31.5f : 123.5f) && output.y == (ready ? 701.25f : 456.5f),
              "successful coordinates and failed output identity");
        Check(output.before == 0xa5a5a5a5U && output.after == 0x5a5a5a5aU, "caller canaries retained");
    }
    std::puts("C++ adapter: all statuses, exact forwarding, high buffers and canaries pass");
    return 0;
}
