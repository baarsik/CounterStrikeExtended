#pragma once
#include "cse_client_abi.h"

namespace cse { namespace abi {

#ifdef vec3_t
// Clients use the retained Vector class as the storage view of engine float[3].
// This describes that data representation only; no Vector object crosses by value.
static_assert(std::is_standard_layout<vec3_t>::value, "Client vector must have plain storage");
static_assert(std::is_trivially_destructible<vec3_t>::value, "Client vector must not own resources");
static_assert(sizeof(vec3_t) == 3 * sizeof(float), "Client vector must contain exactly three floats");
static_assert(offsetof(vec3_t, x) == 0 && offsetof(vec3_t, y) == sizeof(float)
    && offsetof(vec3_t, z) == 2 * sizeof(float), "Client vector component offsets changed");
template<>
struct ClientMemberType<vec3_t, false> {
    static constexpr std::uint64_t Hash() {
        return LayoutHash({3, ClientMemberType<float>::Hash()});
    }
};
#endif

} }

#include "generated_client_module_layouts.h"
