#include <cstdio>
#if __has_include("legacy_numbers.h")
#include "legacy_numbers.h"
int main() {
    using namespace cse::extstats;
    struct Row {
        std::int32_t amount;
        int health;
        int armor;
        int broadcast;
    };
    const Row rows[] = {{-1, 0, 0, 0},         {0, 0, 0, 0},           {1, 1, 1, 1},
                        {100, 100, 100, 100},  {256, 255, 256, 127},   {999, 255, 999, 127},
                        {1000, 255, 999, 127}, {65536, 255, 999, 127}, {2147483647, 255, 999, 127}};
    for (const Row& row : rows) {
        if (LegacyAmount(Stat::Health, row.amount) != row.health ||
            LegacyAmount(Stat::Armor, row.amount) != row.armor ||
            LegacyAmount(Stat::Health, row.amount, true) != row.broadcast) {
            std::fputs("FAIL: legacy numeric safety\n", stderr);
            return 1;
        }
    }
    return 0;
}
#else
int main() {
    std::fputs("FAIL: legacy normalization missing\n", stderr);
    return 1;
}
#endif
