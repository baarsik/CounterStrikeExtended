#ifndef CSE_HUD_STAT_NUMBERS_H
#define CSE_HUD_STAT_NUMBERS_H
#include "cse_extended_stats_protocol.h"

namespace cse::extstats {
inline int StatPositions(std::int32_t amount) {
    const int displayed = DisplayAmount(amount);
    constexpr int FourDigitMinimum = 1000;
    constexpr int FiveDigitMinimum = 10000;
    return displayed >= FiveDigitMinimum ? 5 : displayed >= FourDigitMinimum ? 4 : 3;
}

inline int ArmorAnchor(int minimum, int crossWidth, int digitWidth, std::int32_t health) {
    const int healthRight = crossWidth + digitWidth / 2 + StatPositions(health) * digitWidth;
    const int afterHealth = healthRight + digitWidth;
    return minimum > afterHealth ? minimum : afterHealth;
}
} // namespace cse::extstats
#endif
