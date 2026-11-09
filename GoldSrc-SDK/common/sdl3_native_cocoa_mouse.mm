// Owned SDL3.4.20+CSE Cocoa mouse service. No upstream source/dynapi changes.
#define CSE_SDL_NATIVE_MOUSE_BRIDGE_BUILD 1
#include "sdl3_native_cocoa_mouse_private.h"
#include "SDL_build_config.h"
#include <SDL3/SDL_version.h>
#include <cmath>
#include <limits>

#if SDL_MAJOR_VERSION != 3 || SDL_MINOR_VERSION != 4 || SDL_MICRO_VERSION != 20
#error Requalify the Cocoa mouse service for the selected SDK
#endif
#if !defined(SDL_PLATFORM_MACOS) || !SDL_VIDEO_DRIVER_COCOA
#error The qualified Cocoa mouse service requires native macOS Cocoa
#endif
static_assert(sizeof(void *) == sizeof(uint64_t), "Cocoa mouse provider is native64");
static_assert(sizeof(float) == sizeof(uint32_t) && std::numeric_limits<float>::is_iec559,
              "Cocoa mouse C ABI requires IEEE binary32");
static_assert(std::numeric_limits<int>::digits == 31, "Cocoa mouse extents require signed32 int");

namespace cse {
namespace {
constexpr uint32_t AnyMouseMode = 0;
uint32_t Guard(const NativeMouseCocoaServices &services, uint32_t version, uint32_t requiredMode) {
    if (version != CSE_SDL_COCOA_MOUSE_CONTRACT_V1) {
        return CseSdlCocoaMouseContractMismatch;
    }
    const uint32_t mode = CseSdl_QueryMouseModeV1(CSE_SDL_MOUSE_MODE_CONTRACT_V1);
    switch (mode) {
    case CseSdlMouseModeNotReady:
        return CseSdlCocoaMouseNotReady;
    case CseSdlMouseModeWrongThread:
        return CseSdlCocoaMouseWrongThread;
    case CseSdlMouseModeContractMismatch:
        return CseSdlCocoaMouseContractMismatch;
    case CseSdlMouseModeForeignDispatch:
        return CseSdlCocoaMouseForeignDispatch;
    case CseSdlMouseModeAbsolute:
    case CseSdlMouseModeRelative:
        break;
    default:
        return CseSdlCocoaMouseNotReady;
    }
    if (requiredMode != AnyMouseMode && mode != requiredMode) {
        return CseSdlCocoaMouseWrongMode;
    }
    if (!services.mainThread || !services.mainThread()) {
        return CseSdlCocoaMouseWrongThread;
    }
    return CseSdlCocoaMouseReady;
}
bool Complete(const NativeMouseCocoaServices &s) {
    return s.mainThread && s.windowNumber && s.numberWindow && s.contentView && s.viewWindow && s.applicationActive &&
           s.keyWindow && s.isKey && s.miniaturized && s.bounds && s.flipped && s.outsidePoint && s.convertPoint;
}

bool Owned(const NativeMouseCocoaServices &s, NSWindow *window, NSView **content) {
    if (!window || !s.mainThread()) {
        return false;
    }
    const NSInteger number = s.windowNumber(window);
    if (number <= 0 || s.numberWindow(number) != window) {
        return false;
    }
    NSView *view = s.contentView(window);
    if (!view || s.viewWindow(view) != window) {
        return false;
    }
    *content = view;
    return true;
}

bool FiniteCoordinate(double value) {
    constexpr double maximumCoordinate = static_cast<double>((std::numeric_limits<float>::max)());
    return std::isfinite(value) && value >= -maximumCoordinate && value <= maximumCoordinate;
}

bool Focused(const NativeMouseCocoaServices &s, NSWindow *window, int width, int height, NSView **view,
             NSRect *bounds) {
    if (width <= 0 || height <= 0 || !Owned(s, window, view) || !s.applicationActive() || s.keyWindow() != window ||
        !s.isKey(window) || s.miniaturized(window)) {
        return false;
    }
    const NSRect rect = s.bounds(*view);
    if (!FiniteCoordinate(rect.origin.x) || !FiniteCoordinate(rect.origin.y) || !std::isfinite(rect.size.width) ||
        !std::isfinite(rect.size.height) || rect.size.width != static_cast<double>(width) ||
        rect.size.height != static_cast<double>(height)) {
        return false;
    }
    *bounds = rect;
    return true;
}

bool MainThread() {
    return [NSThread isMainThread];
}
NSInteger WindowNumber(NSWindow *window) {
    return [window windowNumber];
}
NSWindow *NumberWindow(NSInteger number) {
    return [NSApp windowWithWindowNumber:number];
}
NSView *ContentView(NSWindow *window) {
    return [window contentView];
}
NSWindow *ViewWindow(NSView *view) {
    return [view window];
}
bool ApplicationActive() {
    return [NSApp isActive];
}
NSWindow *KeyWindow() {
    return [NSApp keyWindow];
}
bool IsKey(NSWindow *window) {
    return [window isKeyWindow];
}
bool Miniaturized(NSWindow *window) {
    return [window isMiniaturized];
}
NSRect Bounds(NSView *view) {
    return [view bounds];
}
bool Flipped(NSView *view) {
    return [view isFlipped];
}
NSPoint OutsidePoint(NSWindow *window) {
    return [window mouseLocationOutsideOfEventStream];
}
NSPoint ConvertPoint(NSView *view, NSPoint point) {
    return [view convertPoint:point fromView:nil];
}

NativeMouseCocoaServices DefaultServices() {
    return {MainThread, WindowNumber, NumberWindow, ContentView, ViewWindow,   ApplicationActive, KeyWindow,
            IsKey,      Miniaturized, Bounds,       Flipped,     OutsidePoint, ConvertPoint};
}
} // namespace

bool OwnsNativeMouseCocoa(const NativeMouseCocoaServices &s, NSWindow *window) {
    NSView *view = nil;
    return Complete(s) && Owned(s, window, &view);
}

bool FocusesNativeMouseCocoa(const NativeMouseCocoaServices &s, NSWindow *window, int width, int height) {
    NSView *view = nil;
    NSRect bounds{};
    return Complete(s) && Focused(s, window, width, height, &view, &bounds);
}

bool ReadNativeMouseCocoa(const NativeMouseCocoaServices &s, NSWindow *window, int width, int height, float *x,
                          float *y) {
    if (!x || !y || !Complete(s)) {
        return false;
    }
    NSView *view = nil;
    NSRect bounds{};
    if (!Focused(s, window, width, height, &view, &bounds)) {
        return false;
    }
    const NSPoint outside = s.outsidePoint(window);
    if (!FiniteCoordinate(outside.x) || !FiniteCoordinate(outside.y)) {
        return false;
    }
    const NSPoint point = s.convertPoint(view, outside);
    const double relativeX = point.x - bounds.origin.x;
    const double relativeY = point.y - bounds.origin.y;
    const double topY = s.flipped(view) ? relativeY : bounds.size.height - relativeY;
    if (!FiniteCoordinate(point.x) || !FiniteCoordinate(point.y) || !FiniteCoordinate(relativeX) ||
        !FiniteCoordinate(topY)) {
        return false;
    }
    const float resultX = static_cast<float>(relativeX);
    const float resultY = static_cast<float>(topY);
    *x = resultX;
    *y = resultY;
    return true;
}

uint32_t CheckCocoaMouseOwnedV1(const NativeMouseCocoaServices &services, uint32_t version, void *window) {
    if (version != CSE_SDL_COCOA_MOUSE_CONTRACT_V1) {
        return CseSdlCocoaMouseContractMismatch;
    }
    if (!window) {
        return CseSdlCocoaMouseInvalidArgument;
    }
    const uint32_t status = Guard(services, version, AnyMouseMode);
    if (status != CseSdlCocoaMouseReady) {
        return status;
    }
    return OwnsNativeMouseCocoa(services, (__bridge NSWindow *)window) ? CseSdlCocoaMouseReady
                                                                   : CseSdlCocoaMouseRejected;
}

uint32_t CheckCocoaRelativeFocusV1(const NativeMouseCocoaServices &services, uint32_t version, void *window,
                                  int32_t width, int32_t height) {
    if (version != CSE_SDL_COCOA_MOUSE_CONTRACT_V1) {
        return CseSdlCocoaMouseContractMismatch;
    }
    if (!window || width <= 0 || height <= 0) {
        return CseSdlCocoaMouseInvalidArgument;
    }
    const uint32_t status = Guard(services, version, CseSdlMouseModeRelative);
    if (status != CseSdlCocoaMouseReady) {
        return status;
    }
    return FocusesNativeMouseCocoa(services, (__bridge NSWindow *)window, width, height)
               ? CseSdlCocoaMouseReady : CseSdlCocoaMouseRejected;
}

uint32_t ReadCocoaMouseAbsoluteV1(const NativeMouseCocoaServices &services, uint32_t version, void *window,
                                 int32_t width, int32_t height, float *x, float *y) {
    if (version != CSE_SDL_COCOA_MOUSE_CONTRACT_V1) {
        return CseSdlCocoaMouseContractMismatch;
    }
    if (!window || width <= 0 || height <= 0 || !x || !y) {
        return CseSdlCocoaMouseInvalidArgument;
    }
    const uint32_t status = Guard(services, version, CseSdlMouseModeAbsolute);
    if (status != CseSdlCocoaMouseReady) {
        return status;
    }
    return ReadNativeMouseCocoa(services, (__bridge NSWindow *)window, width, height, x, y)
               ? CseSdlCocoaMouseReady : CseSdlCocoaMouseRejected;
}
} // namespace cse

extern "C" uint32_t CSE_SDL_NATIVE_MOUSE_CALL
CseSdl_CheckCocoaMouseOwnedV1(uint32_t version, void *window) {
    return cse::CheckCocoaMouseOwnedV1(cse::DefaultServices(), version, window);
}

extern "C" uint32_t CSE_SDL_NATIVE_MOUSE_CALL
CseSdl_CheckCocoaRelativeFocusV1(uint32_t version, void *window, int32_t width, int32_t height) {
    return cse::CheckCocoaRelativeFocusV1(cse::DefaultServices(), version, window, width, height);
}

extern "C" uint32_t CSE_SDL_NATIVE_MOUSE_CALL
CseSdl_ReadCocoaMouseAbsoluteV1(uint32_t version, void *window, int32_t width, int32_t height, float *x, float *y) {
    return cse::ReadCocoaMouseAbsoluteV1(cse::DefaultServices(), version, window, width, height, x, y);
}
