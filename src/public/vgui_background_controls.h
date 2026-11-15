#pragma once

#include "vgui_background_effects.h"
#include "vgui_style_service.h"
#include "vgui_style.h"
#include <map>
#include <string>

namespace cse {
namespace ui {

inline const StyleApi*& ControlsStyle() {
    static const StyleApi* style = nullptr;
    return style;
}

inline bool ControlsSmokedGlass() {
    const auto* style = ControlsStyle();
    return style && style->CurrentStyle() == SmokedTacticalGlassVGuiStyle;
}

inline std::map<const void*, bool>& ControlsStyles() {
    static std::map<const void*, bool> styles;
    return styles;
}

inline std::map<const void*, bool>& ControlsDecorations() {
    static std::map<const void*, bool> visibility;
    return visibility;
}

inline std::map<const void*, bool>& ControlsSeparators() {
    static std::map<const void*, bool> enabled;
    return enabled;
}

struct MenuRowVisibility {
    bool logical = true;
    bool inRange = true;
};
struct MenuLineLimit {
    int requested = 0;
    int applied = 0;
};
inline std::map<const void*, MenuRowVisibility>& ControlsMenuRows() {
    static std::map<const void*, MenuRowVisibility> rows;
    return rows;
}
inline std::map<const void*, MenuLineLimit>& ControlsMenuLimits() {
    static std::map<const void*, MenuLineLimit> limits;
    return limits;
}
inline bool MenuVisibleRequest(const void* panel, bool state) {
    auto row = ControlsMenuRows().find(panel);
    if (row != ControlsMenuRows().end()) {
        row->second.logical = state;
        return state && row->second.inRange;
    }
    return state;
}

struct ControlResourceGeometry {
    std::string x;
    std::string y;
    int width = 0;
    int height = 0;
    bool hasWidth = false;
    bool hasHeight = false;
};

inline std::map<const void*, ControlResourceGeometry>& ControlsResourceGeometry() {
    static std::map<const void*, ControlResourceGeometry> geometry;
    return geometry;
}

struct ControlFontSelection {
    std::uintptr_t original = 0;
    std::uintptr_t themed = 0;
};

inline std::map<const void*, ControlFontSelection>& ControlsFonts() {
    static std::map<const void*, ControlFontSelection> fonts;
    return fonts;
}

inline std::uint64_t& ControlsGeneration() {
    static std::uint64_t generation = 0;
    return generation;
}

inline bool ControlsStyleChanged(const void* panel) {
    const auto found = ControlsStyles().find(panel);
    return found != ControlsStyles().end() && found->second != ControlsSmokedGlass();
}

inline void RememberControlStyle(const void* panel) {
    ControlsStyles()[panel] = ControlsSmokedGlass();
}

inline void ForgetControlStyle(const void* panel) {
    ControlsStyles().erase(panel);
    ControlsResourceGeometry().erase(panel);
    ControlsFonts().erase(panel);
    ControlsDecorations().erase(panel);
    ControlsSeparators().erase(panel);
    ControlsMenuRows().erase(panel);
    ControlsMenuLimits().erase(panel);
}

inline const BackgroundEffectsApi*& ControlsBackgroundEffects() {
    static const BackgroundEffectsApi* effects = nullptr;
    return effects;
}

inline void BindBackgroundEffects(CreateInterfaceFn* factories, int count) {
    auto& effects = ControlsBackgroundEffects();
    effects = nullptr;
    ControlsStyle() = nullptr;
    ControlsStyles().clear();
    ControlsResourceGeometry().clear();
    ControlsFonts().clear();
    ControlsDecorations().clear();
    ControlsSeparators().clear();
    ControlsMenuRows().clear();
    ControlsMenuLimits().clear();
    ++ControlsGeneration();
    for (int index = 0; index < count; ++index) {
        if (!effects) {
            effects = QueryBackgroundEffects(factories[index]);
        }
        if (!ControlsStyle()) {
            ControlsStyle() = QueryStyle(factories[index]);
        }
    }
}

inline void ClearBackgroundEffects() {
    ControlsBackgroundEffects() = nullptr;
    ControlsStyle() = nullptr;
    ControlsStyles().clear();
    ControlsResourceGeometry().clear();
    ControlsFonts().clear();
    ControlsDecorations().clear();
    ControlsSeparators().clear();
    ControlsMenuRows().clear();
    ControlsMenuLimits().clear();
    ++ControlsGeneration();
}

} // namespace ui
} // namespace cse
