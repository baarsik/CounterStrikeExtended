#pragma once

#include "../common/native_entity_offsets.h"

// Included after the engine entity pool and error service declarations.
edict_t *PEntityOfEntOffset(EOFFSET offset) {
    std::int32_t index = 0;
    if (!cse::entities::IndexFromOffset(sv.edicts, static_cast<std::size_t>(sv.max_edicts), offset, index)) {
        return nullptr;
    }
    return &sv.edicts[index];
}

EOFFSET EntOffsetOfPEntity(const edict_t *entity) {
    if (!entity) {
        return cse::entities::WorldOffset;
    }
    EOFFSET offset = cse::entities::WorldOffset;
    if (!cse::entities::OffsetFromEntity(sv.edicts, static_cast<std::size_t>(sv.max_edicts), entity, offset)) {
        Sys_Error("Bad entity in EntOffsetOfPEntity()");
    }
    return offset;
}

int IndexOfEdict(const edict_t *entity) {
    if (!entity) {
        return 0;
    }
    std::int32_t index = 0;
    if (!cse::entities::IndexFromEntity(sv.edicts, static_cast<std::size_t>(sv.max_edicts), entity, index)) {
        Sys_Error("Bad entity in IndexOfEdict()");
    }
    return index;
}

int NUM_FOR_EDICT(const edict_t *entity) {
    std::int32_t index = 0;
    if (sv.num_edicts > sv.max_edicts
        || !cse::entities::IndexFromEntity(sv.edicts, static_cast<std::size_t>(sv.num_edicts), entity, index)) {
        Sys_Error("NUM_FOR_EDICT: bad pointer");
    }
    return index;
}

edict_t *EDICT_NUM(int index) {
    if (index < 0 || index >= sv.max_edicts
        || !cse::entities::ValidPool(sv.edicts, static_cast<std::size_t>(sv.max_edicts))) {
        Sys_Error("EDICT_NUM: bad number %i", index);
    }
    return &sv.edicts[index];
}
