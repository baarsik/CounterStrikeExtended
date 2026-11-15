extern "C" {
    char VguiEmptyString[] = "";
}

#include "../../../../public/vgui1_style.h"
#include "SmokedGlass.h"
#include "VGUI_Font.h"
#include <map>
#include <memory>

namespace {
const cse::ui::StyleApi* legacyStyle = nullptr;
std::map<int, std::unique_ptr<vgui::Font>> legacyFonts;
}

const cse::ui::StyleApi* cse_vgui1::smoked::Style() {
    return legacyStyle;
}

vgui::Font* cse_vgui1::smoked::TextFont(vgui::Font* original, vgui::Scheme::SchemeFont role) {
    if (!original || !Enabled() || !IsTextFont(role)) {
        return original;
    }
    constexpr int MinimumTextHeight = 12;
    constexpr int MaximumTextHeight = 16;
    constexpr int TextWeight = 400;
    const int tall = (std::clamp)(original->getTall(), MinimumTextHeight, MaximumTextHeight);
    auto& font = legacyFonts[tall];
    if (!font) {
        font = std::make_unique<vgui::Font>("Arial", tall, 0, 0.0f, TextWeight, false, false, false, false);
    }
    return font.get();
}

extern "C" void CSE_ABI_CALL CSE_VGui1BindStyle(const cse::ui::StyleApi* api) {
    legacyFonts.clear();
    legacyStyle = cse::ui::ValidStyleApi(api) ? api : nullptr;
}
