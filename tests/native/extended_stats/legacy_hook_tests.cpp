#include <array>
#include <cstdio>
#include <cstdlib>
#include <vector>
#include "stats_state.h"
#include "legacy_numbers.h"
struct edict_t {
    int index;
};
#define ENTINDEX(entity) ((entity)->index)
enum {
    MRES_IGNORED,
    MRES_SUPERCEDE
};
struct Meta {
    int status = MRES_IGNORED;
} meta;
static Meta* gpMetaGlobals = &meta;
static int result = MRES_IGNORED;
#define RETURN_META(value)                                                                                             \
    do {                                                                                                               \
        result = value;                                                                                                \
        return;                                                                                                        \
    } while (false)
struct Globals {
    int maxClients = 4;
} globals;
static Globals* gpGlobals = &globals;
static std::vector<int> rawWrites;
static void Write(int value) {
    rawWrites.push_back(value);
}
struct EngineFunctions {
    void (*pfnWriteByte)(int) = Write;
    void (*pfnWriteShort)(int) = Write;
} g_engfuncs;
namespace cse::extstats {
bool Publishing = false;
bool MessageOpen = false;
int HealthId = 70;
int ArmorId = 71;
int SpecHealthId = 72;
int SpecHealth2Id = 73;
int HltvId = 74;
int ResetHudId = 75;
int InitHudId = 76;
struct Packet {
    int recipient;
    int subject;
    int health;
    int armor;
};
struct Host : ServerHost {
    std::array<Player, 5> players{};
    std::vector<Packet> legacy;
    int MaxPlayers() const override {
        return 4;
    }
    Player ReadPlayer(int index) const override {
        return index > 0 && index <= 4 ? players[index] : Player{};
    }
    void AssignHealth(int, float) override {
    }
    void AssignArmor(int, float) override {
    }
    void Kill(int) override {
    }
    void Query(int, const char*, int) override {
    }
    void Emit(int, int, Stat, std::int32_t) override {
    }
    void EmitLegacy(int recipient, int subject, std::int32_t health, std::int32_t armor) override {
        legacy.push_back({recipient, subject, LegacyAmount(Stat::Health, health), LegacyAmount(Stat::Armor, armor)});
    }
} host;
ServerState State(host);
Player ReadEntityPlayer(int index) {
    return host.ReadPlayer(index);
}
} // namespace cse::extstats
#include "legacy_hook_bodies.inc"
int main() {
    using namespace cse::extstats;
    host.players[1] = {true, false, false, true, 100, 0, 0};
    host.players[2] = {true, false, false, false, 0, 0, 1};
    State.Frame(0, false, false);
    host.legacy.clear();
    edict_t owner{1};
    gpMetaGlobals->status = MRES_SUPERCEDE; // An earlier AMXX hook has captured the original message.
    host.players[1].armor = 100000;
    StatsMessageBegin(1, ArmorId, nullptr, &owner);
    StatsWriteShort(100000);
    if (!rawWrites.empty()) {
        return 1;
    } // Never write a partial message behind AMXX's buffer.
    StatsMessageEnd();
    State.Frame(0.1, false, false);
    if (host.legacy.empty() || host.legacy.front().armor != 999) {
        std::fputs("FAIL: buffered Battery did not queue a safe manual-mode fallback\n", stderr);
        return 1;
    }
    host.legacy.clear();
    edict_t observer{2};
    host.players[1].health = 100000;
    StatsMessageBegin(1, SpecHealthId, nullptr, &observer);
    StatsWriteByte(100000);
    StatsMessageEnd();
    State.Frame(0.2, false, false);
    bool targetFallback = false;
    for (const Packet& packet : host.legacy) {
        targetFallback = targetFallback || (packet.recipient == 2 && packet.subject == 1 && packet.health == 255);
    }
    if (!targetFallback) {
        std::fputs("FAIL: spectator normalization refreshed the observer instead of the stat subject\n", stderr);
        return 1;
    }
    return 0;
}
