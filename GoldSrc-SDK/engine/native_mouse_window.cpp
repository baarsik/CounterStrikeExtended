#include "native_mouse_window.h"
#include "../common/sdl3_native_mouse_bridge.h"
#include "../common/sdl3_native_cocoa_mouse.h"
#include <cmath>
#include <limits>
#include <type_traits>

namespace cse {
namespace {
NativeMouseWindow state(NativeMouseDefaultPlatform());

bool ValidMode(std::uint32_t mode) {
    return mode == CseSdlMouseModeAbsolute || mode == CseSdlMouseModeRelative;
}
} // namespace

NativeMouseWindow::NativeMouseWindow(const NativeMousePlatform &services) : platform(services) {
}

bool NativeMouseWindow::Advance() {
    if (version == (std::numeric_limits<std::uint64_t>::max)()) {
        return false;
    }
    ++version;
    return true;
}

void NativeMouseWindow::ClearBinding() {
    ready = false;
    pending = false;
    window = nullptr;
    windowId = 0;
    properties = 0;
    native = nullptr;
    cursorRefreshNeeded = true;
    cursorPolicyBound = false;
    cursorPolicyVisible = true;
    committedGameplayCapture = false;
    gameplayKeyboard = false;
    shortcutContextFresh = false;
    shortcutPolicyBound = false;
    shortcutPolicyEnabled = false;
    shortcutPolicyDirty = false;
    retiring = false;
    captureSuspended = false;
    gameplayInputFenced = false;
    gameplayCaptureAttempted = false;
    restorationAcknowledgement = false;
    interactionCached = false;
    interactionActive = true;
    hasCursorIntent = false;
    logicalGrab = false;
    grabIntentKnown = false;
    releaseStage = CaptureRelease::Input;
    interactionResult = CseSdlCocoaMouseNotReady;
    inputCutoff = 0;
    if (platform.interactionOwner) {
        shortcutsSuspended = false;
    }
}

bool NativeMouseWindow::Invalidate(SDL_Window *target) {
    if (!SDL_IsMainThread()) {
        return false;
    }
    if (target && target != window) {
        return true;
    }
    retiring = true;
    if (platform.interactionOwner) {
        InvalidateInteraction();
    }
    if (!Advance()) {
        return false;
    }
    const std::uint64_t expectedVersion = version;
    if (!ApplyGameplayShortcuts(false) || version != expectedVersion) {
        return false;
    }
    const bool wasBusy = busy;
    busy = true;
    const bool released = !cursorPolicyBound || !platform.gameplayCursor || platform.gameplayCursor(native, false);
    busy = wasBusy;
    if (!released || version != expectedVersion) {
        return false;
    }
    ClearBinding();
    // A live outer transaction releases busy. It may never commit the retired generation.
    return true;
}

bool NativeMouseWindow::AdvanceShortcutIntent() {
    shortcutPolicyDirty = shortcutPolicyBound;
    if (shortcutVersion == (std::numeric_limits<std::uint64_t>::max)()) {
        shortcutContextFresh = false;
        return false;
    }
    ++shortcutVersion;
    return true;
}

bool NativeMouseWindow::ApplyGameplayShortcuts(bool enabled) {
    if (!platform.gameplayShortcuts) {
        return true;
    }
    if (!shortcutPolicyBound && !enabled) {
        return !shortcutBusy;
    }
    if (!shortcutPolicyDirty && shortcutPolicyBound && shortcutPolicyEnabled == enabled && !shortcutBusy) {
        return true;
    }
    if (!native || (shortcutBusy && enabled)) {
        return false;
    }
    const std::uint64_t expectedVersion = version;
    const std::uint64_t expectedShortcutVersion = shortcutVersion;
    SDL_Window *const target = window;
    const SDL_WindowID id = windowId;
    const SDL_PropertiesID props = properties;
    void *const handle = native;
    const bool wasBusy = shortcutBusy;
    // Reserve cleanup ownership before native admission, including failed enables.
    // Nested retirement must invalidate the provider even while this setter is live.
    shortcutPolicyBound = true;
    shortcutPolicyDirty = true;
    shortcutBusy = true;
    const bool applied = platform.gameplayShortcuts(native, enabled);
    shortcutBusy = wasBusy;
    const bool current = !enabled ||
                         (ready && !retiring && !busy && !pending && committedGameplayCapture &&
                          gameplayKeyboard && !shortcutsSuspended &&
                          BindingCurrent(target, id, props, handle, expectedVersion) &&
                          SDL_GetKeyboardFocus() == target);
    if (!applied || !current || wasBusy || version != expectedVersion || shortcutVersion != expectedShortcutVersion) {
        return false;
    }
    shortcutPolicyEnabled = enabled;
    shortcutPolicyBound = enabled;
    shortcutPolicyDirty = false;
    return true;
}

bool NativeMouseWindow::RefreshGameplayShortcuts(bool keyboard) {
    if (!SDL_IsMainThread()) {
        return false;
    }
    if (gameplayKeyboard != keyboard) {
        gameplayKeyboard = keyboard;
        if (!AdvanceShortcutIntent()) {
            return false;
        }
    }
    shortcutContextFresh = true;
    const bool enabled = ready && !retiring && !busy && !pending && committedGameplayCapture &&
                         gameplayKeyboard && shortcutContextFresh && !shortcutsSuspended &&
                         BindingCurrent(window, windowId, properties, native, version) &&
                         SDL_GetKeyboardFocus() == window;
    return ApplyGameplayShortcuts(enabled);
}

bool NativeMouseWindow::SetGameplayShortcutsSuspended(bool suspended) {
    if (!SDL_IsMainThread()) {
        return false;
    }
    if (shortcutsSuspended == suspended) {
        return suspended ? ApplyGameplayShortcuts(false) : true;
    }
    shortcutsSuspended = suspended;
    shortcutContextFresh = false;
    if (!AdvanceShortcutIntent()) {
        return false;
    }
    // Resumption preserves logical UI intent but cannot enable using stale context.
    return ApplyGameplayShortcuts(false);
}

namespace {
constexpr std::uint64_t interactionPollNanoseconds = UINT64_C(10000000); // At most one native hit test per 10 ms.
constexpr Uint32 capturedInputTypes[] = {
    SDL_EVENT_KEY_DOWN, SDL_EVENT_KEY_UP, SDL_EVENT_TEXT_EDITING, SDL_EVENT_TEXT_INPUT,
    SDL_EVENT_TEXT_EDITING_CANDIDATES, SDL_EVENT_MOUSE_MOTION, SDL_EVENT_MOUSE_BUTTON_DOWN,
    SDL_EVENT_MOUSE_BUTTON_UP, SDL_EVENT_MOUSE_WHEEL
};

bool KeyboardTextInput(Uint32 type) {
    return type == SDL_EVENT_KEY_DOWN || type == SDL_EVENT_KEY_UP || type == SDL_EVENT_TEXT_EDITING ||
           type == SDL_EVENT_TEXT_INPUT || type == SDL_EVENT_TEXT_EDITING_CANDIDATES;
}

bool CapturedInput(Uint32 type) {
    for (const Uint32 input : capturedInputTypes) {
        if (input == type) {
            return true;
        }
    }
    return false;
}
} // namespace

void NativeMouseWindow::InvalidateInteraction() {
    if (!Advance()) {
        retiring = true;
    }
    interactionCached = false;
    cursorRefreshNeeded = true;
}

bool NativeMouseWindow::CaptureAllowed() const {
    return !platform.interactionOwner ||
           (ready && !retiring && !captureSuspended && interactionActive && interactionCached &&
            interactionResult == CseSdlCocoaMouseReady &&
            BindingCurrent(window, windowId, properties, native, version));
}

bool NativeMouseWindow::InputAllowed(const SDL_Event &event) const {
    if (!platform.interactionOwner || !CapturedInput(event.type)) {
        return true;
    }
    // Unknown timestamps cannot establish that input belongs to the new stream.
    if (shortcutPolicyDirty || shortcutBusy ||
        (inputCutoff && (!event.common.timestamp || event.common.timestamp <= inputCutoff))) {
        return false;
    }
    if (CaptureAllowed()) {
        return true;
    }
    // Interaction result 8 also describes a pointer outside focused UI content.
    // It prohibits mouse capture, but does not establish a gameplay overlay interval.
    // Explicit UI intent cannot waive an interval that already fenced gameplay input.
    return KeyboardTextInput(event.type) && SDL_IsMainThread() && ready && !retiring && !busy && !captureBusy &&
           interactionActive && interactionCached && !gameplayInputFenced && !committedGameplayCapture &&
           !gameplayCaptureAttempted && (!hasCursorIntent || pendingVisible || !pendingGameplayCapture) &&
           (!captureSuspended || releaseStage == CaptureRelease::Complete) &&
           (interactionResult == CseSdlCocoaMouseReady || interactionResult == CseSdlCocoaMouseNotInteractionOwner) &&
           BindingCurrent(window, windowId, properties, native, version) && SDL_GetKeyboardFocus() == window &&
           (SDL_GetWindowFlags(window) & SDL_WINDOW_MINIMIZED) == 0;
}

bool NativeMouseWindow::RefreshInteraction(bool activeApp, bool gameplayContext,
                                           const NativeMouseCaptureHooks &hooks) {
    if (!platform.interactionOwner) {
        return true;
    }
    if (!SDL_IsMainThread() || captureBusy || busy || !ready || retiring) {
        return false;
    }
    const bool activeChanged = interactionActive != activeApp;
    interactionActive = activeApp;
    const std::uint64_t now = SDL_GetTicksNS();
    if (interactionCached && !activeChanged && now - interactionTime < interactionPollNanoseconds) {
        return CaptureAllowed();
    }
    const std::uint64_t expectedVersion = version;
    SDL_Window *const target = window;
    const SDL_WindowID id = windowId;
    const SDL_PropertiesID props = properties;
    void *const handle = native;
    const auto current = [&]() {
        return ready && !retiring && BindingCurrent(target, id, props, handle, expectedVersion);
    };
    captureBusy = true;
    // Scope cleanup must not erase a reentrant window's state.
    struct BusyScope {
        bool &flag;
        ~BusyScope() {
            flag = false;
        }
    } scope{captureBusy};
    if (!current()) {
        return false;
    }
    interactionTime = now;
    const std::uint32_t result = platform.interactionOwner(handle);
    if (!current()) {
        return false;
    }
    interactionResult = result;
    interactionCached = true;
    const bool owned = activeApp && interactionResult == CseSdlCocoaMouseReady;
    if (owned && !captureSuspended) {
        return true;
    }
    if (!captureSuspended && !committedGameplayCapture && !gameplayCaptureAttempted && activeApp &&
        interactionResult == CseSdlCocoaMouseNotInteractionOwner && SDL_GetKeyboardFocus() == target &&
        (SDL_GetWindowFlags(target) & SDL_WINDOW_MINIMIZED) == 0 && current()) {
        // Initial/ordinary UI pointer exit neither releases held keys nor starts
        // an overlay interval. A pending request alone is not effective capture.
        return false;
    }
    // Fence activation and both UI setters before any callback can reenter.
    gameplayInputFenced = gameplayInputFenced || committedGameplayCapture || gameplayCaptureAttempted;
    captureSuspended = true;
    restorationAcknowledgement = false;
    const bool shortcutsReleased = SetGameplayShortcutsSuspended(true);
    if (!current()) {
        return false;
    }
    // Each completed release stage runs once, including under retryable failures.
    if (releaseStage == CaptureRelease::Input) {
        hooks.resetInput();
        // A newer UI intent may supersede the transaction during reset. Finish
        // stopping this same live client before yielding to that intent.
        if (!ready || retiring || !BindingCurrent(target, id, props, handle, version)) {
            return false;
        }
        hooks.setClientMouse(false);
        if (!ready || retiring || !BindingCurrent(target, id, props, handle, version)) {
            return false;
        }
        releaseStage = CaptureRelease::Relative;
        if (!current()) {
            return false;
        }
    }
    if (releaseStage == CaptureRelease::Relative) {
        if (!SDL_SetWindowRelativeMouseMode(target, false) || !current()) {
            return false;
        }
        releaseStage = CaptureRelease::Grab;
    }
    if (releaseStage == CaptureRelease::Grab) {
        if (!SDL_SetWindowMouseGrab(target, false) || !current()) {
            return false;
        }
        releaseStage = CaptureRelease::Presentation;
    }
    if (releaseStage == CaptureRelease::Presentation) {
        if ((platform.gameplayCursor && !platform.gameplayCursor(handle, false)) || !current()) {
            return false;
        }
        cursorPolicyVisible = true;
        releaseStage = CaptureRelease::Cursor;
    }
    if (releaseStage == CaptureRelease::Cursor) {
        if (!SDL_ShowCursor() || !current()) {
            return false;
        }
        releaseStage = CaptureRelease::Complete;
    }
    if (!owned || !shortcutsReleased || !hasCursorIntent ||
        (!gameplayContext && !pendingVisible && pendingGameplayCapture)) {
        return false;
    }
    hooks.resetInput();
    if (!current()) {
        return false;
    }
    // Drain the old native queue before the first reacquisition setter. Lifecycle
    // events survive; an event already fetched by Wait/Poll is fenced by timestamp.
    SDL_PumpEvents();
    if (!current()) {
        return false;
    }
    inputCutoff = SDL_GetTicksNS();
    for (const Uint32 type : capturedInputTypes) {
        SDL_FlushEvent(type);
    }
    if (!current()) {
        return false;
    }
    // Any partially applied restoration must be released again before retrying.
    releaseStage = CaptureRelease::Relative;
    restoringCapture = true;
    NativeCursorTransaction transaction;
    if (!Begin(target, pendingOwner, pendingVisible, pendingRawInput, transaction, pendingGameplayCapture,
               logicalGrab ? NativeMouseGrab::Enabled : NativeMouseGrab::Disabled) || !transaction.Commit()) {
        return false;
    }
    captureSuspended = false;
    restorationAcknowledgement = true;
    releaseStage = CaptureRelease::Input;
    if (!SetGameplayShortcutsSuspended(false)) {
        captureSuspended = true;
        return false;
    }
    gameplayInputFenced = false;
    if (committedGameplayCapture && gameplayContext && CaptureAllowed()) {
        hooks.setClientMouse(true);
    }
    return CaptureAllowed();
}

bool NativeMouseWindow::RefreshCursorPolicy() {
    if (captureSuspended || (platform.interactionOwner && !cursorRefreshNeeded) ||
        !cursorPolicyBound || !platform.gameplayCursor) {
        return true;
    }
    if (!SDL_IsMainThread() || busy || pending ||
        !BindingCurrent(window, windowId, properties, native, version)) {
        return false;
    }
    // AppKit fullscreen transitions can replace the app's presentation mask.
    // Reapply only the owned bit, without repeating SDL mode changes or warps.
    const std::uint64_t expectedVersion = version;
    busy = true;
    const bool refreshed = platform.gameplayCursor(native, !cursorPolicyVisible);
    busy = false;
    if (refreshed && version == expectedVersion && ready) {
        cursorRefreshNeeded = false;
        return true;
    }
    return false;
}

bool NativeMouseWindow::BindingCurrent(SDL_Window *target, SDL_WindowID id, SDL_PropertiesID props, void *handle,
                                       std::uint64_t expectedVersion) const {
    if (!target || !handle || version != expectedVersion || window != target || windowId != id || properties != props ||
        native != handle) {
        return false;
    }
    if (SDL_GetWindowFromID(id) != target ||
        SDL_GetPointerProperty(props, platform.windowProperty, nullptr) != handle) {
        return false;
    }
    return version == expectedVersion && window == target && native == handle;
}

bool NativeMouseWindow::Bind(SDL_Window *target) {
    if (!target || !SDL_IsMainThread() || busy || captureBusy || !platform.windowProperty || !platform.ownedWindow ||
        !platform.focusedWindow || !platform.absolutePosition) {
        return false;
    }
    if ((ready && !Invalidate(nullptr)) || !Advance()) {
        return false;
    }
    ClearBinding();
    busy = true;
    const std::uint64_t expectedVersion = version;
    window = target;
    windowId = SDL_GetWindowID(target);
    properties = SDL_GetWindowProperties(target);
    native = properties ? SDL_GetPointerProperty(properties, platform.windowProperty, nullptr) : nullptr;
    const SDL_WindowID id = windowId;
    const SDL_PropertiesID props = properties;
    void *const handle = native;
    bool success = id && props && handle && BindingCurrent(target, id, props, handle, expectedVersion);
    if (success) {
        success = platform.ownedWindow(handle) && BindingCurrent(target, id, props, handle, expectedVersion);
    }
    if (success) {
        // Before Show/first hidden warp: this also prohibits SDK warp emulation.
        success =
            SDL_SetWindowRelativeMouseMode(target, false) && BindingCurrent(target, id, props, handle, expectedVersion);
    }
    if (success) {
        success = !SDL_GetWindowRelativeMouseMode(target) &&
                  CseSdl_QueryMouseModeV1(CSE_SDL_MOUSE_MODE_CONTRACT_V1) == CseSdlMouseModeAbsolute &&
                  BindingCurrent(target, id, props, handle, expectedVersion);
    }
    if (success && platform.interactionOwner) {
        interactionTime = SDL_GetTicksNS();
        interactionResult = platform.interactionOwner(handle);
        interactionCached = true;
        success = BindingCurrent(target, id, props, handle, expectedVersion);
    }
    ready = success;
    if (!success) {
        ClearBinding();
    }
    busy = false;
    return success;
}

bool NativeMouseWindow::Sample(SDL_Window *target, int width, int height, float *x, float *y) const {
    if (!target || !x || !y || width <= 0 || height <= 0) {
        return false;
    }
    // The provider rejects a foreign/wrong-thread query before any owner/video access.
    const std::uint32_t mode = CseSdl_QueryMouseModeV1(CSE_SDL_MOUSE_MODE_CONTRACT_V1);
    if (!ValidMode(mode) || !ready || busy || pending || !CaptureAllowed() || target != window) {
        return false;
    }
    const std::uint64_t expectedVersion = version;
    const SDL_WindowID id = windowId;
    const SDL_PropertiesID props = properties;
    void *const handle = native;
    if (!BindingCurrent(target, id, props, handle, expectedVersion) || SDL_GetKeyboardFocus() != target) {
        return false;
    }
    float resultX = 0.0f;
    float resultY = 0.0f;
    if (mode == CseSdlMouseModeRelative) {
        if (SDL_GetMouseFocus() != target || !platform.focusedWindow(handle, width, height)) {
            return false;
        }
        SDL_GetMouseState(&resultX, &resultY);
    } else if (!platform.absolutePosition(handle, width, height, &resultX, &resultY)) {
        return false;
    }
    if (!std::isfinite(resultX) || !std::isfinite(resultY) || !ready || busy || pending ||
        !BindingCurrent(target, id, props, handle, expectedVersion)) {
        return false;
    }
    *x = resultX;
    *y = resultY;
    return true;
}

bool NativeMouseWindow::NeedsRetry(NativeCursorOwner owner) const {
    if (platform.interactionOwner && ready && captureSuspended && hasCursorIntent && pendingOwner == owner) {
        // Let the current UI owner publish grab-only changes without applying any
        // capture setters while native interaction belongs elsewhere.
        return true;
    }
#if defined(__APPLE__)
    // Cached UI intent belongs to the previous window generation. Only the first
    // successful request establishes this binding; unchanged inactive owners stay fenced.
    if (ready && !cursorPolicyBound && !pending) {
        return true;
    }
#endif
    return ready && pending && pendingOwner == owner;
}

bool NativeMouseWindow::Begin(SDL_Window *target, NativeCursorOwner owner, bool visible, bool rawInput,
                              NativeCursorTransaction &transaction, bool gameplayCapture, NativeMouseGrab grab) {
    const bool restoring = restoringCapture;
    restoringCapture = false;
    if (!SDL_IsMainThread() || !ready || target != window || transaction.owner ||
        (owner != NativeCursorOwner::VGui1 && owner != NativeCursorOwner::VGui2)) {
        return false;
    }
    // Consume only the exact successfully restored intent. The UI's local cache
    // can acknowledge it without draining motion accepted since restoration.
    if (platform.interactionOwner && restorationAcknowledgement && !busy && !captureBusy && !pending &&
        !shortcutPolicyDirty && releaseStage == CaptureRelease::Input && CaptureAllowed() && pendingOwner == owner &&
        pendingVisible == visible && pendingRawInput == rawInput && pendingGameplayCapture == gameplayCapture &&
        (grab == NativeMouseGrab::Preserve || logicalGrab == (grab == NativeMouseGrab::Enabled))) {
        restorationAcknowledgement = false;
        busy = true;
        transaction.owner = this;
        transaction.version = version;
        transaction.changed = false;
        return true;
    }
    restorationAcknowledgement = false;
    if (busy && pending && pendingOwner == owner && pendingVisible == visible && pendingRawInput == rawInput &&
        pendingGameplayCapture == gameplayCapture && grab == NativeMouseGrab::Preserve) {
        return false;
    }
    if (!Advance()) {
        return false;
    }
    retiring = false;
    pending = true;
    pendingOwner = owner;
    pendingVisible = visible;
    pendingRawInput = rawInput;
    pendingGameplayCapture = gameplayCapture;
    hasCursorIntent = true;
    if (grab != NativeMouseGrab::Preserve) {
        logicalGrab = grab == NativeMouseGrab::Enabled;
        grabIntentKnown = true;
    } else if (platform.interactionOwner && !grabIntentKnown && !captureSuspended) {
        logicalGrab = SDL_GetWindowMouseGrab(target);
        grabIntentKnown = true;
    }
    if (platform.interactionOwner && !restoring && (captureBusy || !CaptureAllowed())) {
        return false;
    }
    // Latest cursor intent wins immediately, even if its mouse setters fail.
    const std::uint64_t requestedVersion = version;
    if (!ApplyGameplayShortcuts(false) || version != requestedVersion) {
        return false;
    }
    if (busy) {
        return false;
    }
    busy = true;
    const std::uint64_t expectedVersion = version;
    const SDL_WindowID id = windowId;
    const SDL_PropertiesID props = properties;
    void *const handle = native;
    const auto current = [&]() {
        return BindingCurrent(target, id, props, handle, expectedVersion);
    };
    bool success = current() && platform.ownedWindow(handle) && current();
#if defined(__APPLE__)
    if (success) {
        // The ownership cutoff precedes mode/grab setters and any caller pump.
        // Retire menu/previous-owner motion once; all subsequently accepted
        // deltas belong to the new stream and must remain for the client.
        SDL_GetRelativeMouseState(nullptr, nullptr);
        success = current();
    }
#endif
    bool relativeCursor = rawInput;
#if defined(__APPLE__)
    // Pinned Cocoa delivers system-accelerated NSEvent deltas in explicit relative
    // mode. Capture the camera without clipping raw-off movement at desktop edges.
    // A custom UI cursor can hide the OS cursor while retaining absolute input.
    relativeCursor = visible ? rawInput : gameplayCapture;
#endif
    if (success && relativeCursor) {
        // Retain existing raw order: show to end emulation, then request explicit mode.
        success = SDL_ShowCursor() && current();
        if (success) {
            if (platform.interactionOwner && !visible && gameplayCapture) {
                // A failed native setter may already have applied relative mode.
                // Merely recording a blocked pending request must not set this marker.
                gameplayCaptureAttempted = true;
            }
            success = SDL_SetWindowRelativeMouseMode(target, !visible) && current();
        }
    } else if (success) {
        // Retain existing nonraw order: prohibit/disable, then show or hide.
        success = SDL_SetWindowRelativeMouseMode(target, false) && current();
        if (success) {
            success = (visible ? SDL_ShowCursor() : SDL_HideCursor()) && current();
        }
    }
    if (success) {
        const bool requestedRelative = relativeCursor && !visible;
        const bool expectedRelative = requestedRelative && SDL_GetKeyboardFocus() == target;
        const std::uint32_t expectedMode = expectedRelative ? CseSdlMouseModeRelative : CseSdlMouseModeAbsolute;
        success = SDL_GetWindowRelativeMouseMode(target) == requestedRelative &&
                  CseSdl_QueryMouseModeV1(CSE_SDL_MOUSE_MODE_CONTRACT_V1) == expectedMode && current();
    }
#if defined(__APPLE__)
    if (success && (grab != NativeMouseGrab::Preserve || restoring)) {
        success = SDL_SetWindowMouseGrab(target, logicalGrab) && current();
    }
#endif
    if (!success) {
        busy = false;
        return false;
    }
    transaction.owner = this;
    transaction.changed = true;
    transaction.version = expectedVersion;
    return true;
}

bool NativeMouseWindow::Current(const NativeCursorTransaction &transaction) const {
    return transaction.owner == this && ready && busy && (pending || !transaction.changed) &&
           version == transaction.version;
}

bool NativeMouseWindow::Finish(NativeCursorTransaction &transaction, bool commit) {
    bool success = commit && Current(transaction);
    if (success && transaction.changed && platform.gameplayCursor) {
        // Reserve ownership before calling the platform: invalidation may reenter
        // synchronously while AppKit applies the presentation options.
        cursorPolicyBound = true;
        cursorPolicyVisible = pendingVisible;
        success = platform.gameplayCursor(native, !pendingVisible) && Current(transaction);
    }
    if (success && transaction.changed) {
        cursorPolicyBound = true;
        cursorPolicyVisible = pendingVisible;
        cursorRefreshNeeded = false;
        committedGameplayCapture = !pendingVisible && pendingGameplayCapture;
        gameplayCaptureAttempted = committedGameplayCapture;
        pending = false;
    }
    busy = false;
    transaction.owner = nullptr;
    return success;
}

NativeCursorTransaction::~NativeCursorTransaction() {
    if (owner) {
        owner->Finish(*this, false);
    }
}

bool NativeCursorTransaction::Current() const {
    return owner && owner->Current(*this);
}

bool NativeCursorTransaction::Changed() const {
    return changed;
}

bool NativeCursorTransaction::Commit() {
    return owner && owner->Finish(*this, true);
}

NativeMouseWindow &NativeMouseWindowState() {
    static_assert(std::is_trivially_destructible<NativeMouseWindow>::value, "No native window static cleanup");
    return state;
}
bool TrySampleNativeMousePosition(SDL_Window *window, int width, int height, float *x, float *y) {
    return state.Sample(window, width, height, x, y);
}
} // namespace cse
