#include "sdl3_native_cocoa_mouse_private.h"
#include <SDL3/SDL_mouse.h>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <initializer_list>

namespace {
int checks = 0;
uint32_t bridgeMode = CseSdlMouseModeAbsolute;
int bridgeCalls = 0;
void Check(bool value, const char *text, int line) {
    ++checks;
    if (!value) {
        std::fprintf(stderr, "cocoa-sample-check:%d:%s\n", line, text);
        std::exit(17);
    }
}
#define CHECK(value) Check((value), #value, __LINE__)
NSWindow *window;
NSWindow *foreign;
NSView *view;
struct Scene {
    bool main = true;
    bool active = true;
    bool key = true;
    bool miniature = false;
    bool flipped = false;
    bool owned = true;
    bool associated = true;
    bool hasView = true;
    NSInteger number = 31;
    NSRect bounds = NSMakeRect(0, 0, 640, 480);
    NSPoint nativePoint = NSMakePoint(360, 216);
    NSPoint convertedPoint = NSMakePoint(360, 216);
    int pointReads = 0;
    int conversions = 0;
    int boundsReads = 0;
};
Scene scene;
bool Main() {
    return scene.main;
}
NSInteger Number(NSWindow *value) {
    CHECK(value == window);
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
    return scene.key ? window : foreign;
}
bool Key(NSWindow *value) {
    CHECK(value == window);
    return scene.key;
}
bool Miniature(NSWindow *value) {
    CHECK(value == window);
    return scene.miniature;
}
NSRect Bounds(NSView *value) {
    CHECK(value == view);
    ++scene.boundsReads;
    return scene.bounds;
}
bool Flipped(NSView *value) {
    CHECK(value == view);
    return scene.flipped;
}
NSPoint Outside(NSWindow *value) {
    CHECK(value == window);
    ++scene.pointReads;
    return scene.nativePoint;
}
NSPoint Convert(NSView *value, NSPoint point) {
    CHECK(value == view && point.x == scene.nativePoint.x && point.y == scene.nativePoint.y);
    ++scene.conversions;
    return scene.convertedPoint;
}
cse::NativeMouseCocoaServices Services() {
    return {Main, Number,    NumberWindow, Content, ViewWindow, Active, KeyWindow,
            Key,  Miniature, Bounds,       Flipped, Outside,    Convert};
}
void Fail() {
    float x = 123.5f;
    float y = 456.5f;
    CHECK(!cse::ReadNativeMouseCocoa(Services(), window, 640, 480, &x, &y));
    CHECK(x == 123.5f && y == 456.5f);
}
} // namespace

extern "C" uint32_t CSE_SDL_NATIVE_MOUSE_CALL CseSdl_QueryMouseModeV1(uint32_t version) {
    CHECK(version == CSE_SDL_MOUSE_MODE_CONTRACT_V1);
    ++bridgeCalls;
    return bridgeMode;
}

extern "C" bool SDLCALL SDL_CursorVisible() {
    CHECK(false); // Opt-in diagnostics must remain disabled in this injected component.
    return false;
}

int main() {
    @autoreleasepool {
        // NSObject tokens only: no NSWindow, view, application or provider is instantiated.
        NSObject *windowToken = [NSObject new];
        NSObject *foreignToken = [NSObject new];
        NSObject *viewToken = [NSObject new];
        window = (NSWindow *)windowToken;
        foreign = (NSWindow *)foreignToken;
        view = (NSView *)viewToken;
        const auto services = Services();
        CHECK(cse::OwnsNativeMouseCocoa(services, window));
        float x = 0.0f;
        float y = 0.0f;
        CHECK(cse::ReadNativeMouseCocoa(services, window, 640, 480, &x, &y));
        CHECK(x == 360.0f && y == 264.0f && scene.pointReads == 1 && scene.conversions == 1);
        CHECK(scene.boundsReads == 1);
        scene = Scene{};
        scene.bounds = NSMakeRect(10, 20, 640, 480);
        scene.convertedPoint = NSMakePoint(370, 236);
        CHECK(cse::ReadNativeMouseCocoa(services, window, 640, 480, &x, &y));
        CHECK(x == 360.0f && y == 264.0f);
        scene.flipped = true;
        CHECK(cse::ReadNativeMouseCocoa(services, window, 640, 480, &x, &y));
        CHECK(x == 360.0f && y == 216.0f);
        scene.convertedPoint = NSMakePoint(-100, 1000);
        CHECK(cse::ReadNativeMouseCocoa(services, window, 640, 480, &x, &y));
        CHECK(x == -110.0f && y == 980.0f);
        const int reads = scene.pointReads;
        CHECK(cse::FocusesNativeMouseCocoa(services, window, 640, 480));
        CHECK(scene.pointReads == reads);
        scene = Scene{};
        scene.main = false;
        Fail();
        CHECK(scene.pointReads == 0);
        scene = Scene{};
        scene.owned = false;
        Fail();
        scene = Scene{};
        scene.associated = false;
        Fail();
        scene = Scene{};
        scene.hasView = false;
        Fail();
        scene = Scene{};
        scene.active = false;
        Fail();
        scene = Scene{};
        scene.key = false;
        Fail();
        scene = Scene{};
        scene.miniature = true;
        Fail();
        scene = Scene{};
        scene.number = -1;
        Fail();
        scene = Scene{};
        scene.bounds.size.width = 1280;
        Fail();
        scene = Scene{};
        scene.bounds.size.height = 0;
        Fail();
        scene = Scene{};
        scene.bounds.origin.x = (std::numeric_limits<double>::infinity)();
        Fail();
        scene = Scene{};
        scene.convertedPoint.x = (std::numeric_limits<double>::quiet_NaN)();
        Fail();
        scene = Scene{};
        scene.convertedPoint.y = (std::numeric_limits<double>::max)();
        Fail();
        scene = Scene{};
        CHECK(!cse::ReadNativeMouseCocoa(services, window, 0, 480, &x, &y));
        CHECK(!cse::ReadNativeMouseCocoa(services, window, 640, 480, nullptr, &y));
        CHECK(!cse::ReadNativeMouseCocoa(services, nil, 640, 480, &x, &y));
        CHECK(scene.pointReads == 0);
        scene = Scene{};
        void *native = (__bridge void *)window;
        constexpr uint32_t contract = CSE_SDL_COCOA_MOUSE_CONTRACT_V1;
        CHECK(cse::CheckCocoaMouseOwnedV1(services, contract, native) == CseSdlCocoaMouseReady);
        CHECK(cse::ReadCocoaMouseAbsoluteV1(services, contract, native, 640, 480, &x, &y) == CseSdlCocoaMouseReady);
        CHECK(x == 360.0f && y == 264.0f);
        CHECK(cse::CheckCocoaRelativeFocusV1(services, contract, native, 640, 480) == CseSdlCocoaMouseWrongMode);
        bridgeMode = CseSdlMouseModeRelative;
        CHECK(cse::CheckCocoaRelativeFocusV1(services, contract, native, 640, 480) == CseSdlCocoaMouseReady);
        x = 11.0f;
        y = 12.0f;
        CHECK(cse::ReadCocoaMouseAbsoluteV1(services, contract, native, 640, 480, &x, &y) == CseSdlCocoaMouseWrongMode);
        CHECK(x == 11.0f && y == 12.0f);
        const int beforeVersion = bridgeCalls;
        CHECK(cse::CheckCocoaMouseOwnedV1(services, 0, native) == CseSdlCocoaMouseContractMismatch);
        CHECK(cse::ReadCocoaMouseAbsoluteV1(services, contract, nullptr, 640, 480, &x, &y) == CseSdlCocoaMouseInvalidArgument);
        CHECK(bridgeCalls == beforeVersion);
        for (uint32_t rejected : {0U, 3U, 4U, 5U, 6U, 0xffffffffU}) {
            bridgeMode = rejected;
            const uint32_t expected = rejected <= 5 ? rejected : 0U;
            void *poison = reinterpret_cast<void *>(static_cast<uintptr_t>(1));
            CHECK(cse::CheckCocoaMouseOwnedV1(services, contract, poison) == expected);
            CHECK(cse::CheckCocoaRelativeFocusV1(services, contract, poison, 640, 480) == expected);
            CHECK(cse::ReadCocoaMouseAbsoluteV1(services, contract, poison, 640, 480, &x, &y) == expected);
            CHECK(x == 11.0f && y == 12.0f);
        }
        bridgeMode = CseSdlMouseModeAbsolute;
        scene.main = false;
        CHECK(cse::CheckCocoaMouseOwnedV1(services, contract, native) == CseSdlCocoaMouseWrongThread);
        window = nil;
        foreign = nil;
        view = nil;
    }
    std::printf("cocoa-sample checks=%d\n", checks);
    return 0;
}
