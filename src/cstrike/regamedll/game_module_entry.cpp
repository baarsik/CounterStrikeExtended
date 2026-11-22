#include <type_traits>

#include "cse_module_abi.h"
#include "precompiled.h"

static_assert(sizeof(QString) == sizeof(uintptr_t));
static_assert(alignof(QString) == alignof(uintptr_t));
static_assert(std::is_trivially_copyable_v<QString>);
static_assert(std::is_standard_layout_v<QString>);

#include "game_module_entry_impl.h"
