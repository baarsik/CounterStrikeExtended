#include <algorithm>
#include "extended_stats.h"
#include "hud.h"
#include "cl_util.h"
#include "demo_api.h"
#include "../public/game_controls/iviewport.h"

namespace cse::extstats {
ClientState& ClientStats() {
    static ClientState state;
    return state;
}
} // namespace cse::extstats

int CSE_LocalPlayerIndex() {
    const cl_entity_t* player = gEngfuncs.GetLocalPlayer();
    return player ? player->index : 0;
}

static bool ValidPlayer(int player) {
    return player > 0 && player <= gEngfuncs.GetMaxClients() && player <= MAX_PLAYERS;
}

static void ApplyHealth(int player, bool localHud) {
    using namespace cse::extstats;
    if (!ValidPlayer(player)) {
        return;
    }
    const Value value = ClientStats().Get(Stat::Health, player);
    g_PlayerExtraInfo[player].health = value.amount;
    if (localHud && player == CSE_LocalPlayerIndex()) {
        gHUD.m_Health.SetHealth(value.amount);
    }
}

bool CSE_HandleExtendedStats(cse::extstats::Stat stat, const void* buffer, int size) {
    using namespace cse::extstats;
    CSE_ThinkExtendedStats();
    const int maximum = (std::min)(gEngfuncs.GetMaxClients(), static_cast<int>(MAX_PLAYERS));
    int player = 0;
    if (!ClientStats().Apply(stat, buffer, static_cast<std::size_t>(size), maximum, player)) {
        return false;
    }
    if (stat == Stat::Health) {
        ApplyHealth(player, true);
    } else if (player == CSE_LocalPlayerIndex()) {
        gHUD.m_Battery.SetArmor(ClientStats().Get(Stat::Armor, player).amount);
    }
    if (gViewPortInterface && g_iUser1 != 0 && player == g_iUser2) {
        gViewPortInterface->UpdateSpectatorPanel();
    }
    return true;
}

void CSE_UpdateLegacyHealth(int player, int health, bool localHud) {
    CSE_ThinkExtendedStats();
    if (ValidPlayer(player)) {
        cse::extstats::ClientStats().Legacy(cse::extstats::Stat::Health, player, health);
        ApplyHealth(player, localHud);
    }
}

void CSE_UpdateLegacyArmor(int armor) {
    CSE_ThinkExtendedStats();
    using namespace cse::extstats;
    const int player = CSE_LocalPlayerIndex();
    gHUD.m_Battery.SetArmor(ValidPlayer(player) ? ClientStats().Legacy(Stat::Armor, player, armor) : armor);
}

void CSE_ResetExtendedStats(bool allPlayers) {
    using namespace cse::extstats;
    if (allPlayers) {
        ClientStats().Reset();
        for (int player = 1; player <= MAX_PLAYERS; ++player) {
            g_PlayerExtraInfo[player].health = 0;
        }
    } else {
        ClientStats().ResetPlayer(CSE_LocalPlayerIndex());
    }
}

void CSE_ThinkExtendedStats() {
    const bool demo = gEngfuncs.pDemoAPI && gEngfuncs.pDemoAPI->IsPlayingback();
    if (cse::extstats::ClientStats().AdvanceTime(gEngfuncs.GetClientTime(), demo)) {
        for (int player = 1; player <= MAX_PLAYERS; ++player) {
            g_PlayerExtraInfo[player].health = 0;
        }
        gHUD.m_Health.SetHealth(0);
        gHUD.m_Battery.SetArmor(0);
    }
}
