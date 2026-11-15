#pragma once

#include "interface.h"
#include "cse_module_abi.h"
#include "vgui_backdrop.h"
#include <cstdint>

namespace cse {
namespace ui {

constexpr char StyleInterface[] = "CSEVGuiStyle001";
constexpr std::uint32_t StyleApiVersion = 1;

// Optional, process-local service. Rectangle coordinates are absolute screen pixels, not panel handles.
// Draw only from background painting; the engine samples its pre-HUD scene and preserves GL state.
struct StyleApi {
    std::uint32_t size;
    std::uint32_t version;
    int (CSE_ABI_CALL *CurrentStyle)();
    bool (CSE_ABI_CALL *DrawSurface)(const BackdropRect* bounds, const BackdropRect* clip,
        const BackdropStyle* style, float radius);
};

inline bool ValidStyleApi(const StyleApi* api) {
    return api && api->size >= sizeof(StyleApi) && api->version == StyleApiVersion &&
        api->CurrentStyle && api->DrawSurface;
}

inline const StyleApi* QueryStyle(CreateInterfaceFn factory) {
    const auto* api = factory ? static_cast<const StyleApi*>(factory(StyleInterface, nullptr)) : nullptr;
    return ValidStyleApi(api) ? api : nullptr;
}

} // namespace ui
} // namespace cse
