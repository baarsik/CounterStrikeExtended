#ifndef CSE_CLIENT_EXTENDED_STATS_H
#define CSE_CLIENT_EXTENDED_STATS_H

#include <array>
#include "cse_extended_stats_protocol.h"

namespace cse::extstats {

class ClientState {
  public:
    bool Apply(Stat stat, const void* buffer, std::size_t size, int maxPlayers, int& player) {
        std::int32_t amount = 0;
        int subject = 0;
        if (!DecodeMessage(buffer, size, maxPlayers, subject, amount)) {
            return false;
        }
        Slot& slot = slots_[subject][static_cast<int>(stat)];
        slot.extended = {amount, amount != InvalidAmount};
        player = subject;
        return true;
    }

    std::int32_t Legacy(Stat stat, int player, std::int32_t amount) {
        if (!Valid(player)) {
            return 0;
        }
        slots_[player][static_cast<int>(stat)].legacy = {amount, true};
        return Get(stat, player).amount;
    }

    std::int32_t ScoreboardHealth(int player, std::int32_t amount) {
        if (!Valid(player)) {
            return 0;
        }
        slots_[player][static_cast<int>(Stat::Health)].scoreboard = {amount, amount >= 0};
        return Get(Stat::Health, player).amount;
    }

    Value Get(Stat stat, int player) const {
        if (!Valid(player)) {
            return {};
        }
        const Slot& slot = slots_[player][static_cast<int>(stat)];
        if (slot.extended.valid) {
            return slot.extended;
        }
        if (stat == Stat::Health && slot.scoreboard.valid) {
            return slot.scoreboard;
        }
        return slot.legacy;
    }

    bool HasExtended(Stat stat, int player) const {
        return Valid(player) && slots_[player][static_cast<int>(stat)].extended.valid;
    }

    void ResetPlayer(int player) {
        if (Valid(player)) {
            slots_[player] = {};
        }
    }

    void Reset() {
        slots_ = {};
        hasTime_ = false;
    }

    bool AdvanceTime(double now, bool demo) {
        const bool reversed = demo && hasTime_ && now < lastTime_;
        if (reversed) {
            Reset();
        }
        lastTime_ = now;
        hasTime_ = true;
        return reversed;
    }

  private:
    struct Slot {
        Value extended;
        Value scoreboard;
        Value legacy;
    };
    static bool Valid(int player) {
        return player > 0 && player <= MaxPlayerIndex;
    }
    std::array<std::array<Slot, 2>, MaxPlayerIndex + 1> slots_{};
    double lastTime_ = 0;
    bool hasTime_ = false;
};

ClientState& ClientStats();
} // namespace cse::extstats

int CSE_LocalPlayerIndex();
bool CSE_ResolveLocalAlive(bool legacyAlive);
bool CSE_HandleExtendedStats(cse::extstats::Stat stat, const void* buffer, int size);
void CSE_UpdateLegacyHealth(int player, int health, bool localHud = false);
void CSE_UpdateScoreboardHealth(int player, int health);
void CSE_UpdateLegacyArmor(int armor);
void CSE_ResetExtendedStats(bool allPlayers);
void CSE_ThinkExtendedStats();

#endif
