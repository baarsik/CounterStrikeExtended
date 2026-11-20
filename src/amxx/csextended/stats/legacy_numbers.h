#ifndef CSE_EXTSTATS_LEGACY_NUMBERS_H
#define CSE_EXTSTATS_LEGACY_NUMBERS_H
#include "cse_extended_stats_protocol.h"

namespace cse::extstats {
inline int LegacyAmount(Stat stat, std::int32_t amount, bool broadcast = false) {
    const int limit = stat == Stat::Armor ? LegacyArmorMaximum
                      : broadcast         ? BroadcastHealthMaximum
                                          : LegacyHealthMaximum;
    return amount <= 0 ? 0 : amount > limit ? limit : amount;
}
} // namespace cse::extstats
#endif
