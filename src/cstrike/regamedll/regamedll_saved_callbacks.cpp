#include "saved_callbacks.h"
#include <cstring>

#include "precompiled.h"
#include "regamedll_saved_callbacks.h"

#define CSE_DECLARE_REGAMEDLL_CALLBACKS(source) bool CseRegisterCallbacks_##source();
CSE_FOR_EACH_REGAMEDLL_CALLBACK_SOURCE(CSE_DECLARE_REGAMEDLL_CALLBACKS)
#undef CSE_DECLARE_REGAMEDLL_CALLBACKS

static bool CseRegisterGameCallbacks() {
#define CSE_CALL_REGAMEDLL_CALLBACKS(source) && CseRegisterCallbacks_##source()
    return true CSE_FOR_EACH_REGAMEDLL_CALLBACK_SOURCE(CSE_CALL_REGAMEDLL_CALLBACKS);
#undef CSE_CALL_REGAMEDLL_CALLBACKS
}

#define CSE_SAVE_GAME_CS
#include "game_saved_callbacks_impl.h"
