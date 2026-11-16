#pragma once

#include "SmokedGlass.h"
#include "Label.h"

// CS game menus own their content and shell together. Legacy fullscreen resource transforms
// remain in the Original path; glass uses one bounded coordinate system at every resolution.
namespace cse::gameui {

inline constexpr int ActionColumnWidth = 248;
inline constexpr int ControlGap = 12;
inline constexpr float MaximumActionWidthFraction = 0.4f;

inline void Center(vgui2::Panel* panel, int referenceWidth, int referenceHeight) {
    int screenWidth = 0;
    int screenHeight = 0;
    vgui2::surface()->GetScreenSize(screenWidth, screenHeight);
    const int margin = vgui2::smoked::Pixels(cse::ui::WindowInset);
    const int width = (std::min)(vgui2::smoked::Pixels(referenceWidth), screenWidth - margin * 2);
    const int height = (std::min)(vgui2::smoked::Pixels(referenceHeight), screenHeight - margin * 2);
    panel->SetBounds((screenWidth - width) / 2, (screenHeight - height) / 2, width, height);
}

inline void Title(vgui2::Panel* panel, int width) {
    if (auto* label = dynamic_cast<vgui2::Label*>(panel)) {
        const int inset = vgui2::smoked::Pixels(cse::ui::WindowInset);
        label->SetBounds(inset, inset, width - inset * 2,
            vgui2::smoked::Pixels(cse::ui::WindowHeaderHeight - cse::ui::WindowInset));
        label->SetFont(vgui2::smoked::TextFont(label, vgui2::scheme()->GetIScheme(label->GetScheme()),
            label->GetFont(), true));
    }
}

inline void Bounds(vgui2::Panel* parent, const char* name, int x, int y, int width, int height) {
    if (auto* child = parent->FindChildByName(name, false)) {
        child->SetBounds(x, y, width, height);
    }
}

inline void MouseInput(vgui2::Panel* panel, bool enabled) {
    panel->SetMouseInputEnabled(enabled);
    for (int index = 0; index < panel->GetChildCount(); ++index) {
        MouseInput(panel->GetChild(index), enabled);
    }
}

inline bool OwnsShell(vgui2::Panel* panel) {
    return vgui2::smoked::DocumentLayout(panel) || !std::strcmp(panel->GetName(), "TeamMenu") ||
        !std::strcmp(panel->GetName(), "BuyMenu") || !std::strcmp(panel->GetName(), "ClassMenu");
}

}
