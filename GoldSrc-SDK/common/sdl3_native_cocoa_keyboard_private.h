#pragma once

#include "sdl3_native_cocoa_keyboard.h"
#include "sdl3_native_cocoa_mouse_private.h"

namespace cse {
// Per-call typed SDL/AppKit seams, never retained or installed by consumers.
// installMonitor installs the provider-owned block, not an engine callback.
struct CocoaGameplayShortcutServices {
    uint32_t (*windowId)(NSWindow *window);
    NSWindow *(*windowForId)(uint32_t id);
    id (*installMonitor)();
    bool (*removeMonitor)(id token);
    NSEventType (*eventType)(NSEvent *event);
    unsigned short (*keyCode)(NSEvent *event);
    NSEventModifierFlags (*modifiers)(NSEvent *event);
    NSWindow *(*eventWindow)(NSEvent *event);
};

struct CocoaGameplayShortcutState {
    id monitor = nil;
    uint32_t windowId = 0;
    uintptr_t nativeIdentity = 0;
    NSInteger windowNumber = 0;
    uint64_t generation = 0;
    uint32_t callbackDepth = 0;
    bool busy = false;
    bool enabled = false;
    uintptr_t requestIdentity = 0;
};

uint32_t SetCocoaGameplayShortcutsV1(const NativeMouseCocoaServices &native,
    const CocoaGameplayShortcutServices &shortcuts, CocoaGameplayShortcutState &state,
    uint32_t version, void *window, uint32_t enabled);
NSEvent *FilterCocoaGameplayShortcut(const NativeMouseCocoaServices &native,
    const CocoaGameplayShortcutServices &shortcuts, CocoaGameplayShortcutState &state, NSEvent *event);
} // namespace cse
