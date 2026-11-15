#pragma once

#include "Panel.h"
#include "vgui_controls/Controls.h"
#include "../../public/vgui_background_controls.h"
#include "vgui/ISurface.h"

namespace vgui2 {

// A background-only effect: Panel paints its own foreground and children afterward.
// Other VGUI panels can also call ControlsBackgroundEffects()->Draw from PaintBackground.
class BackdropPanel : public Panel {
public:
    BackdropPanel(Panel* parent, const char* name, const cse::ui::BackdropStyle& style)
        : Panel(parent, name), style_(style) {}

protected:
    void PaintBackground() override {
        const auto* effects = cse::ui::ControlsBackgroundEffects();
        if (effects && effects->Draw(GetVPanel(), &style_)) {
            return;
        }
        constexpr float MaximumAlpha = 255.0f;
        const int height = GetTall();
        for (int y = 0; y < height; ++y) {
            const float position = (static_cast<float>(y) + 0.5f) / height;
            const float alpha = cse::ui::BackdropOpacity(style_, position) * cse::ui::UnitValue(style_.darkness);
            surface()->DrawSetColor(0, 0, 0, static_cast<int>(alpha * MaximumAlpha));
            surface()->DrawFilledRect(0, y, GetWide(), y + 1);
        }
    }

private:
    cse::ui::BackdropStyle style_;
};

} // namespace vgui2
