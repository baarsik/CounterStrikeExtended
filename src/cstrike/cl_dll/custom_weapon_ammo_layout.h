#ifndef CSE_CUSTOM_WEAPON_AMMO_LAYOUT_H
#define CSE_CUSTOM_WEAPON_AMMO_LAYOUT_H

#include <algorithm>
#include <cstdint>

namespace cse::weapons {

struct AmmoHudRows {
    int primaryY;
    int secondaryY;
    int rightEdge;
};

inline AmmoHudRows AmmoHudLayout(int screenWidth, int screenHeight, int fontHeight, int digitWidth,
                                 int primaryIconHeight, int secondaryIconHeight) {
    const int primaryY = screenHeight - fontHeight - fontHeight / 2;
    const int rowHeight = (std::max)({fontHeight, primaryIconHeight, secondaryIconHeight});
    return {primaryY, primaryY - rowHeight - fontHeight / 4, screenWidth - digitWidth};
}

inline int MoneyHudY(int stockY, const AmmoHudRows* customRows) {
    return customRows ? stockY - (customRows->primaryY - customRows->secondaryY) : stockY;
}

inline int AmmoDigits(std::uint16_t count) {
    constexpr int decimalBase = 10;
    int digits = 1;
    while (count >= decimalBase) {
        count /= decimalBase;
        ++digits;
    }
    return digits;
}

} // namespace cse::weapons

#endif
