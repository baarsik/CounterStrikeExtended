#include "sdl3_native_cocoa_mouse_private.h"
#include <SDL3/SDL_mouse.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <limits>

namespace {
constexpr NSInteger OwnedNumber = 31;
constexpr NSInteger ForeignNumber = 47;
constexpr uint32_t NotInteractionOwner = 8;
constexpr uint32_t Contract = CSE_SDL_COCOA_MOUSE_CONTRACT_V1;
int checks = 0;
uint32_t mode = CseSdlMouseModeAbsolute;
int modeReads = 0;
NSWindow *window;
NSWindow *foreign;
NSView *view;

void Check(bool value, const char *expression, int line) {
    ++checks;
    if (!value) {
        std::fprintf(stderr, "interaction-check:%d:%s\n", line, expression);
        std::exit(17);
    }
}
#define CHECK(value) Check((value), #value, __LINE__)

struct Scene {
    bool main = true;
    bool owned = true;
    bool associated = true;
    bool hasView = true;
    bool active = true;
    bool keyWindow = true;
    bool isKey = true;
    bool minimized = false;
    NSInteger number = OwnedNumber;
    NSInteger hit = OwnedNumber;
    NSRect bounds = NSMakeRect(10, 20, 640, 480);
    NSPoint screen = NSMakePoint(100, 200);
    NSPoint base = NSMakePoint(100, 200);
    NSPoint local = NSMakePoint(50, 60);
    int nativeReads = 0;
    int screenReads = 0;
    int fromScreenReads = 0;
    int conversions = 0;
    int hits = 0;
};
Scene scene;

bool Main() {
    return scene.main;
}
NSInteger Number(NSWindow *value) {
    CHECK(value == window);
    ++scene.nativeReads;
    return scene.number;
}
NSWindow *NumberWindow(NSInteger number) {
    CHECK(number == scene.number);
    return scene.owned ? window : foreign;
}
NSView *Content(NSWindow *value) {
    CHECK(value == window);
    return scene.hasView ? view : nil;
}
NSWindow *ViewWindow(NSView *value) {
    CHECK(value == view);
    return scene.associated ? window : foreign;
}
bool Active() {
    return scene.active;
}
NSWindow *KeyWindow() {
    return scene.keyWindow ? window : foreign;
}
bool IsKey(NSWindow *value) {
    CHECK(value == window);
    return scene.isKey;
}
bool Minimized(NSWindow *value) {
    CHECK(value == window);
    return scene.minimized;
}
NSRect Bounds(NSView *value) {
    CHECK(value == view);
    return scene.bounds;
}
bool Flipped(NSView *) {
    CHECK(false); // Interaction ownership never samples the old coordinate path.
    return false;
}
NSPoint Outside(NSWindow *) {
    CHECK(false);
    return NSZeroPoint;
}
NSPoint Convert(NSView *value, NSPoint point) {
    CHECK(value == view);
    CHECK(point.x == scene.base.x && point.y == scene.base.y);
    ++scene.conversions;
    return scene.local;
}
NSPoint Screen() {
    ++scene.screenReads;
    return scene.screen;
}
NSPoint FromScreen(NSWindow *value, NSPoint point) {
    CHECK(value == window);
    CHECK(point.x == scene.screen.x && point.y == scene.screen.y);
    ++scene.fromScreenReads;
    return scene.base;
}
NSInteger Hit(NSPoint point) {
    CHECK(point.x == scene.screen.x && point.y == scene.screen.y);
    ++scene.hits;
    return scene.hit;
}
cse::NativeMouseCocoaServices Services() {
    return {Main, Number, NumberWindow, Content, ViewWindow, Active, KeyWindow,
        IsKey, Minimized, Bounds, Flipped, Outside, Convert};
}

#ifdef CSE_ORIGINAL_OWNER_BOUNDARY
// The original API validates window identity only. This diagnostic deliberately
// asks it for the new distinction; it does not claim the baseline had a new API.
uint32_t Query() {
    return cse::CheckCocoaMouseOwnedV1(Services(), Contract, (__bridge void *)window);
}
#else
cse::NativeMouseCocoaInteractionServices Interaction() {
    return {Screen, FromScreen, Hit};
}
uint32_t Query() {
    return cse::CheckCocoaInteractionOwnerV1(Services(), Interaction(), Contract, (__bridge void *)window);
}
#endif

void ForeignBoundary() {
    scene = Scene{};
    scene.hit = ForeignNumber;
    const auto actual = Query();
    std::printf("mode=%u active=%d key=%d hit=%ld result=%u expected=%u\n",
        mode, scene.active, scene.isKey, static_cast<long>(scene.hit), actual, NotInteractionOwner);
    CHECK(actual == NotInteractionOwner);
}

#ifndef CSE_ORIGINAL_OWNER_BOUNDARY
void Expected(uint32_t expected) {
    CHECK(Query() == expected);
}

void CombinedMalformedCases() {
    constexpr int InteractionStates = 4;
    constexpr int MalformedReads = 12;
    const double infinity = (std::numeric_limits<double>::infinity)();
    const double nan = (std::numeric_limits<double>::quiet_NaN)();
    for (int state = 0; state < InteractionStates; ++state) {
        for (int malformed = 0; malformed < MalformedReads; ++malformed) {
            scene = Scene{};
            scene.hit = ForeignNumber;
            switch (state) {
            case 0:
                scene.active = false;
                break;
            case 1:
                scene.keyWindow = false;
                break;
            case 2:
                scene.isKey = false;
                break;
            case 3:
                scene.minimized = true;
                break;
            }
            switch (malformed) {
            case 0:
                scene.bounds.origin.x = nan;
                break;
            case 1:
                scene.bounds.origin.y = infinity;
                break;
            case 2:
                scene.bounds.size.width = 0;
                break;
            case 3:
                scene.bounds.size.height = -1;
                break;
            case 4:
                scene.bounds.size.width = infinity;
                break;
            case 5:
                scene.bounds.size.height = nan;
                break;
            case 6:
                scene.screen.x = infinity;
                break;
            case 7:
                scene.screen.y = nan;
                break;
            case 8:
                scene.base.x = infinity;
                break;
            case 9:
                scene.base.y = nan;
                break;
            case 10:
                scene.local.x = infinity;
                break;
            case 11:
                scene.local.y = nan;
                break;
            }
            const uint32_t actual = Query();
            if (actual != CseSdlCocoaMouseRejected) {
                std::fprintf(stderr, "combined mode=%u state=%d malformed=%d result=%u expected=%u\n",
                    mode, state, malformed, actual, CseSdlCocoaMouseRejected);
            }
            CHECK(actual == CseSdlCocoaMouseRejected);
            CHECK(scene.hits == 0);
        }
    }
    std::printf("combined malformed cases=%d\n", InteractionStates * MalformedReads);
}

void Cases() {
    CombinedMalformedCases();
    scene = Scene{};
    Expected(CseSdlCocoaMouseReady);
    CHECK(scene.screenReads == 1 && scene.fromScreenReads == 1 && scene.conversions == 1 && scene.hits == 1);
    // A transparent/ignoresMouseEvents window is skipped by AppKit's hit test.
    // Its resulting recipient is still our window; no process/visual heuristic is used.
    scene = Scene{};
    scene.hit = OwnedNumber;
    Expected(CseSdlCocoaMouseReady);
    for (NSInteger hit : {ForeignNumber, NSInteger(0), NSInteger(-1)}) {
        scene = Scene{};
        scene.hit = hit;
        Expected(NotInteractionOwner);
    }
    for (int failure = 0; failure < 4; ++failure) {
        scene = Scene{};
        switch (failure) {
        case 0:
            scene.active = false;
            break;
        case 1:
            scene.keyWindow = false;
            break;
        case 2:
            scene.isKey = false;
            break;
        case 3:
            scene.minimized = true;
            break;
        }
        Expected(NotInteractionOwner);
        CHECK(scene.screenReads == 1 && scene.fromScreenReads == 1 && scene.conversions == 1 && scene.hits == 0);
    }
    for (int failure = 0; failure < 5; ++failure) {
        scene = Scene{};
        switch (failure) {
        case 0:
            scene.owned = false;
            break;
        case 1:
            scene.associated = false;
            break;
        case 2:
            scene.hasView = false;
            break;
        case 3:
            scene.number = 0;
            break;
        case 4:
            scene.number = -1;
            break;
        }
        Expected(CseSdlCocoaMouseRejected);
        CHECK(scene.screenReads == 0 && scene.hits == 0);
    }
    const double infinity = (std::numeric_limits<double>::infinity)();
    const double nan = (std::numeric_limits<double>::quiet_NaN)();
    for (NSRect rect : {NSMakeRect(0, 0, 0, 480), NSMakeRect(0, 0, 640, -1),
        NSMakeRect(infinity, 0, 640, 480), NSMakeRect(0, nan, 640, 480),
        NSMakeRect(0, 0, infinity, 480), NSMakeRect(0, 0, 640, nan)}) {
        scene = Scene{};
        scene.bounds = rect;
        Expected(CseSdlCocoaMouseRejected);
        CHECK(scene.screenReads == 0 && scene.hits == 0);
    }
    for (NSPoint point : {NSMakePoint(9, 60), NSMakePoint(650, 60), NSMakePoint(50, 19), NSMakePoint(50, 500)}) {
        scene = Scene{};
        scene.local = point;
        Expected(NotInteractionOwner);
        CHECK(scene.hits == 0);
    }
    for (int failure = 0; failure < 6; ++failure) {
        scene = Scene{};
        switch (failure) {
        case 0:
            scene.screen.x = infinity;
            break;
        case 1:
            scene.screen.y = nan;
            break;
        case 2:
            scene.local.x = infinity;
            break;
        case 3:
            scene.local.y = nan;
            break;
        case 4:
            scene.base.x = infinity;
            break;
        case 5:
            scene.base.y = nan;
            break;
        }
        Expected(CseSdlCocoaMouseRejected);
        CHECK(scene.hits == 0);
    }
    scene = Scene{};
    for (int missing = 0; missing < 3; ++missing) {
        auto interaction = Interaction();
        switch (missing) {
        case 0:
            interaction.screenPoint = nullptr;
            break;
        case 1:
            interaction.pointFromScreen = nullptr;
            break;
        case 2:
            interaction.interactionWindowNumber = nullptr;
            break;
        }
        CHECK(cse::CheckCocoaInteractionOwnerV1(Services(), interaction, Contract,
            (__bridge void *)window) == CseSdlCocoaMouseRejected);
    }
    CHECK(scene.nativeReads == 0 && scene.hits == 0);
    for (int missing = 0; missing < 12; ++missing) {
        auto incomplete = Services();
        switch (missing) {
        case 0:
            incomplete.windowNumber = nullptr;
            break;
        case 1:
            incomplete.numberWindow = nullptr;
            break;
        case 2:
            incomplete.contentView = nullptr;
            break;
        case 3:
            incomplete.viewWindow = nullptr;
            break;
        case 4:
            incomplete.applicationActive = nullptr;
            break;
        case 5:
            incomplete.keyWindow = nullptr;
            break;
        case 6:
            incomplete.isKey = nullptr;
            break;
        case 7:
            incomplete.miniaturized = nullptr;
            break;
        case 8:
            incomplete.bounds = nullptr;
            break;
        case 9:
            incomplete.flipped = nullptr;
            break;
        case 10:
            incomplete.outsidePoint = nullptr;
            break;
        case 11:
            incomplete.convertPoint = nullptr;
            break;
        }
        CHECK(cse::CheckCocoaInteractionOwnerV1(incomplete, Interaction(), Contract,
            (__bridge void *)window) == CseSdlCocoaMouseRejected);
    }
    auto services = Services();
    services.mainThread = nullptr;
    CHECK(cse::CheckCocoaInteractionOwnerV1(services, Interaction(), Contract,
        (__bridge void *)window) == CseSdlCocoaMouseWrongThread);
    scene.main = false;
    Expected(CseSdlCocoaMouseWrongThread);
    CHECK(scene.nativeReads == 0);
    scene = Scene{};
    const int before = modeReads;
    CHECK(cse::CheckCocoaInteractionOwnerV1(Services(), Interaction(), 0,
        (__bridge void *)window) == CseSdlCocoaMouseContractMismatch);
    CHECK(cse::CheckCocoaInteractionOwnerV1(Services(), Interaction(), Contract,
        nullptr) == CseSdlCocoaMouseInvalidArgument);
    CHECK(modeReads == before && scene.nativeReads == 0);
    const auto savedMode = mode;
    for (uint32_t rejected : {0U, 3U, 4U, 5U, 6U, 0xffffffffU}) {
        mode = rejected;
        const uint32_t expected = rejected <= 5 ? rejected : 0U;
        CHECK(cse::CheckCocoaInteractionOwnerV1(Services(), Interaction(), Contract,
            reinterpret_cast<void *>(uintptr_t(1))) == expected);
        CHECK(scene.nativeReads == 0 && scene.screenReads == 0 && scene.hits == 0);
    }
    mode = savedMode;
    for (int repeat = 0; repeat < 10; ++repeat) {
        scene = Scene{};
        Expected(CseSdlCocoaMouseReady);
    }
}
#endif
} // namespace

extern "C" uint32_t CSE_SDL_NATIVE_MOUSE_CALL CseSdl_QueryMouseModeV1(uint32_t version) {
    CHECK(version == CSE_SDL_MOUSE_MODE_CONTRACT_V1);
    ++modeReads;
    return mode;
}
extern "C" bool SDLCALL SDL_CursorVisible() {
    CHECK(false); // Query must not enter cursor mutation/diagnostics.
    return false;
}

int main(int argc, char **argv) {
    __weak NSObject *releasedWindow;
    __weak NSObject *releasedForeign;
    __weak NSObject *releasedView;
    @autoreleasepool {
        NSObject *windowToken = [NSObject new];
        NSObject *foreignToken = [NSObject new];
        NSObject *viewToken = [NSObject new];
        releasedWindow = windowToken;
        releasedForeign = foreignToken;
        releasedView = viewToken;
        window = (NSWindow *)windowToken;
        foreign = (NSWindow *)foreignToken;
        view = (NSView *)viewToken;
        if (argc == 2 && std::strcmp(argv[1], "--relative") == 0) {
            mode = CseSdlMouseModeRelative;
        }
        ForeignBoundary();
#ifndef CSE_ORIGINAL_OWNER_BOUNDARY
        Cases();
#endif
        window = nil;
        foreign = nil;
        view = nil;
    }
    CHECK(releasedWindow == nil && releasedForeign == nil && releasedView == nil);
    std::printf("interaction checks=%d\n", checks);
    return 0;
}
