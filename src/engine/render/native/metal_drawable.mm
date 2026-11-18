#include "metal_drawable.h"
#include <SDL3/SDL_video.h>
#import <AppKit/AppKit.h>
#import <QuartzCore/CAMetalLayer.h>

namespace cse {
namespace render {
namespace {
CAMetalLayer* FindMetalDrawableLayer(SDL_Window* window) {
    const auto properties = SDL_GetWindowProperties(window);
    auto* nativeWindow = (__bridge NSWindow*)SDL_GetPointerProperty(
        properties, SDL_PROP_WINDOW_COCOA_WINDOW_POINTER, nullptr);
    CAMetalLayer* drawableLayer = nil;
    // The pinned SDL Metal driver adds its swapchain view directly to the content view.
    for (NSView* view in nativeWindow.contentView.subviews) {
        if ([view.layer isKindOfClass:CAMetalLayer.class]) {
            if (drawableLayer) {
                return nil;
            }
            drawableLayer = (CAMetalLayer*)view.layer;
        }
    }
    return drawableLayer;
}
}

int MetalDrawableCount(SDL_Window* window) {
    @autoreleasepool {
        return static_cast<int>(FindMetalDrawableLayer(window).maximumDrawableCount);
    }
}

bool SetMetalDrawableCount(SDL_Window* window, int count) {
    @autoreleasepool {
        auto* layer = FindMetalDrawableLayer(window);
        if (!layer) {
            return false;
        }
        layer.maximumDrawableCount = static_cast<NSUInteger>(count);
        return true;
    }
}
}
}
