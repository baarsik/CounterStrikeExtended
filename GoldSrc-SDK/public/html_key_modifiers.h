#pragma once

namespace cse {
namespace html {

// Native KeyDown/KeyUp retain the legacy Alt=1, Control=2 and Shift=4 bits.
// This bit preserves keypad location when Enter shares VK_RETURN with Return.
constexpr int KeypadModifier = 8;

}
}
