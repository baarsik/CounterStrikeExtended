#include <cstdint>
#include <type_traits>
#include "extdll.h"
#if defined(M_PI)
#undef M_PI
#endif
#include "util.h"

static_assert(sizeof(EOFFSET) == sizeof(std::intptr_t), "Native entity offsets must retain pointer-width differences");
static_assert(std::is_same<decltype(enginefuncs_t::pfnPEntityOfEntOffset), edict_t *(*)(EOFFSET)>::value,
              "Entity offset lookup must accept a native offset");
static_assert(std::is_same<decltype(enginefuncs_t::pfnEntOffsetOfPEntity), EOFFSET (*)(const edict_t *)>::value,
              "Entity offset creation must return a native offset");
static_assert(sizeof(decltype(((enginefuncs_t *)nullptr)->pfnIndexOfEdict(nullptr))) == sizeof(std::int32_t),
              "Entity indices keep their fixed-width boundary");
int main() {
    return 0;
}
