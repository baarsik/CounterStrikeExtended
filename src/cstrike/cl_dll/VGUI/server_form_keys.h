#pragma once

#include "../../../public/keydefs.h"

#include <array>

namespace cse::forms {

constexpr char FormHotkey(int key) {
    if ((key >= 'a' && key <= 'z') || (key >= '0' && key <= '9')) {
        return static_cast<char>(key);
    }
    if (key >= 'A' && key <= 'Z') {
        return static_cast<char>(key - 'A' + 'a');
    }
    // GoldSrc assigns keypad digits the distinct navigation codes irrespective of Num Lock.
    constexpr std::array NumberPadDigits{'7', '8', '9', '4', '5', '6', '1', '2', '3'};
    if (key >= K_KP_HOME && key <= K_KP_PGDN) {
        return NumberPadDigits[key - K_KP_HOME];
    }
    return key == K_KP_INS ? '0' : '\0';
}

}
