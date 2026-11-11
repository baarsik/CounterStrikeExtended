#pragma once

#include "sdl3_native_cocoa_mouse.h"
#import <Cocoa/Cocoa.h>

namespace cse {
// Typed AppKit reads. Default bindings are real SDK calls; fixtures inject every operation.
struct NativeMouseCocoaServices {
    bool (*mainThread)();
    NSInteger (*windowNumber)(NSWindow *window);
    NSWindow *(*numberWindow)(NSInteger number);
    NSView *(*contentView)(NSWindow *window);
    NSWindow *(*viewWindow)(NSView *view);
    bool (*applicationActive)();
    NSWindow *(*keyWindow)();
    bool (*isKey)(NSWindow *window);
    bool (*miniaturized)(NSWindow *window);
    NSRect (*bounds)(NSView *view);
    bool (*flipped)(NSView *view);
    NSPoint (*outsidePoint)(NSWindow *window);
    NSPoint (*convertPoint)(NSView *view, NSPoint point);
};

// Additional public AppKit reads, borrowed only by the interaction query. Older
// service entrypoints neither receive nor require these callbacks. screenPoint
// returns NSEvent's global screen position. pointFromScreen converts that point
// into this window's base coordinates; existing convertPoint then maps it into
// the content view. interactionWindowNumber returns NSWindow's frontmost
// mouse-down recipient at that screen point (below window number zero), excluding
// ignoresMouseEvents windows. No callback or native pointer is retained.
struct NativeMouseCocoaInteractionServices {
    NSPoint (*screenPoint)();
    NSPoint (*pointFromScreen)(NSWindow *window, NSPoint point);
    NSInteger (*interactionWindowNumber)(NSPoint point);
};

bool OwnsNativeMouseCocoa(const NativeMouseCocoaServices &services, NSWindow *window);
bool FocusesNativeMouseCocoa(const NativeMouseCocoaServices &services, NSWindow *window, int width, int height);
bool ReadNativeMouseCocoa(const NativeMouseCocoaServices &services, NSWindow *window, int width, int height, float *x,
                          float *y);
// Private typed body entrypoints for the same synchronous guards, not exported
// provider callbacks or an installable registry. Each call borrows its services.
uint32_t CheckCocoaMouseOwnedV1(const NativeMouseCocoaServices &services, uint32_t version, void *window);
uint32_t CheckCocoaInteractionOwnerV1(const NativeMouseCocoaServices &services,
    const NativeMouseCocoaInteractionServices &interaction, uint32_t version, void *window);
uint32_t CheckCocoaRelativeFocusV1(const NativeMouseCocoaServices &services, uint32_t version, void *window,
                                  int32_t width, int32_t height);
uint32_t ReadCocoaMouseAbsoluteV1(const NativeMouseCocoaServices &services, uint32_t version, void *window,
                                 int32_t width, int32_t height, float *x, float *y);
} // namespace cse
