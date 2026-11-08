#pragma once

#include <cstring>
#include <type_traits>

inline void InitializeFixtureEntity(edict_t &entity) {
    static_assert(std::is_standard_layout<entvars_t>::value, "Engine entity variables must retain their field layout");
    // Match ED_ClearEdict: Vector's empty constructor does not zero its components,
    // including when an edict uses C++ aggregate value initialization.
    std::memset(static_cast<void *>(&entity.v), 0, sizeof(entity.v));
    entity.free = false;
    entity.v.pContainingEntity = &entity;
}
