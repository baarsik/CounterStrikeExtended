#pragma once

#include <SDL3/SDL.h>
#include <cstdint>

namespace cse {
// Engine-private borrowed services; copied into the owner, with no provider registry or cleanup.
struct NativeMousePlatform {
    const char *windowProperty;
    bool (*ownedWindow)(void *nativeWindow);
    bool (*focusedWindow)(void *nativeWindow, int logicalWidth, int logicalHeight);
    bool (*absolutePosition)(void *nativeWindow, int logicalWidth, int logicalHeight, float *x, float *y);
};

NativeMousePlatform NativeMouseDefaultPlatform();

enum class NativeCursorOwner {
    VGui1,
    VGui2
};
class NativeMouseWindow;

class NativeCursorTransaction {
    public:
    NativeCursorTransaction() = default;
    ~NativeCursorTransaction();
    NativeCursorTransaction(const NativeCursorTransaction &) = delete;
    NativeCursorTransaction &operator=(const NativeCursorTransaction &) = delete;
    bool Current() const;
    bool Commit();

    private:
    friend class NativeMouseWindow;
    NativeMouseWindow *owner = nullptr;
    std::uint64_t version = 0;
};

class NativeMouseWindow {
    public:
    explicit NativeMouseWindow(const NativeMousePlatform &platform);
    NativeMouseWindow(const NativeMouseWindow &) = delete;
    NativeMouseWindow &operator=(const NativeMouseWindow &) = delete;
    bool Bind(SDL_Window *window);
    void Invalidate(SDL_Window *window);
    bool Sample(SDL_Window *window, int logicalWidth, int logicalHeight, float *x, float *y) const;
    bool NeedsRetry(NativeCursorOwner owner) const;
    bool Begin(SDL_Window *window, NativeCursorOwner owner, bool visible, bool rawInput,
               NativeCursorTransaction &transaction);

    private:
    friend class NativeCursorTransaction;
    bool Advance();
    void ClearBinding();
    bool BindingCurrent(SDL_Window *window, SDL_WindowID id, SDL_PropertiesID properties, void *native,
                        std::uint64_t version) const;
    bool Current(const NativeCursorTransaction &transaction) const;
    bool Finish(NativeCursorTransaction &transaction, bool commit);
    NativeMousePlatform platform;
    SDL_Window *window = nullptr;
    SDL_WindowID windowId = 0;
    SDL_PropertiesID properties = 0;
    void *native = nullptr;
    std::uint64_t version = 0;
    NativeCursorOwner pendingOwner = NativeCursorOwner::VGui1;
    bool ready = false;
    bool busy = false;
    bool pending = false;
    bool pendingVisible = false;
    bool pendingRawInput = false;
};

NativeMouseWindow &NativeMouseWindowState();
bool TrySampleNativeMousePosition(SDL_Window *window, int logicalWidth, int logicalHeight, float *x, float *y);
} // namespace cse
