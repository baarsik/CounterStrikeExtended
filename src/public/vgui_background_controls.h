#pragma once

#include "vgui_background_effects.h"

namespace cse {
namespace ui {

inline const BackgroundEffectsApi*& ControlsBackgroundEffects() {
    static const BackgroundEffectsApi* effects = nullptr;
    return effects;
}

inline void BindBackgroundEffects(CreateInterfaceFn* factories, int count) {
    auto& effects = ControlsBackgroundEffects();
    effects = nullptr;
    for (int index = 0; index < count && !effects; ++index) {
        effects = QueryBackgroundEffects(factories[index]);
    }
}

inline void ClearBackgroundEffects() {
    ControlsBackgroundEffects() = nullptr;
}

} // namespace ui
} // namespace cse
