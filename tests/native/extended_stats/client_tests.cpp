#include <cstdio>
#include <cstdlib>
#include <cstring>
#if __has_include("extended_stats.h")
#include "extended_stats.h"
using namespace cse::extstats;
static void Check(bool condition, const char* why) {
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", why);
        std::exit(1);
    }
}
int main(int, char** argv) {
    ClientState state;
    const unsigned char high[] = {1, 0x9f, 0x86, 1, 0};
    const unsigned char invalid[] = {1, 255, 255, 255, 255};
    int player = 0;
    if (std::strcmp(argv[1], "message_validation") == 0) {
        Check(!state.Apply(Stat::Health, high, 4, 32, player), "reject short packet");
        Check(!state.Get(Stat::Health, 1).valid, "bad packet does not mutate state");
        Check(state.Apply(Stat::Health, high, 5, 32, player), "accept valid packet");
        Check(player == 1 && state.Get(Stat::Health, 1).amount == 99999, "full integer stored");
        const unsigned char negative[] = {1, 254, 255, 255, 255};
        Check(!state.Apply(Stat::Health, negative, 5, 32, player), "reject unsupported negative");
        Check(state.Get(Stat::Health, 1).amount == 99999, "bad update retains amount");
    } else if (std::strcmp(argv[1], "legacy_precedence") == 0) {
        state.Apply(Stat::Health, high, 5, 32, player);
        Check(state.Legacy(Stat::Health, 1, 0) == 99999, "wrapped zero cannot replace extended health");
        state.Legacy(Stat::Armor, 1, 100);
        Check(state.Get(Stat::Armor, 1).amount == 100, "stats are independent");
    } else if (std::strcmp(argv[1], "invalidation_fallback") == 0) {
        state.Apply(Stat::Health, high, 5, 32, player);
        state.Legacy(Stat::Health, 1, 255);
        state.Apply(Stat::Health, invalid, 5, 32, player);
        Check(state.Get(Stat::Health, 1).amount == 255 && !state.HasExtended(Stat::Health, 1),
              "latest fallback restored");
    } else if (std::strcmp(argv[1], "local_alive_state") == 0) {
        state.Apply(Stat::Health, high, 5, 32, player);
        Check(state.Legacy(Stat::Health, 1, 0) > 0, "canonical local amount stays alive");
        const unsigned char death[] = {1, 0, 0, 0, 0};
        state.Apply(Stat::Health, death, 5, 32, player);
        Check(state.Get(Stat::Health, 1).amount == 0, "real extended death accepted");
        Check(!state.Get(Stat::Armor, 2).valid, "foreign stat not invented");
    } else {
        state.Apply(Stat::Armor, high, 5, 32, player);
        state.ResetPlayer(1);
        Check(!state.Get(Stat::Armor, 1).valid, "slot reset clears ownership and fallback");
        state.Apply(Stat::Armor, high, 5, 32, player);
        state.AdvanceTime(100, true);
        Check(state.AdvanceTime(10, true) && !state.Get(Stat::Armor, 1).valid, "demo reversal clears stale state");
        Check(!state.Get(Stat::Health, -1).valid && !state.Get(Stat::Health, 256).valid, "invalid lookup safe");
    }
    return 0;
}
#else
int main() {
    std::fputs("FAIL: client extended stats state missing\n", stderr);
    return 1;
}
#endif
