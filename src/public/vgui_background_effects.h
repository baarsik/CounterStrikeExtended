#pragma once

#include "interface.h"
#include "cse_module_abi.h"
#include "vgui_backdrop.h"
#include <cstdint>

namespace cse {
namespace ui {

constexpr char BackgroundEffectsInterface[] = "CSEVGuiBackgroundEffects001";
constexpr std::uint32_t BackgroundEffectsVersion = 1;

// Optional engine service. panel is a VPANEL handle in the current process.
// Call only during the panel's PaintBackground; false requests a darkness-only fallback.
struct BackgroundEffectsApi {
    std::uint32_t size;
    std::uint32_t version;
    bool (CSE_ABI_CALL *Draw)(std::uintptr_t panel, const BackdropStyle* style);
};

inline const BackgroundEffectsApi* QueryBackgroundEffects(CreateInterfaceFn factory) {
    const auto* api = factory ? static_cast<const BackgroundEffectsApi*>(
        factory(BackgroundEffectsInterface, nullptr)) : nullptr;
    return api && api->size >= sizeof(BackgroundEffectsApi) && api->version == BackgroundEffectsVersion && api->Draw
        ? api : nullptr;
}

} // namespace ui
} // namespace cse
