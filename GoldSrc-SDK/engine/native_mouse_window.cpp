#include "native_mouse_window.h"
#include "../common/sdl3_native_mouse_bridge.h"
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
        ClearBinding();
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
}

void NativeMouseWindow::Invalidate(SDL_Window *target) {
    if (!SDL_IsMainThread() || (target && target != window)) {
        return;
    }
    Advance();
    ClearBinding();
    // A live outer transaction releases busy. It may never commit the retired generation.
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
    if (!target || !SDL_IsMainThread() || busy || !platform.windowProperty || !platform.ownedWindow ||
        !platform.focusedWindow || !platform.absolutePosition || !Advance()) {
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
    if (!ValidMode(mode) || !ready || busy || pending || target != window) {
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
    return ready && pending && pendingOwner == owner;
}

bool NativeMouseWindow::Begin(SDL_Window *target, NativeCursorOwner owner, bool visible, bool rawInput,
                              NativeCursorTransaction &transaction) {
    if (!SDL_IsMainThread() || !ready || target != window || transaction.owner ||
        (owner != NativeCursorOwner::VGui1 && owner != NativeCursorOwner::VGui2)) {
        return false;
    }
    if (busy && pending && pendingOwner == owner && pendingVisible == visible && pendingRawInput == rawInput) {
        return false;
    }
    if (!Advance()) {
        return false;
    }
    pending = true;
    pendingOwner = owner;
    pendingVisible = visible;
    pendingRawInput = rawInput;
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
    if (success && rawInput) {
        // Retain existing raw order: show to end emulation, then request explicit mode.
        success = SDL_ShowCursor() && current();
        if (success) {
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
        const bool requestedRelative = rawInput && !visible;
        const bool expectedRelative = requestedRelative && SDL_GetKeyboardFocus() == target;
        const std::uint32_t expectedMode = expectedRelative ? CseSdlMouseModeRelative : CseSdlMouseModeAbsolute;
        success = SDL_GetWindowRelativeMouseMode(target) == requestedRelative &&
                  CseSdl_QueryMouseModeV1(CSE_SDL_MOUSE_MODE_CONTRACT_V1) == expectedMode && current();
    }
    if (!success) {
        busy = false;
        return false;
    }
    transaction.owner = this;
    transaction.version = expectedVersion;
    return true;
}

bool NativeMouseWindow::Current(const NativeCursorTransaction &transaction) const {
    return transaction.owner == this && ready && busy && pending && version == transaction.version;
}

bool NativeMouseWindow::Finish(NativeCursorTransaction &transaction, bool commit) {
    const bool success = commit && Current(transaction);
    if (success) {
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
