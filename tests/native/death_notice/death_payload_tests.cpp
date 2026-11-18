#include <cstdlib>
#include <cstdint>
#include <cstring>

#include "death_message_extra.h"

#define CHECK(condition) do { if (!(condition)) { std::abort(); } } while (false)

int main() {
    constexpr std::uint8_t legacy[] = {1, 2, 0, 'a', 'k', '4', '7', 0};
    auto extra = ParseDeathMessageExtra(legacy, sizeof(legacy));
    CHECK(!extra.present);

    constexpr std::uint8_t extended[] = {
        1, 2, 1, 'a', 'k', '4', '7', 0,
        7, 0, 0, 0,                         // position, assistant, rarity
        0x10, 0x00, 0xf0, 0xff, 0x08, 0x00, // (2, -2, 1) in eighths
        3, 0x04, 0x03, 0x02, 0x01
    };
    extra = ParseDeathMessageExtra(extended, sizeof(extended));
    CHECK(extra.present && extra.has_position);
    CHECK(extra.position[0] == 2.0f && extra.position[1] == -2.0f && extra.position[2] == 1.0f);
    CHECK(extra.assistant == 3 && extra.rarity == 0x01020304u);

    constexpr std::uint8_t assistant_only[] = {1, 2, 0, 'm', '4', 'a', '1', 0, 2, 0, 0, 0, 4};
    extra = ParseDeathMessageExtra(assistant_only, sizeof(assistant_only));
    CHECK(extra.present && !extra.has_position && extra.assistant == 4 && extra.rarity == 0);

    extra = ParseDeathMessageExtra(extended, sizeof(extended) - 1);
    CHECK(!extra.present && !extra.has_position && extra.assistant == 0);
    constexpr std::uint8_t no_terminator[] = {1, 2, 0, 'a', 'k', '4', '7'};
    CHECK(!ParseDeathMessageExtra(no_terminator, sizeof(no_terminator)).present);

    char badges[64];
    FormatDeathRarity(regamedll_death::kHeadshot | regamedll_death::kNoScope |
                          regamedll_death::kThroughSmoke | regamedll_death::kRevenge, badges, sizeof(badges));
    CHECK(std::strcmp(badges, " [NS] [SMOKE] [REV]") == 0);
}
