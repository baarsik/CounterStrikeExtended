#include <windows.h>
#include "NativeKeyState.h"
#include <type_traits>
namespace keys = cse_vgui2_input;
static_assert(sizeof(SHORT) == 2 && std::is_signed<SHORT>::value, "Windows packed key-state return");
static_assert(std::is_same<decltype(&::GetKeyState), SHORT (WINAPI*)(int)>::value, "Actual API call type");
static_assert(static_cast<int>(keys::VirtualKey::CapsLock) == VK_CAPITAL, "SDK CapsLock numeric domain");
static_assert(static_cast<int>(keys::VirtualKey::NumLock) == VK_NUMLOCK, "SDK NumLock numeric domain");
static_assert(static_cast<int>(keys::VirtualKey::ScrollLock) == VK_SCROLL, "SDK ScrollLock numeric domain");
static_assert(static_cast<int>(keys::VirtualKey::LeftShift) == VK_LSHIFT, "SDK left Shift numeric domain");
static_assert(static_cast<int>(keys::VirtualKey::RightShift) == VK_RSHIFT, "SDK right Shift numeric domain");
static_assert(static_cast<int>(keys::VirtualKey::LeftControl) == VK_LCONTROL, "SDK left Control numeric domain");
static_assert(static_cast<int>(keys::VirtualKey::RightControl) == VK_RCONTROL, "SDK right Control numeric domain");
static_assert(static_cast<int>(keys::VirtualKey::LeftAlt) == VK_LMENU, "SDK left Alt numeric domain");
static_assert(static_cast<int>(keys::VirtualKey::RightAlt) == VK_RMENU, "SDK right Alt numeric domain");
static_assert(static_cast<int>(keys::VirtualKey::Alt) == VK_MENU, "SDK aggregate Alt numeric domain");
static_assert(static_cast<int>(keys::VirtualKey::Shift) == VK_SHIFT, "SDK aggregate Shift numeric domain");
static_assert(static_cast<int>(keys::VirtualKey::Control) == VK_CONTROL, "SDK aggregate Control numeric domain");
