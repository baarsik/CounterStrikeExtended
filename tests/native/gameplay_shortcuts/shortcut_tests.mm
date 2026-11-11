// Mutations caught: guard reordering, broad filtering, lost cleanup ownership,
// missing generation recheck, stale SDL identity, Ready while a callback is live.
#include "sdl3_native_cocoa_keyboard_private.h"
#include <SDL3/SDL_mouse.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {
int checks = 0;
void Check(bool value, const char *expression, int line) {
    ++checks;
    if (!value) {
        std::fprintf(stderr, "shortcut-check:%d:%s\n", line, expression);
        std::exit(17);
    }
}
#define CHECK(value) Check((value), #value, __LINE__)
NSWindow *window;
NSWindow *foreign;
NSView *view;
NSView *foreignView;
constexpr uint32_t ForeignWindowId = 23;
enum class ConflictingReentry { None, WindowNumber, WindowId };
NSEvent *event;
id token;
cse::CocoaGameplayShortcutState state;
uint32_t mode = CseSdlMouseModeAbsolute;
int modeReads = 0;
struct Scene {
    bool main = true;
    bool owned = true;
    bool associated = true;
    bool active = true;
    bool key = true;
    bool minimized = false;
    bool sdlOwned = true;
    bool installFails = false;
    bool removeFails = false;
    bool installThrows = false;
    bool removeThrows = false;
    bool eventThrows = false;
    bool callbackChangesId = false;
    bool admissionReenters = false;
    ConflictingReentry conflictingReentry = ConflictingReentry::None;
    bool installReenters = false;
    bool removeReenters = false;
    bool callbackDisables = false;
    bool callbackReenables = false;
    bool foreignEvent = false;
    bool nestedEvent = false;
    NSInteger number = 31;
    uint32_t id = 19;
    NSEventType type = NSEventTypeKeyDown;
    unsigned short keyCode = 49;
    NSEventModifierFlags modifiers = NSEventModifierFlagControl;
    int nativeReads = 0;
    int installs = 0;
    int removals = 0;
    uint32_t nestedResult = 99;
} scene;
cse::NativeMouseCocoaServices Native();
cse::CocoaGameplayShortcutServices Shortcuts();
uint32_t Set(uint32_t enabled = 1, void *native = nullptr) {
    return cse::SetCocoaGameplayShortcutsV1(Native(), Shortcuts(), state, 1,
        native ? native : (__bridge void *)window, enabled);
}
NSEvent *Filter() {
    return cse::FilterCocoaGameplayShortcut(Native(), Shortcuts(), state, event);
}
bool Main() {
    return scene.main;
}

void ReenterConflictingDisable(NSWindow *value, ConflictingReentry point) {
    if (value == foreign && scene.conflictingReentry == point) {
        scene.conflictingReentry = ConflictingReentry::None;
        scene.nestedResult = Set(0, (__bridge void *)foreign);
    }
}

NSInteger Number(NSWindow *value) {
    CHECK(value == window || value == foreign);
    ++scene.nativeReads;
    ReenterConflictingDisable(value, ConflictingReentry::WindowNumber);
    return value == window ? scene.number : 47;
}
NSWindow *NumberWindow(NSInteger number) {
    return scene.owned && number == scene.number ? window : foreign;
}

NSView *Content(NSWindow *value) {
    return value == foreign ? foreignView : view;
}

NSWindow *ViewWindow(NSView *value) {
    return value == foreignView || !scene.associated ? foreign : window;
}

bool Active() {
    if (scene.callbackChangesId) {
        scene.callbackChangesId = false;
        ++scene.id;
    }
    if (scene.callbackDisables) {
        scene.callbackDisables = false;
        scene.nestedResult = Set(0);
        CHECK(state.callbackDepth > 0);
        CHECK(state.monitor == nil);
    }
    if (scene.callbackReenables) {
        scene.callbackReenables = false;
        scene.nestedResult = Set();
    }
    if (scene.nestedEvent) {
        scene.nestedEvent = false;
        CHECK(Filter() == nil);
    }
    return scene.active;
}
NSWindow *Key() {
    return scene.key ? window : foreign;
}

bool IsKey(NSWindow *) {
    return scene.key;
}

bool Minimized(NSWindow *) {
    return scene.minimized;
}

NSRect Bounds(NSView *) {
    CHECK(false);
    return NSZeroRect;
}

bool Flipped(NSView *) {
    CHECK(false);
    return false;
}

NSPoint Outside(NSWindow *) {
    CHECK(false);
    return NSZeroPoint;
}

NSPoint Convert(NSView *, NSPoint) {
    CHECK(false);
    return NSZeroPoint;
}

uint32_t WindowId(NSWindow *value) {
    ReenterConflictingDisable(value, ConflictingReentry::WindowId);
    if (scene.admissionReenters) {
        scene.admissionReenters = false;
        scene.nestedResult = Set(0);
    }
    if (!scene.sdlOwned) {
        return 0;
    }
    return value == window ? scene.id : ForeignWindowId;
}
NSWindow *WindowForId(uint32_t id) {
    if (!scene.sdlOwned) {
        return nil;
    }
    return id == scene.id ? window : (id == ForeignWindowId ? foreign : nil);
}

id Install() {
    ++scene.installs;
    if (scene.installThrows) {
        @throw [NSException exceptionWithName:@"FixtureInstall" reason:nil userInfo:nil];
    }
    if (scene.installReenters) {
        scene.installReenters = false;
        scene.nestedResult = Set(0);
    }
    return scene.installFails ? nil : token;
}
bool Remove(id value) {
    CHECK(value == token);
    ++scene.removals;
    if (scene.removeThrows) {
        @throw [NSException exceptionWithName:@"FixtureRemove" reason:nil userInfo:nil];
    }
    if (scene.removeReenters) {
        scene.removeReenters = false;
        scene.nestedResult = Set();
    }
    return !scene.removeFails;
}
NSEventType Type(NSEvent *) {
    if (scene.eventThrows) {
        @throw [NSException exceptionWithName:@"FixtureEvent" reason:nil userInfo:nil];
    }
    return scene.type;
}

unsigned short Code(NSEvent *) {
    return scene.keyCode;
}

NSEventModifierFlags Modifiers(NSEvent *) {
    return scene.modifiers;
}

NSWindow *EventWindow(NSEvent *) {
    return scene.foreignEvent ? foreign : window;
}

cse::NativeMouseCocoaServices Native() {
    return {Main, Number, NumberWindow, Content, ViewWindow, Active, Key, IsKey, Minimized,
        Bounds, Flipped, Outside, Convert};
}
cse::CocoaGameplayShortcutServices Shortcuts() {
    return {WindowId, WindowForId, Install, Remove, Type, Code, Modifiers, EventWindow};
}
void Reset() {
    state = {};
    scene = {};
    modeReads = 0;
}
void Guards() {
    Reset();
    CHECK(cse::SetCocoaGameplayShortcutsV1(Native(), Shortcuts(), state, 2, nullptr, 2) == 4);
    CHECK(modeReads == 0 && scene.nativeReads == 0);
    CHECK(cse::SetCocoaGameplayShortcutsV1(Native(), Shortcuts(), state, 1, nullptr, 1) == 6);
    CHECK(Set(2) == 6);
    CHECK(modeReads == 0 && scene.nativeReads == 0);
    const uint32_t modes[] = {0, 3, 4, 5, 99};
    const uint32_t results[] = {0, 3, 4, 5, 0};
    const auto saved = mode;
    for (unsigned int index = 0; index < sizeof(modes) / sizeof(modes[0]); ++index) {
        mode = modes[index];
        CHECK(Set() == results[index]);
        CHECK(scene.nativeReads == 0 && scene.installs == 0);
    }
    mode = saved;
    scene.main = false;
    CHECK(Set() == 3);
    CHECK(scene.nativeReads == 0);
    scene.main = true;
    scene.owned = false;
    CHECK(Set() == 2);
    scene.owned = true;
    scene.sdlOwned = false;
    CHECK(Set() == 2);
    scene.sdlOwned = true;
    scene.associated = false;
    CHECK(Set() == 2);
    scene.associated = true;
    auto missing = Shortcuts();
    missing.removeMonitor = nullptr;
    CHECK(cse::SetCocoaGameplayShortcutsV1(Native(), missing, state, 1, (__bridge void *)window, 1) == 2);
    CHECK(scene.installs == 0);
}
void Lifecycle() {
    Reset();
    CHECK(Set(0) == 1);
    CHECK(scene.removals == 0);
    scene.installFails = true;
    CHECK(Set() == 2);
    scene.installFails = false;
    CHECK(Set() == 1);
    CHECK(scene.installs == 2);
    CHECK(Set() == 1);
    CHECK(scene.installs == 2);
    CHECK(Set(1, (__bridge void *)foreign) == 2);
    CHECK(state.monitor == token);
    scene.removeFails = true;
    CHECK(Set(0) == 2);
    CHECK(state.monitor == token);
    CHECK(Filter() == event);
    scene.removeFails = false;
    scene.active = false;
    scene.key = false;
    CHECK(Set(0) == 1);
    CHECK(state.monitor == nil);
    CHECK(Set(0) == 1);
    CHECK(scene.removals == 2);
    Reset();
    CHECK(Set() == 1);
    ++scene.id; // Same native address/number, different SDL lifetime.
    CHECK(Filter() == event);
    CHECK(Set() == 2);
    CHECK(Set(0) == 2);
    CHECK(state.monitor == token);
    --scene.id;
    ++scene.number;
    CHECK(Filter() == event);
    CHECK(Set() == 2);
    --scene.number;
    CHECK(Set(0) == 1);
}
void Events() {
    Reset();
    CHECK(Set() == 1);
    CHECK(Filter() == nil);
    // Device-specific left/right/both Control bits must not alter admission.
    const NSEventModifierFlags controls[] = {0, 1, 0x2000, 0x2001};
    for (auto control : controls) {
        scene.modifiers = NSEventModifierFlagControl | control;
        CHECK(Filter() == nil);
        scene.modifiers |= NSEventModifierFlagShift;
        CHECK(Filter() == nil);
    }
    scene.modifiers = NSEventModifierFlagControl | NSEventModifierFlagCommand;
    CHECK(Filter() == event);
    scene.modifiers = NSEventModifierFlagControl | NSEventModifierFlagOption;
    CHECK(Filter() == event);
    scene.modifiers = NSEventModifierFlagShift;
    CHECK(Filter() == event);
    scene.modifiers = NSEventModifierFlagControl;
    scene.type = NSEventTypeKeyUp;
    CHECK(Filter() == event);
    scene.type = NSEventTypeFlagsChanged;
    CHECK(Filter() == event);
    scene.type = NSEventTypeKeyDown;
    scene.keyCode = 50;
    CHECK(Filter() == event);
    scene.keyCode = 49;
    scene.foreignEvent = true;
    CHECK(Filter() == event);
    scene.foreignEvent = false;
    scene.active = false;
    CHECK(Filter() == event);
    scene.active = true;
    scene.key = false;
    CHECK(Filter() == event);
    scene.key = true;
    scene.minimized = true;
    CHECK(Filter() == event);
    scene.minimized = false;
    const auto saved = mode;
    mode = CseSdlMouseModeNotReady;
    CHECK(Filter() == event);
    mode = saved;
    CHECK(Set(0) == 1);
    CHECK(Filter() == event);
}
void Exceptions() {
    Reset();
    scene.installThrows = true;
    CHECK(Set() == 2);
    CHECK(!state.busy && !state.enabled);
    scene.installThrows = false;
    CHECK(Set(0) == 1);
    CHECK(Set() == 1);
    scene.eventThrows = true;
    CHECK(Filter() == event);
    CHECK(state.callbackDepth == 0);
    scene.eventThrows = false;
    scene.removeThrows = true;
    CHECK(Set(0) == 2);
    CHECK(state.monitor == token && !state.enabled && !state.busy);
    CHECK(Set() == 2);
    scene.removeThrows = false;
    CHECK(Set(0) == 1);
}
// A rejected conflicting transaction must never cancel the installed owner.
void ConflictingOwnerReentrancy() {
    const ConflictingReentry seams[] = {ConflictingReentry::WindowNumber, ConflictingReentry::WindowId};
    for (auto seam : seams) {
        for (uint32_t requested = 0; requested <= 1; ++requested) {
            Reset();
            CHECK(Set() == 1);
            const uint64_t generation = state.generation;
            id installedToken = state.monitor;
            scene.conflictingReentry = seam;
            CHECK(Set(requested, (__bridge void *)foreign) == 2);
            CHECK(scene.conflictingReentry == ConflictingReentry::None);
            CHECK(scene.nestedResult == 2);
            CHECK(state.enabled);
            CHECK(state.generation == generation);
            CHECK(state.monitor == installedToken);
            CHECK(scene.installs == 1 && scene.removals == 0);
            CHECK(Filter() == nil);
            CHECK(Set() == 1);
            CHECK(scene.installs == 1);
            CHECK(Set(0) == 1);
        }
    }
}

void Reentrancy() {
    Reset();
    scene.admissionReenters = true;
    CHECK(Set() == 2);
    CHECK(scene.nestedResult == 2);
    CHECK(scene.installs == 0);
    CHECK(Set(0) == 1);
    Reset();
    scene.installReenters = true;
    CHECK(Set() == 2);
    CHECK(scene.nestedResult == 2);
    CHECK(scene.installs == 1 && scene.removals == 1);
    CHECK(Filter() == event);
    CHECK(Set(0) == 1);
    CHECK(Set() == 1);
    scene.removeReenters = true;
    CHECK(Set(0) == 1);
    CHECK(scene.nestedResult == 2);
    CHECK(Set() == 1);
    scene.callbackDisables = true;
    CHECK(Filter() == event); // Generation invalidation beats earlier admission.
    CHECK(scene.nestedResult == 2); // Caller cannot unload while filter is on stack.
    CHECK(state.callbackDepth == 0);
    CHECK(Set(0) == 1); // Explicit post-unwind acknowledgement authorizes retirement.
    CHECK(Set() == 1);
    scene.callbackChangesId = true;
    CHECK(Filter() == event);
    --scene.id;
    scene.callbackReenables = true;
    CHECK(Filter() == nil);
    CHECK(scene.nestedResult == 1);
    scene.nestedEvent = true;
    CHECK(Filter() == nil);
    CHECK(state.callbackDepth == 0);
    CHECK(Set(0) == 1);
    Reset();
    scene.installReenters = true;
    scene.removeFails = true;
    CHECK(Set() == 2);
    CHECK(state.monitor == token);
    CHECK(Filter() == event);
    scene.removeFails = false;
    CHECK(Set(0) == 1);
}
} // namespace
extern "C" uint32_t CseSdl_QueryMouseModeV1(uint32_t version) {
    CHECK(version == 1);
    ++modeReads;
    return mode;
}
extern "C" bool SDL_CursorVisible() {
    CHECK(false);
    return true;
}

int main(int argc, char **argv) {
    @autoreleasepool {
        window = (NSWindow *)[NSObject new];
        foreign = (NSWindow *)[NSObject new];
        view = (NSView *)[NSObject new];
        foreignView = (NSView *)[NSObject new];
        event = (NSEvent *)[NSObject new];
        token = [NSObject new];
        if (argc == 2 && std::strcmp(argv[1], "--relative") == 0) {
            mode = CseSdlMouseModeRelative;
        }
        Guards();
        Lifecycle();
        Events();
        ConflictingOwnerReentrancy();
        Reentrancy();
        Exceptions();
        std::printf("actual-provider shortcuts: %d checks, mode=%u\n", checks, mode);
    }
    return 0;
}
