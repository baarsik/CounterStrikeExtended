#ifndef CSE_VGUI2_NATIVE_KEY_STATE_H
#define CSE_VGUI2_NATIVE_KEY_STATE_H

#include <cstdint>

namespace cse_vgui2_input {
// Private finite Win32 virtual-key domain used by InputWin32's retained table.
// These are numeric key IDs, not platform handles or SDL keycodes.
enum class VirtualKey : int {
    Shift = 0x10,
    Control = 0x11,
    Alt = 0x12,
    CapsLock = 0x14,
    NumLock = 0x90,
    ScrollLock = 0x91,
    LeftShift = 0xa0,
    RightShift = 0xa1,
    LeftControl = 0xa2,
    RightControl = 0xa3,
    LeftAlt = 0xa4,
    RightAlt = 0xa5
};

// Captured on the event owner's thread without pumping/reentering SDL. No SDL
// array pointer, provider object or resource ownership survives the capture.
struct Snapshot {
    bool capsLockDown;
    bool numLockDown;
    bool scrollLockDown;
    bool leftShiftDown;
    bool rightShiftDown;
    bool leftControlDown;
    bool rightControlDown;
    bool leftAltDown;
    bool rightAltDown;
    bool capsLockToggled;
    bool numLockToggled;
    bool scrollLockToggled;
};

#if defined(__APPLE__)
// The standalone event owner borrows this snapshot only while dispatching one
// native event. SDL_GetKeyboardState otherwise reflects the end of a pumped batch.
inline thread_local const Snapshot* standaloneEventSnapshot = nullptr;
// Synchronize the active retained context without fabricating key callbacks for
// native modifier events that have no window target.
void ApplyStandaloneModifiers(const Snapshot& snapshot, bool leftCommandDown, bool rightCommandDown);
#endif

constexpr std::uint16_t PackedState(bool down, bool toggled = false) noexcept {
    constexpr std::uint16_t DownMask = 0x8000;
    constexpr std::uint16_t ToggleMask = 0x0001;
    return static_cast<std::uint16_t>((down ? DownMask : 0) | (toggled ? ToggleMask : 0));
}

inline bool TryGetKeyState(const Snapshot& snapshot, int virtualKey, std::uint16_t& output) noexcept {
    switch (virtualKey) {
    case static_cast<int>(VirtualKey::CapsLock):
        output = PackedState(snapshot.capsLockDown, snapshot.capsLockToggled);
        break;
    case static_cast<int>(VirtualKey::NumLock):
        output = PackedState(snapshot.numLockDown, snapshot.numLockToggled);
        break;
    case static_cast<int>(VirtualKey::ScrollLock):
        output = PackedState(snapshot.scrollLockDown, snapshot.scrollLockToggled);
        break;
    case static_cast<int>(VirtualKey::LeftShift):
        output = PackedState(snapshot.leftShiftDown);
        break;
    case static_cast<int>(VirtualKey::RightShift):
        output = PackedState(snapshot.rightShiftDown);
        break;
    case static_cast<int>(VirtualKey::LeftControl):
        output = PackedState(snapshot.leftControlDown);
        break;
    case static_cast<int>(VirtualKey::RightControl):
        output = PackedState(snapshot.rightControlDown);
        break;
    case static_cast<int>(VirtualKey::LeftAlt):
        output = PackedState(snapshot.leftAltDown);
        break;
    case static_cast<int>(VirtualKey::RightAlt):
        output = PackedState(snapshot.rightAltDown);
        break;
    case static_cast<int>(VirtualKey::Alt):
        output = PackedState(snapshot.leftAltDown || snapshot.rightAltDown);
        break;
    case static_cast<int>(VirtualKey::Shift):
        output = PackedState(snapshot.leftShiftDown || snapshot.rightShiftDown);
        break;
    case static_cast<int>(VirtualKey::Control):
        output = PackedState(snapshot.leftControlDown || snapshot.rightControlDown);
        break;
    default:
        return false;
    }
    return true;
}
}
#endif
