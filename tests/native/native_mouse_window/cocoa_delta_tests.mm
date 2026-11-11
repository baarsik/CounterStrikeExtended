#import <Cocoa/Cocoa.h>
#include <cstdio>
#include <cstdlib>
#include <cstdint>

// Explicit OS boundary: queued native deltas remain independent of the latest
// global position. This fixture neither creates a window nor moves the cursor.
@interface ControlledEvent : NSObject
@property CGFloat dx;
@property CGFloat dy;
+ (NSPoint)mouseLocation;
- (NSEventType)type;
- (NSWindow *)window;
- (NSPoint)locationInWindow;
- (NSTimeInterval)timestamp;
- (CGFloat)deltaX;
- (CGFloat)deltaY;
@end
@implementation ControlledEvent
+ (NSPoint)mouseLocation {
    return NSMakePoint(1920, 1080);
}
- (NSEventType)type {
    return NSEventTypeMouseMoved;
}
- (NSWindow *)window {
    return nil;
}
- (NSPoint)locationInWindow {
    return NSZeroPoint;
}
- (NSTimeInterval)timestamp {
    return 1.0;
}
- (CGFloat)deltaX {
    return _dx;
}
- (CGFloat)deltaY {
    return _dy;
}
@end
@interface SDL_CocoaVideoData : NSObject
@property CGFloat mainDisplayHeight;
@end
@implementation SDL_CocoaVideoData
@end
using SDL_MouseID = unsigned;
constexpr SDL_MouseID SDL_DEFAULT_MOUSE_ID = 1;
struct SDL_MouseData {
    bool seenWarp;
    CGFloat lastWarpX;
    CGFloat lastWarpY;
    CGFloat lastMoveX;
    CGFloat lastMoveY;
};
struct SDL_Mouse {
    bool relative_mode;
    void *internal;
    void *focus;
};
struct SDL_VideoDevice {
    void *internal;
};
static SDL_MouseData native{};
static SDL_Mouse mouse{false, &native, nullptr};
static NSWindow *Cocoa_MouseFocus = nil;
static float acceptedX = 0.0f;
static float acceptedY = 0.0f;
static int acceptedEvents = 0;
static SDL_Mouse *SDL_GetMouse() {
    return &mouse;
}
static void Cocoa_ReconcileButtonState(ControlledEvent *) {}
static void Cocoa_HandleTitleButtonEvent(SDL_VideoDevice *, ControlledEvent *) {}
static std::uint64_t Cocoa_GetEventTimestamp(double) {
    return 1;
}
static void SDL_SendMouseMotion(std::uint64_t, void *, SDL_MouseID, bool relative, float x, float y) {
    if (!relative) {
        std::abort();
    }
    acceptedX += x;
    acceptedY += y;
    ++acceptedEvents;
}
#define NSEvent ControlledEvent
#define DLog(...) ((void)0)
#include "cocoa_delta.inc"
#undef NSEvent
#undef DLog

int main() {
    @autoreleasepool {
        ControlledEvent *event = [ControlledEvent new];
        SDL_VideoDevice device{};
        event.dx = 80.5;
        event.dy = -12.5;
        Cocoa_HandleMouseEvent(&device, event);
        if (acceptedEvents != 0) {
            return 17;
        }
        mouse.relative_mode = true;
        Cocoa_HandleMouseEvent(&device, event);
        event.dx = 17.75;
        event.dy = -4.75;
        Cocoa_HandleMouseEvent(&device, event);
        // A very fast burst remains motion even at the screen/window boundary.
        event.dx = 4096.25;
        event.dy = -2048.25;
        Cocoa_HandleMouseEvent(&device, event);
        if (acceptedEvents != 3 || acceptedX != 4194.5f || acceptedY != -2065.5f) {
            return 17;
        }
        std::printf("pinned Cocoa relative accepted=%g,%g events=%d native-event-deltas unchanged; "
                    "physical acceleration outside fixture\n", acceptedX, acceptedY, acceptedEvents);
    }
}
