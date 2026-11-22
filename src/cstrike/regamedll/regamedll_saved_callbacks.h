#pragma once

#include <type_traits>

// Include after ReGameDLL's precompiled header, which supplies the native
// classes and callback types used by the shared serializer.
using BASEPTR = CBaseEntity::thinkfn_t;
using ENTITYFUNCPTR = CBaseEntity::touchfn_t;
using USEPTR = CBaseEntity::usefn_t;

static_assert(std::is_same_v<ENTITYFUNCPTR, CBaseEntity::blockedfn_t>);

#include "game_saved_callbacks.h"
#include "generated_saved_callbacks.inc"

#define CSE_REGAMEDLL_CALLBACK(kind, name, type, method) && CseRegister##kind(name, static_cast<type>(method))

// Keep each registration in the translation unit that owns the methods.
#define CSE_DEFINE_REGAMEDLL_CALLBACKS(source) \
    bool CseRegisterCallbacks_##source() { \
        return true CSE_REGAMEDLL_CALLBACKS_##source; \
    }
