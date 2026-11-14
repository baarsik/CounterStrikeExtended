#include "module.h"
#include "legacy_numbers.h"

namespace cse::extstats {
static int CurrentType = 0;
static int Recipient = 0;
static int Argument = 0;
static int Subject = 0;
static bool LegacyNeeded = false;
struct LegacyCorrection {
    int destination = 0;
    edict_t* entity = nullptr;
    std::array<float, 3> origin{};
    bool hasOrigin = false;
    std::array<int, 2> values{};
    int fields = 0;
    bool shortValue = false;
    bool needed = false;
    bool blocked = false;
};
static LegacyCorrection Correction;

void ResetLegacyMessage() {
    CurrentType = 0;
    Recipient = 0;
    Argument = 0;
    Subject = 0;
    LegacyNeeded = false;
    MessageOpen = false;
    Correction = {};
}
} // namespace cse::extstats

void StatsMessageBegin(int destination, int type, const float* origin, edict_t* entity) {
    using namespace cse::extstats;
    MessageOpen = true;
    Correction = {};
    Correction.destination = destination;
    Correction.entity = entity;
    if (origin) {
        for (std::size_t coordinate = 0; coordinate < Correction.origin.size(); ++coordinate) {
            Correction.origin[coordinate] = origin[coordinate];
        }
        Correction.hasOrigin = true;
    }
    constexpr int SingleStatFields = 1;
    constexpr int PlayerStatFields = 2;
    if ((HealthId && type == HealthId) || (SpecHealthId && type == SpecHealthId)) {
        Correction.fields = SingleStatFields;
    } else if (ArmorId && type == ArmorId) {
        Correction.fields = SingleStatFields;
        Correction.shortValue = true;
    } else if ((SpecHealth2Id && type == SpecHealth2Id) || (HltvId && type == HltvId)) {
        Correction.fields = PlayerStatFields;
    }
    if (!Publishing && Correction.fields && g_fn_MessageBlock) {
        int block = BLOCK_NOT;
        MF_MessageBlock(MSGBLOCK_GET, type, &block);
        Correction.blocked = block != BLOCK_NOT;
    }
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
    const LegacyCorrection correction = Correction;
    const bool correct = correction.needed && !correction.blocked && Argument == correction.fields;
    ResetLegacyMessage();
    if (!Publishing && recipient > 0 && (type == ResetHudId || type == InitHudId)) {
        State.PublishStats(recipient, true);
    }
    if (!Publishing && correct && HookedEngine && (recipient == 0 || ReadEntityPlayer(recipient).connected)) {
        // AMXX has finished replaying/resetting its buffer. Preserve the destination so an
        // unreliable original cannot arrive after a correction from the reliable stream.
        Publishing = true;
        HookedEngine->pfnMessageBegin(correction.destination, type,
                                      correction.hasOrigin ? correction.origin.data() : nullptr, correction.entity);
        for (int field = 0; field < correction.fields; ++field) {
            if (correction.shortValue) {
                HookedEngine->pfnWriteShort(correction.values[field]);
            } else {
                HookedEngine->pfnWriteByte(correction.values[field]);
            }
        }
        HookedEngine->pfnMessageEnd();
        Publishing = false;
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
    } else if (CurrentType == SpecHealth2Id && argument == 1 && LegacyNeeded && !Correction.blocked) {
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
    if (Correction.fields && argument < Correction.fields) {
        Correction.values[argument] = normalized;
        Correction.needed = Correction.needed || (normalized != value && gpMetaGlobals->status >= MRES_SUPERCEDE);
    }
    // AMXX may already be buffering this message. Do not partially write to the engine in that case.
    // The publication queue emits a complete safe fallback through the hook chain after the callback.
    if (normalized != value && legacySubject > 0 && !Correction.blocked) {
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
        Correction.values[0] = normalized;
        Correction.needed = normalized != value && gpMetaGlobals->status >= MRES_SUPERCEDE;
        if (normalized != value && !Correction.blocked) {
            State.QueueLegacy(Recipient);
        }
        if (normalized != value && gpMetaGlobals->status < MRES_SUPERCEDE) {
            g_engfuncs.pfnWriteShort(normalized);
            RETURN_META(MRES_SUPERCEDE);
        }
    }
    RETURN_META(MRES_IGNORED);
}
