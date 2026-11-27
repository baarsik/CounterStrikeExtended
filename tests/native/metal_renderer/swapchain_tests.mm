#include <SDL3/SDL.h>
#import <AppKit/AppKit.h>
#import <QuartzCore/CAMetalLayer.h>
#import <objc/runtime.h>
#include <cstdio>

namespace {
unsigned missingDrawableCalls = 0;

id<CAMetalDrawable> MissingDrawable(id, SEL) {
    ++missingDrawableCalls;
    return nil;
}

CAMetalLayer* SwapchainLayer(SDL_Window* window) {
    auto* nativeWindow = (__bridge NSWindow*)SDL_GetPointerProperty(
        SDL_GetWindowProperties(window), SDL_PROP_WINDOW_COCOA_WINDOW_POINTER, nullptr);
    if (!nativeWindow) {
        return nil;
    }
    for (NSView* view in nativeWindow.contentView.subviews) {
        if ([view.layer isKindOfClass:CAMetalLayer.class]) {
            return (CAMetalLayer*)view.layer;
        }
    }
    return nil;
}
}

int main() {
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        std::fprintf(stderr, "SDL video initialization: %s\n", SDL_GetError());
        return 1;
    }
    auto* window = SDL_CreateWindow("Metal missing drawable", 32, 32, 0);
    auto* device = SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_METALLIB, true, "metal");
    if (!window || !device || !SDL_ClaimWindowForGPUDevice(device, window)) {
        std::fprintf(stderr, "Metal window setup: %s\n", SDL_GetError());
        return 1;
    }
    auto* layer = SwapchainLayer(window);
    auto* method = layer ? class_getInstanceMethod([layer class], @selector(nextDrawable)) : nullptr;
    auto* command = SDL_AcquireGPUCommandBuffer(device);
    if (!method || !command) {
        std::fprintf(stderr, "Metal command setup: %s\n", SDL_GetError());
        return 1;
    }

    const IMP original = method_setImplementation(method, (IMP)MissingDrawable);
    SDL_GPUTexture* texture = nullptr;
    Uint32 width = 0;
    Uint32 height = 0;
    const bool acquired = SDL_WaitAndAcquireGPUSwapchainTexture(command, window, &texture, &width, &height);
    method_setImplementation(method, original);
    if (!acquired || missingDrawableCalls != 1 || texture != nullptr) {
        // The unpatched provider exposes a non-null wrapper around a nil MTLTexture.
        // Submitting it would assert in Metal's presentDrawable: path.
        std::fprintf(stderr, "Missing drawable: acquired=%d calls=%u texture=%p error=%s\n",
            acquired, missingDrawableCalls, texture, SDL_GetError());
        return 1;
    }
    if (!SDL_CancelGPUCommandBuffer(command)) {
        std::fprintf(stderr, "Cancel missed frame: %s\n", SDL_GetError());
        return 1;
    }

    command = SDL_AcquireGPUCommandBuffer(device);
    texture = nullptr;
    if (!command || !SDL_WaitAndAcquireGPUSwapchainTexture(command, window, &texture, &width, &height) || !texture ||
        !SDL_SubmitGPUCommandBuffer(command)) {
        std::fprintf(stderr, "Acquire after missed frame: %s\n", SDL_GetError());
        return 1;
    }
    SDL_ReleaseWindowFromGPUDevice(device, window);
    SDL_DestroyGPUDevice(device);
    SDL_DestroyWindow(window);
    SDL_Quit();
    std::puts("Missing Metal drawable is skipped and the next frame recovers");
    return 0;
}
