#include <climits>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include "extdll.h"
#ifdef M_PI
#undef M_PI
#endif
#include "util.h"
#include "cbase.h"
#include "../../../GoldSrc-SDK/common/game_save_field_layout.h"

int main() {
    struct Snapshot {
        edict_t *entity;
    };
    TYPEDESCRIPTION field = DEFINE_FIELD(Snapshot, entity, FIELD_EDICT);
    if (!cse::save::ValidFieldStorage(field)) {
        return 1;
    }
    // A manually supplied descriptor must not bypass the metadata macro's compile-time checks.
    field.fieldType = FIELD_POINTER;
    if (cse::save::ValidFieldStorage(field)) {
        std::fprintf(stderr, "Raw saved pointers still accepted by native storage validation\n");
        return 1;
    }
    std::puts("Raw pointer metadata rejected; symbolic entity references remain valid");
    return 0;
}
