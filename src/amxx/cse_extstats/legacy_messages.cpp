#include "module.h"
#include "legacy_numbers.h"

namespace cse::extstats {
static int CurrentType = 0;
static int Recipient = 0;
static int Argument = 0;
static int Subject = 0;
static bool LegacyNeeded = false;

void ResetLegacyMessage() {
    CurrentType = 0;
    Recipient = 0;
    Argument = 0;
    Subject = 0;
    LegacyNeeded = false;
    MessageOpen = false;
}
} // namespace cse::extstats

void StatsMessageBegin(int, int type, const float*, edict_t* entity) {
    using namespace cse::extstats;
    MessageOpen = true;
    CurrentType = type;
    Recipient = entity ? ENTINDEX(entity) : 0;
    Argument = 0;
    Subject = 0;
    LegacyNeeded = false;
    RETURN_META(MRES_IGNORED);
}

void StatsMessageEnd() {
    using namespace cse::extstats;
    const int type = CurrentType;
    const int recipient = Recipient;
    ResetLegacyMessage();
    if (!Publishing && recipient > 0 && (type == ResetHudId || type == InitHudId)) {
        State.PublishStats(recipient, true);
    }
    RETURN_META(MRES_IGNORED);
}

void StatsWriteByte(int value) {
    using namespace cse::extstats;
    const int argument = Argument++;
    if (Publishing) {
        RETURN_META(MRES_IGNORED);
    }
    int normalized = value;
    int legacySubject = 0;
    if (CurrentType == HealthId && argument == 0 && Recipient > 0) {
        legacySubject = Recipient;
        const Player player = ReadEntityPlayer(Recipient);
        normalized = LegacyAmount(Stat::Health, NormalizeHealth(player.health, player.alive));
    } else if (CurrentType == SpecHealthId && argument == 0) {
        const Player observer = ReadEntityPlayer(Recipient);
        legacySubject = observer.target;
        const Player target = ReadEntityPlayer(observer.target);
        normalized = LegacyAmount(Stat::Health, NormalizeHealth(target.health, target.alive));
    } else if (CurrentType == SpecHealth2Id && argument == 0) {
        normalized = LegacyAmount(Stat::Health, value);
        LegacyNeeded = normalized != value;
    } else if (CurrentType == SpecHealth2Id && argument == 1 && LegacyNeeded) {
        State.QueueLegacy(value);
    } else if (CurrentType == HltvId) {
        if (argument == 0) {
            Subject = value;
        } else if (argument == 1) {
            constexpr int HealthFlag = 0x80;
            if ((value & HealthFlag) != 0) {
                const Player player = ReadEntityPlayer(Subject);
                normalized =
                    HealthFlag |
                    (Subject > 0 ? LegacyAmount(Stat::Health, NormalizeHealth(player.health, player.alive), true)
                                 : (value & BroadcastHealthMaximum));
                if (Subject == 0) {
                    for (int index = 1; index <= gpGlobals->maxClients; ++index) {
                        State.PublishStats(index, true);
                    }
                }
            }
        }
    }
    // AMXX may already be buffering this message. Do not partially write to the engine in that case.
    // The publication queue emits a complete safe fallback through the hook chain after the callback.
    if (normalized != value && legacySubject > 0) {
        State.QueueLegacy(legacySubject);
    }
    if (normalized != value && gpMetaGlobals->status < MRES_SUPERCEDE) {
        g_engfuncs.pfnWriteByte(normalized);
        RETURN_META(MRES_SUPERCEDE);
    }
    RETURN_META(MRES_IGNORED);
}

void StatsWriteShort(int value) {
    using namespace cse::extstats;
    const int argument = Argument++;
    if (!Publishing && CurrentType == ArmorId && argument == 0 && Recipient > 0) {
        const int normalized = LegacyAmount(Stat::Armor, NormalizeArmor(ReadEntityPlayer(Recipient).armor));
        if (normalized != value) {
            State.QueueLegacy(Recipient);
        }
        if (normalized != value && gpMetaGlobals->status < MRES_SUPERCEDE) {
            g_engfuncs.pfnWriteShort(normalized);
            RETURN_META(MRES_SUPERCEDE);
        }
    }
    RETURN_META(MRES_IGNORED);
}
