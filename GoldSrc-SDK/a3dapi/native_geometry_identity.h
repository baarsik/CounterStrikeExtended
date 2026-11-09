#pragma once

#include <cstdint>

#if UINTPTR_MAX > UINT32_MAX
// Private native identity only. Public DWORD tags and geometry/COM records keep
// their existing representation. Polygon keys are non-owning address values;
// recorded lists retain the original key without dereferencing its polygon.
struct A3dGeometryIdentity {
    enum class Domain : std::uint32_t {
        PublicTag,
        OwnedPolygon
    };

    std::uintptr_t value;
    Domain domain;

    static constexpr A3dGeometryIdentity PublicTag(std::uint32_t tag) {
        return {tag, Domain::PublicTag};
    }

    static A3dGeometryIdentity OwnedPolygon(const void* polygon) {
        return {reinterpret_cast<std::uintptr_t>(polygon), Domain::OwnedPolygon};
    }

    bool operator==(const A3dGeometryIdentity& other) const {
        return domain == other.domain && value == other.value;
    }
};
#endif
