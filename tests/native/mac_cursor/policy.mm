#import <Cocoa/Cocoa.h>
#include "native_mouse_window.h"
#include "sdl3_native_cocoa_mouse.h"
#include CSE_KEYBOARD_POLICY_HEADER
#include <cstdio>
#include <cstdlib>
#include <initializer_list>
#include <thread>
#include <dlfcn.h>
#include <cstring>

@interface NSCursor (CseFixtureInvisibleCursor)
+ (NSCursor*)invisibleCursor;
@end

@interface NSApplication (CseFixtureStartupDefaults)
+ (void)registerUserDefaults;
@end

@interface CseCursorPolicyObserver : NSObject
@property(nonatomic) void* nativeWindow;
@property(nonatomic) uint32_t nestedResult;
@property(nonatomic) NSUInteger notifications;
@property(nonatomic, strong) NSWindow* focusTarget;
@end

@implementation CseCursorPolicyObserver
- (void)observeValueForKeyPath:(NSString*)keyPath ofObject:(id)object
                      change:(NSDictionary*)change context:(void*)context {
    (void)keyPath;
    (void)object;
    (void)change;
    (void)context;
    ++_notifications;
    _nestedResult = CseSdl_SetCocoaGameplayCursorV1(CSE_SDL_COCOA_MOUSE_CONTRACT_V1, _nativeWindow, 0);
    if (_focusTarget) {
        [_focusTarget makeKeyAndOrderFront:nil];
    }
}
@end

namespace {
constexpr auto CursorAssistance = NSApplicationPresentationDisableCursorLocationAssistance;

void Require(bool value, const char* message) {
    if (!value) {
        std::fprintf(stderr, "mac-cursor-policy: %s\n", message);
        std::exit(17);
    }
}

const cse::NativeMouseCaptureHooks CaptureHooks{[] {}, [](bool) {}};

void SetCursor(cse::NativeMouseWindow& state, SDL_Window* window, cse::NativeCursorOwner owner,
               bool visible, bool raw) {
    Require(state.RefreshInteraction(true, true, CaptureHooks), "refresh interaction before setting cursor");
    cse::NativeCursorTransaction transaction;
    Require(state.Begin(window, owner, visible, raw, transaction), "begin actual cursor transition");
    Require(transaction.Commit(), "commit actual cursor transition");
}

template <typename Restored>
void WaitForCursorRepair(cse::NativeMouseWindow &state, Restored restored, const char *message) {
    constexpr Uint64 RepairTimeoutMilliseconds = 1000;
    const Uint64 deadline = SDL_GetTicks() + RepairTimeoutMilliseconds;
    do {
        Require(state.RefreshInteraction(true, true, CaptureHooks), "retain active window capture");
        Require(state.RefreshCursorPolicy(), "refresh active cursor policy");
        if (restored()) {
            return;
        }
        SDL_Delay(1);
    } while (SDL_GetTicks() < deadline);
    Require(false, message);
}

void FocusWindow(SDL_Window* window, NSWindow* native) {
    [NSApp activate];
    [native makeKeyAndOrderFront:nil];
    Require(SDL_RaiseWindow(window), "raise owned image fixture");
    constexpr Uint64 FocusDeadlineMilliseconds = 3000;
    const Uint64 deadline = SDL_GetTicks() + FocusDeadlineMilliseconds;
    while (SDL_GetTicks() < deadline &&
        (![NSApp isActive] || [NSApp keyWindow] != native || SDL_GetKeyboardFocus() != window)) {
        SDL_PumpEvents();
        SDL_Delay(1);
    }
    Require([NSApp isActive] && [NSApp keyWindow] == native && SDL_GetKeyboardFocus() == window,
            "fixture owns active key window");
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
    Require(SDL_GetWindowPosition(window, &x, &y) && SDL_GetWindowSize(window, &width, &height),
            "locate the focused fixture window");
    Require(SDL_WarpMouseGlobal(x + width / 2.0f, y + height / 2.0f), "place pointer inside focused window");
    while (SDL_GetTicks() < deadline &&
           [NSWindow windowNumberAtPoint:[NSEvent mouseLocation] belowWindowWithWindowNumber:0] !=
               [native windowNumber]) {
        SDL_PumpEvents();
        SDL_Delay(1);
    }
    Require([NSWindow windowNumberAtPoint:[NSEvent mouseLocation] belowWindowWithWindowNumber:0] ==
            [native windowNumber], "owned content is the frontmost mouse target");
    cse::NativeMouseWindowState().InvalidateInteraction();
    Require(cse::NativeMouseWindowState().RefreshInteraction(true, true, CaptureHooks),
            "admit capture for the focused game window");
}

void CursorImageFixture() {
    float originalX = 0;
    float originalY = 0;
    SDL_GetGlobalMouseState(&originalX, &originalY);
    SDL_Window* window = SDL_CreateWindow("CSE cursor image fixture", 640, 480, 0);
    Require(window != nullptr, "create shown image fixture");
    NSWindow* native = (__bridge NSWindow*)SDL_GetPointerProperty(SDL_GetWindowProperties(window),
        SDL_PROP_WINDOW_COCOA_WINDOW_POINTER, nullptr);
    auto& state = cse::NativeMouseWindowState();
    Require(state.Bind(window), "bind image fixture");
    FocusWindow(window, native);
    for (bool raw : {false, true}) {
        SetCursor(state, window, cse::NativeCursorOwner::VGui2, false, raw);
        const bool visible = SDL_CursorVisible();
        const auto mode = CseSdl_QueryMouseModeV1(CSE_SDL_MOUSE_MODE_CONTRACT_V1);
        const NSPoint point = [NSEvent mouseLocation];
        [[NSCursor arrowCursor] set];
        WaitForCursorRepair(state, [] { return [NSCursor currentCursor] == [NSCursor invisibleCursor]; },
                            "repair stale gameplay cursor image");
        Require(SDL_CursorVisible() == visible &&
                CseSdl_QueryMouseModeV1(CSE_SDL_MOUSE_MODE_CONTRACT_V1) == mode &&
                NSEqualPoints(point, [NSEvent mouseLocation]), "image repair preserves SDL mode/visibility and position");
        SetCursor(state, window, cse::NativeCursorOwner::VGui1, true, raw);
        [[NSCursor arrowCursor] set];
        state.InvalidateInteraction();
        Require(state.RefreshCursorPolicy(), "refresh visible menu intent");
        Require([NSCursor currentCursor] == [NSCursor arrowCursor], "visible intent retains menu cursor image");
    }

    SetCursor(state, window, cse::NativeCursorOwner::VGui2, false, false);
    Require(SDL_SetWindowRelativeMouseMode(window, false) && SDL_ShowCursor(),
            "seed absolute/visible SDL state with stale hidden intent");
    [[NSCursor arrowCursor] set];
    state.InvalidateInteraction();
    Require(state.RefreshCursorPolicy(), "refresh stale hidden intent");
    Require([NSCursor currentCursor] == [NSCursor arrowCursor], "actual SDL visible state wins over stale intent");
    SetCursor(state, window, cse::NativeCursorOwner::VGui2, false, false);

    NSWindow* other = [[NSWindow alloc] initWithContentRect:NSMakeRect(20, 20, 120, 80)
        styleMask:NSWindowStyleMaskTitled backing:NSBackingStoreBuffered defer:NO];
    [other setReleasedWhenClosed:NO];
    [other makeKeyAndOrderFront:nil];
    Require([NSApp keyWindow] == other, "another window owns keyboard focus");
    [[NSCursor arrowCursor] set];
    state.InvalidateInteraction();
    Require(state.RefreshCursorPolicy(), "refresh while another window is key");
    Require([NSCursor currentCursor] == [NSCursor arrowCursor], "do not repair another key window's image");
    FocusWindow(window, native);
    SetCursor(state, window, cse::NativeCursorOwner::VGui2, true, false);
    CseCursorPolicyObserver* focusObserver = [CseCursorPolicyObserver new];
    focusObserver.nativeWindow = (__bridge void*)native;
    focusObserver.focusTarget = other;
    [NSApp addObserver:focusObserver forKeyPath:@"presentationOptions" options:0 context:nullptr];
    [[NSCursor arrowCursor] set];
    SetCursor(state, window, cse::NativeCursorOwner::VGui2, false, false);
    [NSApp removeObserver:focusObserver forKeyPath:@"presentationOptions"];
    Require(focusObserver.notifications > 0 && focusObserver.nestedResult == CseSdlCocoaMouseRejected &&
            [NSApp keyWindow] == other, "synchronous presentation observer changes focus during the busy scope");
    Require([NSCursor currentCursor] == [NSCursor arrowCursor], "recheck focus after presentation notifications");
    [other close];
    FocusWindow(window, native);

    int windowX = 0;
    int windowY = 0;
    Require(SDL_GetWindowPosition(window, &windowX, &windowY), "read owned window position");
    constexpr float OutsideOffset = 40;
    Require(SDL_WarpMouseGlobal(windowX - OutsideOffset, windowY - OutsideOffset), "place pointer outside content");
    [[NSCursor arrowCursor] set];
    state.InvalidateInteraction();
    Require(state.RefreshCursorPolicy(), "refresh with pointer outside content");
    Require([NSCursor currentCursor] == [NSCursor arrowCursor], "do not repair an outside cursor image");
    FocusWindow(window, native);

    const NSRect content = [native convertRectToScreen:[[native contentView] bounds]];
    NSPanel* panel = [[NSPanel alloc] initWithContentRect:content styleMask:NSWindowStyleMaskNonactivatingPanel
        backing:NSBackingStoreBuffered defer:NO];
    [panel setReleasedWhenClosed:NO];
    [panel setLevel:NSFloatingWindowLevel];
    [panel orderFront:nil];
    constexpr Uint64 PanelDeadlineMilliseconds = 3000;
    const Uint64 panelDeadline = SDL_GetTicks() + PanelDeadlineMilliseconds;
    while (SDL_GetTicks() < panelDeadline &&
        [NSWindow windowNumberAtPoint:[NSEvent mouseLocation] belowWindowWithWindowNumber:0] != [panel windowNumber]) {
        SDL_PumpEvents();
        SDL_Delay(1);
    }
    Require([NSApp keyWindow] == native &&
            [NSWindow windowNumberAtPoint:[NSEvent mouseLocation] belowWindowWithWindowNumber:0] ==
            [panel windowNumber], "nonactivating panel covers owned content while original window remains key");
    [[NSCursor arrowCursor] set];
    state.InvalidateInteraction();
    Require(state.RefreshCursorPolicy(), "refresh behind a nonactivating panel");
    Require([NSCursor currentCursor] == [NSCursor arrowCursor], "do not replace an overlapping panel's cursor");
    [panel close];
    FocusWindow(window, native);

    SetCursor(state, window, cse::NativeCursorOwner::VGui2, true, false);
    Require(state.Invalidate(window), "retire image fixture");
    SDL_DestroyWindow(window);
    Require(SDL_WarpMouseGlobal(originalX, originalY), "restore pointer after image fixture");
    std::puts("Actual AppKit cursor image repair and foreign-target guards passed");
}
}

int main(int argc, char** argv) {
    @autoreleasepool {
        Require(SDL_SetHintWithPriority(SDL_HINT_MAC_PRESS_AND_HOLD, "1", SDL_HINT_OVERRIDE),
                "seed accent-picker policy to catch missing override");
        const bool imageFixture = argc == 2 && std::strcmp(argv[1], "--cursor-image") == 0;
        if (argc == 2 && !imageFixture) {
            void* engine = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
            Require(engine != nullptr, "load actual engine and its global UI constructor");
            Require(!SDL_GetHintBoolean(SDL_HINT_MAC_PRESS_AND_HOLD, true),
                    "engine load configures key repeat before any Run/video-mode entry");
            Require(SDL_Init(SDL_INIT_VIDEO), "initialize Cocoa with the engine's keyboard policy");
            Require([[NSApp class] respondsToSelector:@selector(registerUserDefaults)], "actual SDL application");
            [[NSApp class] registerUserDefaults];
            Require(![[NSUserDefaults standardUserDefaults] boolForKey:@"ApplePressAndHoldEnabled"],
                    "actual engine-load Cocoa defaults suppress accent picker");
            SDL_Quit();
            Require(dlclose(engine) == 0, "close test engine");
            std::puts("Actual engine-load keyboard policy passed before Run");
            return 0;
        }
        Require(cse::ConfigureKeyboardMapping(), "configure keyboard before Cocoa initialization");
        Require(SDL_Init(SDL_INIT_VIDEO), "initialize actual SDL Cocoa provider");
        // A hidden command-line fixture may never receive didFinishLaunching.
        // Execute SDL's real registration method, also used by that notification.
        Require([[NSApp class] respondsToSelector:@selector(registerUserDefaults)], "actual SDL application class");
        [[NSApp class] registerUserDefaults];
        Require([[NSUserDefaults standardUserDefaults] objectForKey:@"ApplePressAndHoldEnabled"] != nil &&
                ![[NSUserDefaults standardUserDefaults] boolForKey:@"ApplePressAndHoldEnabled"],
                "held keys repeat without macOS accent picker");
        if (imageFixture) {
            CursorImageFixture();
            SDL_Quit();
            return 0;
        }
        SDL_Window* window = SDL_CreateWindow("CSE hidden cursor fixture", 640, 480, SDL_WINDOW_HIDDEN);
        Require(window != nullptr, "create hidden owned window");
        auto& state = cse::NativeMouseWindowState();
        Require(state.Bind(window), "bind actual window");
        const auto native = SDL_GetPointerProperty(SDL_GetWindowProperties(window),
                                                   SDL_PROP_WINDOW_COCOA_WINDOW_POINTER, nullptr);
        cse::NativeCursorTransaction hidden;
        Require(!state.Begin(window, cse::NativeCursorOwner::VGui1, false, false, hidden),
                "a hidden window must not acquire gameplay capture");
        float originalX = 0.0f;
        float originalY = 0.0f;
        SDL_GetGlobalMouseState(&originalX, &originalY);
        Require(SDL_ShowWindow(window), "show cursor policy fixture");
        FocusWindow(window, (__bridge NSWindow*)native);
        std::thread worker([native]() {
            Require(CseSdl_SetCocoaGameplayCursorV1(CSE_SDL_COCOA_MOUSE_CONTRACT_V1, native, 1) ==
                    CseSdlCocoaMouseWrongThread, "wrong thread rejected before AppKit");
        });
        worker.join();
        const auto baseline = [NSApp presentationOptions];
        Require((baseline & CursorAssistance) == 0, "fresh fixture starts with assistance enabled");
        CseCursorPolicyObserver* observer = [CseCursorPolicyObserver new];
        observer.nativeWindow = native;
        [NSApp addObserver:observer forKeyPath:@"presentationOptions" options:0 context:nullptr];
        for (bool raw : {false, true}) {
            SetCursor(state, window, cse::NativeCursorOwner::VGui1, false, raw);
            if (!raw) {
                [NSApp removeObserver:observer forKeyPath:@"presentationOptions"];
                Require(observer.notifications > 0 && observer.nestedResult == CseSdlCocoaMouseRejected,
                        "actual AppKit notification rejects recursive policy mutation");
            }
            Require(([NSApp presentationOptions] & CursorAssistance) != 0,
                    "gameplay suppresses mouse-finder in both raw and nonraw modes");
            Require(([NSApp presentationOptions] & ~CursorAssistance) == baseline,
                    "gameplay preserves unrelated presentation options");
            constexpr auto unrelated = NSApplicationPresentationAutoHideDock;
            [NSApp setPresentationOptions:baseline | unrelated];
            WaitForCursorRepair(state, [&] {
                return [NSApp presentationOptions] == (baseline | unrelated | CursorAssistance);
            }, "fullscreen-style mask replacement retains current unrelated options");
            SetCursor(state, window, cse::NativeCursorOwner::VGui2, true, raw);
            Require([NSApp presentationOptions] == (baseline | unrelated), "release preserves newer options");
            [NSApp setPresentationOptions:baseline];
            Require([NSApp presentationOptions] == baseline, "menu restores app presentation options");
        }
        {
            cse::NativeCursorTransaction abandoned;
            Require(state.Begin(window, cse::NativeCursorOwner::VGui1, false, false, abandoned), "begin abandoned");
        }
        Require([NSApp presentationOptions] == baseline, "abandoned transition acquires no presentation bit");
        SetCursor(state, window, cse::NativeCursorOwner::VGui1, false, false);
        Require(state.Invalidate(window), "release before teardown");
        Require([NSApp presentationOptions] == baseline, "teardown restores assistance without focus");
        Require(state.Bind(window), "rebind after release");
        Require(state.RefreshInteraction(true, true, CaptureHooks), "admit capture after rebind");
        [NSApp setPresentationOptions:baseline | CursorAssistance];
        SetCursor(state, window, cse::NativeCursorOwner::VGui2, false, true);
        SetCursor(state, window, cse::NativeCursorOwner::VGui1, true, false);
        Require([NSApp presentationOptions] == (baseline | CursorAssistance), "preserve preexisting owned bit");
        Require(state.Invalidate(window), "release preexisting-bit session");
        [NSApp setPresentationOptions:baseline];
        SDL_DestroyWindow(window);
        Require(SDL_WarpMouseGlobal(originalX, originalY), "restore pointer after cursor policy fixture");
        SDL_Quit();
    }
    std::puts("Actual AppKit gameplay/menu/abandoned/teardown cursor policy passed");
    return 0;
}
