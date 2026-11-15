#pragma once

#include "vgui_backdrop.h"
#include "vgui_style.h"
#include <cstring>

namespace cse {
namespace ui {

struct Rgba {
    int r;
    int g;
    int b;
    int a;
};

inline constexpr Rgba Text{235, 232, 226, 255};
inline constexpr Rgba SecondaryText{174, 170, 162, 255};
inline constexpr Rgba DisabledText{108, 115, 118, 255};
inline constexpr Rgba Accent{228, 177, 55, 255};
inline constexpr Rgba Content{38, 35, 30, 160};
inline constexpr Rgba SurfaceFallback{43, 39, 33, 255};
inline constexpr Rgba Field{38, 35, 30, 154};
inline constexpr Rgba Control{235, 241, 243, 12};
inline constexpr Rgba Hover{228, 177, 55, 26};
inline constexpr Rgba Selection{228, 177, 55, 48};
inline constexpr Rgba TextSelection{91, 76, 45, 255};
inline constexpr Rgba TextSelectionInactive{65, 59, 47, 255};
inline constexpr Rgba Edge{232, 224, 208, 48};
inline constexpr Rgba Glaze{68, 62, 53, 112};
inline constexpr Rgba Transparent{0, 0, 0, 0};
inline constexpr int WindowInset = 16;
inline constexpr int WindowHeaderHeight = 48;
inline constexpr int TitleButtonSize = 20;
inline constexpr int TitleButtonGap = 8;
inline constexpr int ScrollbarWidth = 18;
inline constexpr int ControlHeight = 32;
inline constexpr int RadioDotDiameter = 6;
inline constexpr int RadioDiameter = 18;
inline constexpr int RadioTextGap = 10;
inline constexpr float SurfaceRadius = 10.0f;
inline constexpr float ControlRadius = 6.0f;
inline constexpr BackdropStyle SmokedBackdrop{1.0f, 1.0f, 0.58f};
inline constexpr int MaximumColor = 255;

inline int LegacyAlpha(int alpha) {
    return MaximumColor - (std::clamp)(alpha, 0, MaximumColor);
}

// Pixel-center geometry is shared by the GPU material and the no-blur surface fallback.
inline float RoundedInset(float width, float height, float radius, float y) {
    if (!(width > 0.0f) || !(height > 0.0f) || !std::isfinite(radius) || radius <= 0.0f) {
        return 0.0f;
    }
    constexpr float Half = 0.5f;
    radius = (std::min)(radius, (std::min)(width, height) * Half);
    const float distance = (std::min)(y, height - y);
    if (distance >= radius) {
        return 0.0f;
    }
    const float offset = radius - (std::max)(0.0f, distance);
    return radius - std::sqrt((std::max)(0.0f, radius * radius - offset * offset));
}

inline int RoundedCornerRows(int width, int height, float radius) {
    if (!std::isfinite(radius) || radius <= 0.0f || width <= 0 || height <= 0) {
        return 0;
    }
    constexpr float Half = 0.5f;
    return (std::min)(height / 2, static_cast<int>(std::ceil(
        (std::min)(radius, (std::min)(width, height) * Half))));
}

// Only the rounded corners need scanlines. Straight edges remain a handful of surface draws.
template<typename Paint>
void VisitRoundedFill(int width, int height, float radius, Paint paint) {
    if (width <= 0 || height <= 0) {
        return;
    }
    const int corners = RoundedCornerRows(width, height, radius);
    if (height > corners + corners) {
        paint(0, corners, width, height - corners);
    }
    for (int row = 0; row < corners; ++row) {
        constexpr float PixelCenter = 0.5f;
        const int inset = static_cast<int>(std::ceil(RoundedInset(static_cast<float>(width),
            static_cast<float>(height), radius, row + PixelCenter)));
        if (inset < width - inset) {
            paint(inset, row, width - inset, row + 1);
            paint(inset, height - row - 1, width - inset, height - row);
        }
    }
}

template<typename Paint>
void VisitRoundedOutline(int width, int height, float radius, Paint paint) {
    if (width <= 0 || height <= 0) {
        return;
    }
    constexpr int ThinShape = 2;
    if (width <= ThinShape || height <= ThinShape) {
        VisitRoundedFill(width, height, radius, paint);
        return;
    }
    const int corners = (std::max)(1, RoundedCornerRows(width, height, radius));
    constexpr float PixelCenter = 0.5f;
    const int topInset = static_cast<int>(std::ceil(RoundedInset(static_cast<float>(width),
        static_cast<float>(height), radius, PixelCenter)));
    if (topInset < width - topInset) {
        paint(topInset, 0, width - topInset, 1);
        paint(topInset, height - 1, width - topInset, height);
    }
    for (int row = 1; row < corners; ++row) {
        const int inset = static_cast<int>(std::ceil(RoundedInset(static_cast<float>(width),
            static_cast<float>(height), radius, row + PixelCenter)));
        paint(inset, row, inset + 1, row + 1);
        paint(width - inset - 1, row, width - inset, row + 1);
        paint(inset, height - row - 1, inset + 1, height - row);
        paint(width - inset - 1, height - row - 1, width - inset, height - row);
    }
    if (height > corners + corners) {
        paint(0, corners, 1, height - corners);
        paint(width - 1, corners, width, height - corners);
    }
}

enum class MaterialRole { None, Window, Menu, Container, Content, Control, MenuRow, Label };

struct SpectatorRailLayout {
    BackdropRect surface;
    BackdropRect settings;
    BackdropRect previous;
    BackdropRect player;
    BackdropRect health;
    BackdropRect next;
    BackdropRect camera;
};

inline SpectatorRailLayout SpectatorRail(int width, int height, int fontTall) {
    width = (std::max)(0, width);
    height = (std::max)(0, height);
    fontTall = (std::max)(1, fontTall);
    constexpr int MinimumControlHeight = 32;
    constexpr int TextVerticalPadding = 12;
    constexpr int MaximumMargin = 16;
    constexpr int MaximumGap = 8;
    constexpr int SettingsTextCells = 14;
    constexpr int CameraTextCells = 22;
    constexpr int PlayerGroupCells = 32;
    constexpr int HealthTextCells = 10;
    const int margin = (std::min)({MaximumMargin, width / 32, height / 8});
    const int gap = (std::min)(MaximumGap, width / 80);
    const int tall = (std::min)(height - margin - margin,
        (std::max)(MinimumControlHeight, fontTall + TextVerticalPadding));
    const int centerGap = (std::min)(gap * 2, width / 12);
    const int y = (std::max)(0, height - margin - centerGap - tall);
    const int settingsWide = (std::min)(width / 5, fontTall * SettingsTextCells);
    const int cameraWide = (std::min)(width / 4, fontTall * CameraTextCells);
    const int left = margin + settingsWide + gap;
    const int right = (std::max)(left, width - margin - cameraWide - gap);
    const int centerWide = (std::min)(right - left, fontTall * PlayerGroupCells);
    const int center = (std::clamp)((width - centerWide) / 2, left, right - centerWide);
    const int arrowWide = (std::min)(tall, centerWide / 6);
    const int healthWide = (std::min)(fontTall * HealthTextCells, centerWide / 3);
    const int nameWide = (std::max)(0, centerWide - arrowWide - arrowWide - healthWide - centerGap * 3);
    SpectatorRailLayout result{};
    result.surface = {margin, (std::max)(0, y - centerGap), (std::max)(0, width - margin - margin),
        (std::min)(height - margin - (std::max)(0, y - centerGap), tall + centerGap * 2)};
    result.settings = {margin + gap, y, (std::max)(0, settingsWide - gap), tall};
    result.previous = {center, y, arrowWide, tall};
    result.player = {center + arrowWide + centerGap, y, nameWide, tall};
    result.health = {result.player.x + nameWide + centerGap, y, healthWide, tall};
    result.next = {result.health.x + healthWide + centerGap, y, arrowWide, tall};
    result.camera = {width - margin - cameraWide, y, (std::max)(0, cameraWide - gap), tall};
    return result;
}

inline MaterialRole SmokedRole(const char* className) {
    if (!className) {
        return MaterialRole::None;
    }
    if (!std::strcmp(className, "Frame")) {
        return MaterialRole::Window;
    }
    if (!std::strcmp(className, "Menu")) {
        return MaterialRole::Menu;
    }
    if (!std::strcmp(className, "MenuItem")) {
        return MaterialRole::MenuRow;
    }
    if (!std::strcmp(className, "Panel") || !std::strcmp(className, "PropertySheet") ||
        !std::strcmp(className, "PropertyPage")) {
        return MaterialRole::Container;
    }
    if (!std::strcmp(className, "Button") || !std::strcmp(className, "ComboBox")) {
        return MaterialRole::Control;
    }
    constexpr const char* ContentClasses[]{"TextEntry", "RichText", "ListPanel", "SectionedListPanel",
        "ScrollBarSlider", "Slider"};
    for (const char* name : ContentClasses) {
        if (!std::strcmp(className, name)) {
            return MaterialRole::Content;
        }
    }
    return !std::strcmp(className, "Label") ? MaterialRole::Label : MaterialRole::None;
}

inline bool SmokedSchemeColor(const char* name, Rgba original, Rgba& result) {
    if (!name) {
        return false;
    }
    struct Entry {
        const char* name;
        Rgba color;
    };
    static constexpr Entry Colors[]{
        {"InGameDesktop/MenuColor", Text}, {"InGameDesktop/ArmedMenuColor", Text},
        {"InGameDesktop/DepressedMenuColor", Accent}, {"InGameDesktop/MenuHintColor", SecondaryText},
        {"InGameDesktop/BlurMenuColor", Transparent}, {"SelectionBG", Selection},
        {"FgColor", Text}, {"BaseText", Text}, {"WindowFgColor", Text}, {"Label.TextColor", Text},
        {"LabelText", Text}, {"LabelFgColor", Text}, {"BrightText", Text},
        {"BrightControlText", Text}, {"BrightBaseText", Text}, {"ControlText", Text},
        {"ButtonFgColor", Text}, {"ButtonArmedFgColor", Text}, {"ButtonDepressedFgColor", Text},
        {"MapDescriptionText", Text}, {"Menu/FgColor", Text}, {"Menu/ArmedFgColor", Text}, {"MenuItem/FgColor", Text},
        {"MenuItem/ArmedFgColor", Text}, {"SelectionFgColor", Text}, {"ListSelectionFgColor", Text},
        {"LabelDimText", SecondaryText}, {"DimBaseText", SecondaryText}, {"DisabledText", DisabledText},
        {"FgColorDim", SecondaryText}, {"DisabledFgColor1", Transparent}, {"DisabledFgColor2", DisabledText},
        {"WindowDisabledFgColor", DisabledText}, {"Label.DisabledFgColor1", Transparent},
        {"Label.DisabledFgColor2", DisabledText}, {"Menu/DisabledFgColor", DisabledText},
        {"ButtonBgColor", Control}, {"ButtonArmedBgColor", Hover}, {"ButtonDepressedBgColor", Selection},
        {"MenuButton/ButtonBgColor", Transparent}, {"MenuButton/ButtonArrowColor", SecondaryText},
        {"MenuButton/ArmedArrowColor", Text},
        {"Menu/BgColor", Transparent}, {"Menu/ArmedBgColor", Selection}, {"MenuItem/BgColor", Transparent},
        {"MenuItem/ArmedBgColor", Selection}, {"WindowBgColor", Field}, {"ListBgColor", Content},
        {"LabelBgColor", Transparent}, {"BuddyListBgColor", Content},
        {"BuddyButton/FgColor1", Text}, {"BuddyButton/FgColor2", SecondaryText},
        {"BuddyButton/ArmedFgColor1", Text}, {"BuddyButton/ArmedFgColor2", Text},
        {"BuddyButton/ArmedBgColor", Selection}, {"SectionTextColor", SecondaryText},
        {"SectionDividerColor", Edge}, {"Slider/SliderFgColor", Accent}, {"SliderTickColor", SecondaryText},
        {"SliderTrackColor", Edge}, {"ScrollBarSlider/ScrollBarSliderFgColor", SecondaryText},
        {"ScrollBarSlider/ScrollBarSliderBgColor", Content}, {"CheckBgColor", Field},
        {"CheckButtonBorder1", Edge}, {"CheckButtonBorder2", Edge}, {"CheckButtonCheck", Accent},
        {"WindowDisabledBgColor", Content}, {"ControlBG", Content}, {"SelectionBgColor", Selection},
        {"SelectionBG2", Hover}, {"TitleBarBgColor", Transparent}, {"TitleBarDisabledBgColor", Transparent},
        {"TitleBarFgColor", Text}, {"TitleBarDisabledFgColor", SecondaryText},
        {"TitleButtonBgColor", Transparent}, {"TitleButtonDisabledBgColor", Transparent},
        {"TitleButtonFgColor", Text}, {"TitleButtonDisabledFgColor", SecondaryText},
        {"BorderBright", Edge}, {"BorderDark", Edge}, {"BorderSelection", Accent},
        {"ButtonFocusBorder", Accent}, {"TextCursorColor", Text},
        {"PropertySheet.SelectedTextColor", Accent}, {"PropertySheet.TextColor", SecondaryText}
    };
    if (!std::strcmp(name, "BgColor")) {
        result = original.a == 0 ? Transparent : Content;
        return true;
    }
    for (const Entry& entry : Colors) {
        if (!std::strcmp(name, entry.name)) {
            result = entry.color;
            return true;
        }
    }
    return false; // Team/status colors and artwork are owned by their original producers.
}

} // namespace ui
} // namespace cse
