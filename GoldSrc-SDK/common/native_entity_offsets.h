#pragma once

#include <cstddef>
#include <cstdint>
#include "../public/cse_entity.h"

namespace cse { namespace entities {

template<typename Entity>
inline bool ValidPool(const Entity *base, std::size_t count) {
    constexpr auto stride = sizeof(Entity);
    if (!base || count == 0 || count > static_cast<std::size_t>(INT32_MAX) || count > UINTPTR_MAX / stride) {
        return false;
    }
    const auto bytes = count * stride;
    const auto address = reinterpret_cast<std::uintptr_t>(base);
    return bytes <= UINTPTR_MAX - address && bytes - 1 <= static_cast<std::uintptr_t>(INTPTR_MAX);
}

template<typename Entity>
inline bool IndexFromOffset(const Entity *base, std::size_t count, EOFFSET offset, std::int32_t &index) {
    if (!ValidPool(base, count) || offset < 0) {
        return false;
    }
    const auto bytes = static_cast<std::uintptr_t>(offset);
    if (bytes % sizeof(Entity) != 0 || bytes / sizeof(Entity) >= count) {
        return false;
    }
    index = static_cast<std::int32_t>(bytes / sizeof(Entity));
    return true;
}

template<typename Entity>
inline bool OffsetFromEntity(const Entity *base, std::size_t count, const Entity *entity, EOFFSET &offset) {
    if (!ValidPool(base, count) || !entity) {
        return false;
    }
    const auto start = reinterpret_cast<std::uintptr_t>(base);
    const auto address = reinterpret_cast<std::uintptr_t>(entity);
    if (address < start) {
        return false;
    }
    const auto bytes = address - start;
    if (bytes > static_cast<std::uintptr_t>(INTPTR_MAX) || bytes % sizeof(Entity) != 0
        || bytes / sizeof(Entity) >= count) {
        return false;
    }
    offset = static_cast<EOFFSET>(bytes);
    return true;
}

template<typename Entity>
inline bool IndexFromEntity(const Entity *base, std::size_t count, const Entity *entity, std::int32_t &index) {
    EOFFSET offset = WorldOffset;
    return OffsetFromEntity(base, count, entity, offset) && IndexFromOffset(base, count, offset, index);
}

} } // namespace cse::entities
