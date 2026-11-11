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
    bool (*gameplayCursor)(void *nativeWindow, bool hidden) = nullptr;
    bool (*gameplayShortcuts)(void *nativeWindow, bool enabled) = nullptr;
    std::uint32_t (*interactionOwner)(void *nativeWindow) = nullptr;
};

NativeMousePlatform NativeMouseDefaultPlatform();

enum class NativeCursorOwner {
    VGui1,
    VGui2
};
enum class NativeMouseGrab { Preserve, Disabled, Enabled };

// Borrowed synchronous engine services; never stored by the window owner.
struct NativeMouseCaptureHooks {
    void (*resetInput)();
    void (*setClientMouse)(bool active);
};
class NativeMouseWindow;

class NativeCursorTransaction {
    public:
    NativeCursorTransaction() = default;
    ~NativeCursorTransaction();
    NativeCursorTransaction(const NativeCursorTransaction &) = delete;
    NativeCursorTransaction &operator=(const NativeCursorTransaction &) = delete;
    bool Current() const;
    bool Changed() const;
    bool Commit();

    private:
    friend class NativeMouseWindow;
    bool changed = true;
    NativeMouseWindow *owner = nullptr;
    std::uint64_t version = 0;
};

class NativeMouseWindow {
    public:
    explicit NativeMouseWindow(const NativeMousePlatform &platform);
    NativeMouseWindow(const NativeMouseWindow &) = delete;
    NativeMouseWindow &operator=(const NativeMouseWindow &) = delete;
    bool Bind(SDL_Window *window);
    bool Invalidate(SDL_Window *window);
    bool RefreshCursorPolicy();
    bool RefreshGameplayShortcuts(bool gameplayKeyboard);
    bool SetGameplayShortcutsSuspended(bool suspended);
    bool RefreshInteraction(bool activeApp, bool gameplayContext, const NativeMouseCaptureHooks &hooks);
    void InvalidateInteraction();
    bool CaptureAllowed() const;
    bool InputAllowed(const SDL_Event &event) const;
    bool Sample(SDL_Window *window, int logicalWidth, int logicalHeight, float *x, float *y) const;
    bool NeedsRetry(NativeCursorOwner owner) const;
    bool Begin(SDL_Window *window, NativeCursorOwner owner, bool visible, bool rawInput,
               NativeCursorTransaction &transaction, bool gameplayCapture = true,
               NativeMouseGrab grab = NativeMouseGrab::Preserve);

    private:
    friend class NativeCursorTransaction;
    bool Advance();
    bool AdvanceShortcutIntent();
    bool ApplyGameplayShortcuts(bool enabled);
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
    bool pendingGameplayCapture = true;
    bool committedGameplayCapture = false;
    bool gameplayKeyboard = false;
    bool shortcutsSuspended = false;
    bool shortcutContextFresh = false;
    bool shortcutPolicyBound = false;
    bool shortcutPolicyEnabled = false;
    bool shortcutPolicyDirty = false;
    bool shortcutBusy = false;
    bool retiring = false;
    std::uint64_t shortcutVersion = 0;
    bool captureSuspended = false;
    bool gameplayInputFenced = false;
    bool gameplayCaptureAttempted = false;
    bool captureBusy = false;
    bool restoringCapture = false;
    bool restorationAcknowledgement = false;
    bool interactionCached = false;
    bool interactionActive = true;
    bool hasCursorIntent = false;
    bool logicalGrab = false;
    bool grabIntentKnown = false;
    enum class CaptureRelease { Input, Relative, Grab, Presentation, Cursor, Complete };
    CaptureRelease releaseStage = CaptureRelease::Input;
    std::uint32_t interactionResult = 0;
    std::uint64_t interactionTime = 0;
    std::uint64_t inputCutoff = 0;
    bool cursorRefreshNeeded = true;
    bool cursorPolicyBound = false;
    bool cursorPolicyVisible = true;
};

NativeMouseWindow &NativeMouseWindowState();
// Refresh current engine UI intent immediately before any native event dispatch.
bool RefreshNativeGameplayShortcuts();
bool RefreshNativeMouseCapture();
#if defined(__APPLE__)
// Deferred listen-owner teardown is resumed by the native root-close boundary.
bool BeginNativeListenShutdown(bool releaseVideo, bool shutdownGame, bool shutdownVideo);
bool FinishNativeListenShutdown();
#endif
bool TrySampleNativeMousePosition(SDL_Window *window, int logicalWidth, int logicalHeight, float *x, float *y);
} // namespace cse
