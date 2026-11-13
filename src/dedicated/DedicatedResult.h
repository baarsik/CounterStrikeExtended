#pragma once

#include "../common/dll_state.h"

namespace cse::dedicated {
inline int ProcessResult(int providerResult) noexcept {
    // RunServer reports startup failures as negative values. Its other results
    // are DLL states, including DLL_CLOSE and a still-active state on window quit.
    // Zero also covers cancelling the configuration dialog before startup.
    return providerResult < 0 ? providerResult : 0;
}
} // namespace cse::dedicated
