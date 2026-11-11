// Owned SDL3.4.20+CSE Cocoa mouse service. No upstream source/dynapi changes.
#define CSE_SDL_NATIVE_MOUSE_BRIDGE_BUILD 1
#include "sdl3_native_cocoa_mouse_private.h"
#include "SDL_build_config.h"
#include <SDL3/SDL_version.h>
#include <SDL3/SDL_mouse.h>
#include <cmath>
#include <limits>
#include <chrono>
#include <cstdio>
#include <cstdlib>

@interface NSCursor (CseInvisibleCursorReference)
+ (NSCursor*)invisibleCursor;
@end

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
NSInteger cursorPolicyWindowNumber = 0;
bool cursorPolicyOriginalBit = false;
bool cursorPolicyBusy = false;

void TraceCursorPolicy(NSWindow* window, bool hidden) {
    static const bool enabled = std::getenv("CSE_NATIVE_CURSOR_TRACE") != nullptr;
    if (!enabled) {
        return;
    }
    constexpr auto SampleInterval = std::chrono::milliseconds(10);
    static std::chrono::steady_clock::time_point nextSample;
    const auto now = std::chrono::steady_clock::now();
    if (now < nextSample) {
        return;
    }
    nextSample = now + SampleInterval;
    NSView* view = [window contentView];
    const NSPoint point = [view convertPoint:[window mouseLocationOutsideOfEventStream] fromView:nil];
    const bool active = [NSApp isActive];
    const bool key = [NSApp keyWindow] == window;
    const bool inside = NSPointInRect(point, [view bounds]);
    const bool front = [NSWindow windowNumberAtPoint:[NSEvent mouseLocation]
        belowWindowWithWindowNumber:0] == [window windowNumber];
    const bool visible = SDL_CursorVisible();
    const bool transparent = [NSCursor currentCursor] == [NSCursor invisibleCursor];
    const auto mode = CseSdl_QueryMouseModeV1(CSE_SDL_MOUSE_MODE_CONTRACT_V1);
    constexpr unsigned int HiddenIntent = 1U << 0;
    constexpr unsigned int Active = 1U << 1;
    constexpr unsigned int Key = 1U << 2;
    constexpr unsigned int Inside = 1U << 3;
    constexpr unsigned int Front = 1U << 4;
    constexpr unsigned int Visible = 1U << 5;
    constexpr unsigned int Transparent = 1U << 6;
    constexpr unsigned int Relative = 1U << 7;
    const unsigned int state = (hidden ? HiddenIntent : 0) | (active ? Active : 0) | (key ? Key : 0) |
        (inside ? Inside : 0) | (front ? Front : 0) | (visible ? Visible : 0) |
        (transparent ? Transparent : 0) | (mode == CseSdlMouseModeRelative ? Relative : 0);
    constexpr unsigned int Unobserved = (std::numeric_limits<unsigned int>::max)();
    static unsigned int previous = Unobserved;
    if (state != previous) {
        std::fprintf(stderr, "cse_cursor hidden=%d active=%d key=%d inside=%d front=%d sdl_visible=%d "
            "transparent=%d mode=%u point=%.1f,%.1f\n", hidden, active, key, inside, front, visible,
            transparent, mode, point.x, point.y);
        previous = state;
    }
}
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

NSPoint ScreenPoint() {
    return [NSEvent mouseLocation];
}

NSPoint PointFromScreen(NSWindow *window, NSPoint point) {
    return [window convertPointFromScreen:point];
}

NSInteger InteractionWindowNumber(NSPoint point) {
    // Public hit testing skips windows that ignore mouse events, including
    // transparent pass-through overlays; visual occlusion alone is not ownership.
    return [NSWindow windowNumberAtPoint:point belowWindowWithWindowNumber:0];
}

NativeMouseCocoaInteractionServices DefaultInteractionServices() {
    return {ScreenPoint, PointFromScreen, InteractionWindowNumber};
}

NativeMouseCocoaServices DefaultServices() {
    return {MainThread, WindowNumber, NumberWindow, ContentView, ViewWindow,   ApplicationActive, KeyWindow,
            IsKey,      Miniaturized, Bounds,       Flipped,     OutsidePoint, ConvertPoint};
}

void RepairGameplayCursorImage(NSWindow* window) {
    if (CheckCocoaMouseOwnedV1(DefaultServices(), CSE_SDL_COCOA_MOUSE_CONTRACT_V1,
        (__bridge void*)window) != CseSdlCocoaMouseReady || ![NSApp isActive] || [NSApp keyWindow] != window) {
        return;
    }
    const auto mode = CseSdl_QueryMouseModeV1(CSE_SDL_MOUSE_MODE_CONTRACT_V1);
    if (mode != CseSdlMouseModeRelative && SDL_CursorVisible()) {
        return;
    }
    NSCursor* desired = [NSCursor invisibleCursor];
    if ([NSCursor currentCursor] == desired) {
        return;
    }
    NSView* content = [window contentView];
    const NSPoint screenPoint = [NSEvent mouseLocation];
    const NSPoint localPoint = [content convertPoint:[window convertPointFromScreen:screenPoint] fromView:nil];
    if (!NSPointInRect(localPoint, [content bounds]) ||
        [NSWindow windowNumberAtPoint:screenPoint belowWindowWithWindowNumber:0] != [window windowNumber]) {
        return;
    }
    if ([NSCursor currentCursor] != desired) {
        // Repair a stale native image without acquiring an AppKit hide count or
        // changing SDL visibility, relative mode, cursor stacks or pointer position.
        [desired set];
    }
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

uint32_t CheckCocoaInteractionOwnerV1(const NativeMouseCocoaServices &services,
    const NativeMouseCocoaInteractionServices &interaction, uint32_t version, void *window) {
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
    if (!Complete(services) || !interaction.screenPoint || !interaction.pointFromScreen ||
        !interaction.interactionWindowNumber) {
        return CseSdlCocoaMouseRejected;
    }
    NSWindow *nativeWindow = (__bridge NSWindow *)window;
    NSView *view = nil;
    if (!Owned(services, nativeWindow, &view)) {
        return CseSdlCocoaMouseRejected;
    }
    const NSRect bounds = services.bounds(view);
    if (!FiniteCoordinate(bounds.origin.x) || !FiniteCoordinate(bounds.origin.y) ||
        !FiniteCoordinate(bounds.size.width) || !FiniteCoordinate(bounds.size.height) ||
        bounds.size.width <= 0 || bounds.size.height <= 0) {
        return CseSdlCocoaMouseRejected;
    }
    const NSPoint screenPoint = interaction.screenPoint();
    if (!FiniteCoordinate(screenPoint.x) || !FiniteCoordinate(screenPoint.y)) {
        return CseSdlCocoaMouseRejected;
    }
    const NSPoint basePoint = interaction.pointFromScreen(nativeWindow, screenPoint);
    if (!FiniteCoordinate(basePoint.x) || !FiniteCoordinate(basePoint.y)) {
        return CseSdlCocoaMouseRejected;
    }
    const NSPoint localPoint = services.convertPoint(view, basePoint);
    if (!FiniteCoordinate(localPoint.x) || !FiniteCoordinate(localPoint.y)) {
        return CseSdlCocoaMouseRejected;
    }
    if (!services.applicationActive() || services.keyWindow() != nativeWindow || !services.isKey(nativeWindow) ||
        services.miniaturized(nativeWindow)) {
        return CseSdlCocoaMouseNotInteractionOwner;
    }
    if (!NSPointInRect(localPoint, bounds) ||
        interaction.interactionWindowNumber(screenPoint) != services.windowNumber(nativeWindow)) {
        return CseSdlCocoaMouseNotInteractionOwner;
    }
    return CseSdlCocoaMouseReady;
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
CseSdl_CheckCocoaInteractionOwnerV1(uint32_t version, void *window) {
    return cse::CheckCocoaInteractionOwnerV1(cse::DefaultServices(), cse::DefaultInteractionServices(), version, window);
}

extern "C" uint32_t CSE_SDL_NATIVE_MOUSE_CALL
CseSdl_CheckCocoaRelativeFocusV1(uint32_t version, void *window, int32_t width, int32_t height) {
    return cse::CheckCocoaRelativeFocusV1(cse::DefaultServices(), version, window, width, height);
}

extern "C" uint32_t CSE_SDL_NATIVE_MOUSE_CALL
CseSdl_ReadCocoaMouseAbsoluteV1(uint32_t version, void *window, int32_t width, int32_t height, float *x, float *y) {
    return cse::ReadCocoaMouseAbsoluteV1(cse::DefaultServices(), version, window, width, height, x, y);
}

extern "C" uint32_t CSE_SDL_NATIVE_MOUSE_CALL
CseSdl_SetCocoaGameplayCursorV1(uint32_t version, void *window, uint32_t hidden) {
    if (hidden > 1) {
        return CseSdlCocoaMouseInvalidArgument;
    }
    const uint32_t status = cse::CheckCocoaMouseOwnedV1(cse::DefaultServices(), version, window);
    if (status != CseSdlCocoaMouseReady) {
        return status;
    }
    if (cse::cursorPolicyBusy) {
        return CseSdlCocoaMouseRejected;
    }
    NSWindow* const nativeWindow = (__bridge NSWindow*)window;
    cse::TraceCursorPolicy(nativeWindow, hidden != 0);
    const NSInteger number = [nativeWindow windowNumber];
    if (cse::cursorPolicyWindowNumber != 0 && cse::cursorPolicyWindowNumber != number) {
        return CseSdlCocoaMouseRejected;
    }
    if (!hidden && cse::cursorPolicyWindowNumber == 0) {
        return CseSdlCocoaMouseReady;
    }
    constexpr auto assistance = NSApplicationPresentationDisableCursorLocationAssistance;
    const auto current = [NSApp presentationOptions];
    const bool original = cse::cursorPolicyWindowNumber == 0 ? (current & assistance) != 0
                                                           : cse::cursorPolicyOriginalBit;
    const auto requested = hidden || original ? current | assistance : current & ~assistance;
    // Reserve cleanup ownership before AppKit can issue synchronous notifications.
    // An exception retains that ownership so a later visible request can release.
    if (hidden) {
        cse::cursorPolicyWindowNumber = number;
        cse::cursorPolicyOriginalBit = original;
    }
    struct CallScope {
        CallScope() {
            cse::cursorPolicyBusy = true;
        }
        ~CallScope() {
            cse::cursorPolicyBusy = false;
        }
    } callScope;
    @try {
        if (requested != current) {
            [NSApp setPresentationOptions:requested];
        }
        if ([NSApp presentationOptions] != requested) {
            return CseSdlCocoaMouseRejected;
        }
        if (hidden) {
            cse::RepairGameplayCursorImage(nativeWindow);
        }
    } @catch (NSException *exception) {
        (void)exception;
        return CseSdlCocoaMouseRejected;
    }
    if (!hidden) {
        cse::cursorPolicyWindowNumber = 0;
        cse::cursorPolicyOriginalBit = false;
    }
    return CseSdlCocoaMouseReady;
}

// Additive keyboard policy. Keep all pre-existing mouse bodies above unchanged.
#include "sdl3_native_cocoa_keyboard_private.h"
#include <SDL3/SDL_video.h>
#include <Carbon/Carbon.h>

namespace cse {
namespace {
CocoaGameplayShortcutState shortcutPolicy;

bool ShortcutServicesComplete(const CocoaGameplayShortcutServices &s) {
    return s.windowId && s.windowForId && s.installMonitor && s.removeMonitor && s.eventType &&
        s.keyCode && s.modifiers && s.eventWindow;
}

uintptr_t ShortcutIdentity(NSWindow *window) {
    return reinterpret_cast<uintptr_t>((__bridge void *)window);
}

NSWindow *ShortcutWindowForId(uint32_t id) {
    SDL_Window *window = SDL_GetWindowFromID(id);
    if (!window) {
        return nil;
    }
    return (__bridge NSWindow *)SDL_GetPointerProperty(SDL_GetWindowProperties(window),
        SDL_PROP_WINDOW_COCOA_WINDOW_POINTER, nullptr);
}

uint32_t ShortcutWindowId(NSWindow *native) {
    int count = 0;
    SDL_Window **windows = SDL_GetWindows(&count);
    uint32_t result = 0;
    for (int index = 0; windows && index < count; ++index) {
        if (SDL_GetPointerProperty(SDL_GetWindowProperties(windows[index]),
            SDL_PROP_WINDOW_COCOA_WINDOW_POINTER, nullptr) == (__bridge void *)native) {
            result = SDL_GetWindowID(windows[index]);
            break;
        }
    }
    SDL_free(windows);
    return result;
}

CocoaGameplayShortcutServices DefaultShortcutServices();

id InstallShortcutMonitor() {
    return [NSEvent addLocalMonitorForEventsMatchingMask:NSEventMaskKeyDown handler:^NSEvent *(NSEvent *event) {
        return FilterCocoaGameplayShortcut(DefaultServices(), DefaultShortcutServices(), shortcutPolicy, event);
    }];
}

bool RemoveShortcutMonitor(id token) {
    [NSEvent removeMonitor:token];
    return true;
}

NSEventType ShortcutEventType(NSEvent *event) {
    return [event type];
}

unsigned short ShortcutKeyCode(NSEvent *event) {
    return [event keyCode];
}

NSEventModifierFlags ShortcutModifiers(NSEvent *event) {
    return [event modifierFlags];
}

NSWindow *ShortcutEventWindow(NSEvent *event) {
    return [event window];
}

CocoaGameplayShortcutServices DefaultShortcutServices() {
    return {ShortcutWindowId, ShortcutWindowForId, InstallShortcutMonitor, RemoveShortcutMonitor,
        ShortcutEventType, ShortcutKeyCode, ShortcutModifiers, ShortcutEventWindow};
}

bool ShortcutCurrent(const NativeMouseCocoaServices &native, const CocoaGameplayShortcutServices &shortcuts,
    const CocoaGameplayShortcutState &state, NSWindow *window) {
    if (!state.windowId || ShortcutIdentity(window) != state.nativeIdentity ||
        shortcuts.windowForId(state.windowId) != window) {
        return false;
    }
    NSView *view = nil;
    return Owned(native, window, &view) && native.windowNumber(window) == state.windowNumber;
}

void ClearShortcutOwner(CocoaGameplayShortcutState &state) {
    state.windowId = 0;
    state.nativeIdentity = 0;
    state.windowNumber = 0;
    state.enabled = false;
}

bool RemoveShortcutToken(const CocoaGameplayShortcutServices &shortcuts, CocoaGameplayShortcutState &state) {
    if (state.monitor) {
        if (!shortcuts.removeMonitor(state.monitor)) {
            return false;
        }
        state.monitor = nil;
    }
    return true;
}
} // namespace

uint32_t SetCocoaGameplayShortcutsV1(const NativeMouseCocoaServices &native,
    const CocoaGameplayShortcutServices &shortcuts, CocoaGameplayShortcutState &state,
    uint32_t version, void *window, uint32_t enabled) {
    if (version != CSE_SDL_COCOA_KEYBOARD_CONTRACT_V1) {
        return CseSdlCocoaMouseContractMismatch;
    }
    if (!window || enabled > 1) {
        return CseSdlCocoaMouseInvalidArgument;
    }
    // Reject foreign threads before accessing the main-thread-only policy state.
    const uint32_t status = Guard(native, CSE_SDL_COCOA_MOUSE_CONTRACT_V1, AnyMouseMode);
    if (status != CseSdlCocoaMouseReady) {
        return status;
    }
    const uintptr_t identity = reinterpret_cast<uintptr_t>(window);
    if (state.busy) {
        // A pending request cannot invalidate a different installed owner.
        // Ownerless admission still permits cancellation of its pending install.
        if (!enabled && state.requestIdentity == identity &&
            (!state.windowId || state.nativeIdentity == identity)) {
            state.enabled = false;
            ++state.generation;
        }
        return CseSdlCocoaMouseRejected;
    }
    if (!Complete(native) || !ShortcutServicesComplete(shortcuts)) {
        return CseSdlCocoaMouseRejected;
    }
    struct Transaction {
        CocoaGameplayShortcutState &state;
        Transaction(CocoaGameplayShortcutState &value, uintptr_t request) : state(value) {
            state.busy = true;
            state.requestIdentity = request;
        }
        ~Transaction() {
            state.requestIdentity = 0;
            state.busy = false;
        }
    } transaction(state, identity);
    const uint64_t generation = state.generation;
    @try {
        NSWindow *nativeWindow = (__bridge NSWindow *)window;
        NSView *view = nil;
        if (!Owned(native, nativeWindow, &view)) {
            return CseSdlCocoaMouseRejected;
        }
        const uint32_t id = shortcuts.windowId(nativeWindow);
        const NSInteger number = native.windowNumber(nativeWindow);
        if (!id || shortcuts.windowForId(id) != nativeWindow || generation != state.generation) {
            return CseSdlCocoaMouseRejected;
        }
        if (state.windowId && (state.windowId != id || state.nativeIdentity != identity ||
            state.windowNumber != number)) {
            return CseSdlCocoaMouseRejected;
        }
        if (!enabled) {
            state.enabled = false;
            ++state.generation;
            if (!RemoveShortcutToken(shortcuts, state) || state.callbackDepth) {
                return CseSdlCocoaMouseRejected;
            }
            ClearShortcutOwner(state);
            return CseSdlCocoaMouseReady;
        }
        if (state.windowId) {
            return state.enabled && state.monitor ? CseSdlCocoaMouseReady : CseSdlCocoaMouseRejected;
        }
        state.windowId = id;
        state.nativeIdentity = identity;
        state.windowNumber = number;
        state.monitor = shortcuts.installMonitor();
        if (!state.monitor) {
            ClearShortcutOwner(state);
            return CseSdlCocoaMouseRejected;
        }
        const bool current = ShortcutCurrent(native, shortcuts, state, nativeWindow);
        if (!current || generation != state.generation) {
            // Even after successful removal, preserve cleanup ownership for an
            // explicit retry. A nested retirement request must not be lost.
            RemoveShortcutToken(shortcuts, state);
            return CseSdlCocoaMouseRejected;
        }
        state.enabled = true;
        return CseSdlCocoaMouseReady;
    } @catch (NSException *exception) {
        (void)exception;
        state.enabled = false;
        ++state.generation;
        return CseSdlCocoaMouseRejected;
    }
}

NSEvent *FilterCocoaGameplayShortcut(const NativeMouseCocoaServices &native,
    const CocoaGameplayShortcutServices &shortcuts, CocoaGameplayShortcutState &state, NSEvent *event) {
    // The monitor is main-thread-only. Keep the admission count live across all
    // SDL/AppKit calls, including callbacks nested by native notifications.
    struct Callback {
        CocoaGameplayShortcutState &state;
        explicit Callback(CocoaGameplayShortcutState &value) : state(value) {
            ++state.callbackDepth;
        }
        ~Callback() {
            --state.callbackDepth;
        }
    } callback(state);
    const uint64_t generation = state.generation;
    @try {
        if (!event || !state.enabled || !state.monitor || state.busy || !Complete(native) ||
            !ShortcutServicesComplete(shortcuts) ||
            Guard(native, CSE_SDL_COCOA_MOUSE_CONTRACT_V1, AnyMouseMode) != CseSdlCocoaMouseReady ||
            shortcuts.eventType(event) != NSEventTypeKeyDown || shortcuts.keyCode(event) != kVK_Space) {
            return event;
        }
        const NSEventModifierFlags flags = shortcuts.modifiers(event);
        constexpr NSEventModifierFlags ExcludedModifiers = NSEventModifierFlagCommand | NSEventModifierFlagOption;
        if (!(flags & NSEventModifierFlagControl) || (flags & ExcludedModifiers)) {
            return event;
        }
        NSWindow *window = shortcuts.eventWindow(event);
        if (!ShortcutCurrent(native, shortcuts, state, window) || !native.applicationActive() ||
            native.keyWindow() != window || !native.isKey(window) || native.miniaturized(window)) {
            return event;
        }
        // Native reads may synchronously invalidate or replace ownership. Check
        // current SDL/property identity once more, then generation without a callout.
        const bool current = ShortcutCurrent(native, shortcuts, state, window);
        return current && generation == state.generation && state.enabled && state.monitor && !state.busy
            ? nil : event;
    } @catch (NSException *exception) {
        (void)exception;
        return event;
    }
}
} // namespace cse

extern "C" uint32_t CSE_SDL_NATIVE_MOUSE_CALL
CseSdl_SetCocoaGameplayShortcutsV1(uint32_t version, void *window, uint32_t enabled) {
    return cse::SetCocoaGameplayShortcutsV1(cse::DefaultServices(), cse::DefaultShortcutServices(),
        cse::shortcutPolicy, version, window, enabled);
}
