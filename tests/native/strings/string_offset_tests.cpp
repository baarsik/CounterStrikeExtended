#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <type_traits>
#include "high_offset_buffer.h"
#include "extdll.h"
#if defined(M_PI)
#undef M_PI
#endif
#include "util.h"

static_assert(sizeof(string_t) == sizeof(std::uintptr_t), "Engine string identity must retain native offsets");
static_assert(std::is_same<decltype(enginefuncs_t::pfnAllocString), string_t (*)(const char *)>::value,
              "Engine allocation callback must return a native string identity");
static_assert(std::is_same<decltype(enginefuncs_t::pfnSzFromIndex), const char *(*)(string_t)>::value,
              "Engine lookup callback must accept a native string identity");
static_assert(std::is_same<decltype(enginefuncs_t::pfnCreateNamedEntity), edict_t *(*)(string_t)>::value,
              "Entity creation must accept a native class name");
static_assert(std::is_same<decltype(enginefuncs_t::pfnCreateInstancedBaseline),
                          int (*)(string_t, entity_state_s *)>::value,
              "Instanced baselines must accept native class name identities");
static_assert(sizeof(entvars_t::viewmodel) == sizeof(string_t), "Viewmodel names are native strings");
static_assert(sizeof(entvars_t::weaponmodel) == sizeof(string_t), "Weaponmodel names are native strings");

globalvars_t globals{};
globalvars_t *gpGlobals = &globals;
enginefuncs_t g_engfuncs{};
TYPEDESCRIPTION gEntvarsDescription[] = {
    DEFINE_ENTITY_FIELD(classname, FIELD_STRING),
    DEFINE_ENTITY_FIELD(globalname, FIELD_STRING),
    DEFINE_ENTITY_FIELD(model, FIELD_MODELNAME),
    DEFINE_ENTITY_FIELD(viewmodel, FIELD_MODELNAME),
    DEFINE_ENTITY_FIELD(weaponmodel, FIELD_MODELNAME),
    DEFINE_ENTITY_FIELD(noise, FIELD_SOUNDNAME)
};
constexpr int ENTVARS_COUNT = static_cast<int>(sizeof(gEntvarsDescription) / sizeof(gEntvarsDescription[0]));

void UTIL_StringToVector(float *, const char *) {
    // The fixture exercises string parsing; vector conversion remains an owned external boundary.
    std::abort();
}

#include "../../../src/common/game_entvars_keyvalue_impl.h"

string_t allocatedIdentity = 0;
string_t AllocateIdentity(const char *) {
    return allocatedIdentity;
}

bool CheckEntityStringParsing(string_t highIdentity) {
    edict_t entity{};
    g_engfuncs.pfnAllocString = AllocateIdentity;
    for (const auto &field : gEntvarsDescription) {
        auto *storage = reinterpret_cast<string_t *>(reinterpret_cast<char *>(&entity.v) + field.fieldOffset);
        *storage = ~static_cast<string_t>(0);
        allocatedIdentity = highIdentity;
        KeyValueData key{nullptr, const_cast<char *>(field.fieldName), const_cast<char *>("owned-name"), FALSE};
        EntvarsKeyvalue(&entity.v, &key);
        if (!key.fHandled || *storage != highIdentity) {
            std::fprintf(stderr, "Native entvars key-value string truncated in %s\n", field.fieldName);
            return false;
        }
        allocatedIdentity = 0;
        key.fHandled = FALSE;
        EntvarsKeyvalue(&entity.v, &key);
        if (!key.fHandled || *storage != 0) {
            std::fprintf(stderr, "Native entvars string overwrite retained stale bytes in %s\n", field.fieldName);
            return false;
        }
    }
    return true;
}

int main() {
    static char base[] = "base";
    HighOffsetBuffer storage(base);
    if (!storage.get()) {
        return 7;
    }
    std::memcpy(storage.get(), "mutable-name", sizeof("mutable-name"));
    globals.pStringBase = base;
    const string_t heapName = MAKE_STRING(storage.get());
    const string_t staticName = MAKE_STRING(base);
    if (staticName != 0 || !FStringNull(staticName) || STRING(heapName) != storage.get()
        || STRING(staticName) != base) {
        return 1;
    }
    if (sizeof(void *) > sizeof(std::uint32_t)) {
        if (heapName <= UINT32_MAX) {
            std::fprintf(stderr, "Fixture needs a relative string address above 4 GiB: base=%p text=%p offset=%llu\n",
                         static_cast<void *>(base), static_cast<void *>(storage.get()),
                         static_cast<unsigned long long>(heapName));
            return 2;
        }
        constexpr std::uint64_t highOnly = UINT64_C(1) << 32;
        if (FStringNull(static_cast<string_t>(highOnly))) {
            return 3;
        }
    }
    const string_t alias = MAKE_STRING(storage.get());
    if (alias != heapName) {
        return 4;
    }
    if (!CheckEntityStringParsing(heapName)) {
        return 8;
    }
    storage[0] = 'M';
    if (std::strcmp(STRING(alias), "Mutable-name") != 0) {
        return 5;
    }
    // Reversing base and target covers unsigned relative offsets in both directions.
    globals.pStringBase = storage.get();
    const string_t reversed = MAKE_STRING(base);
    if (STRING(reversed) != base || MAKE_STRING(STRING(reversed)) != reversed) {
        return 6;
    }
    std::printf("Native string identity, high offsets, aliases and mutable storage passed\n");
    return 0;
}
