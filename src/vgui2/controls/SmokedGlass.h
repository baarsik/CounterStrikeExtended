#pragma once

#include "Panel.h"
#include <initializer_list>
#include <cwctype>
#include <cstdlib>
#include <typeinfo>
#include "vgui/ISurface.h"
#include "vgui/IPanel.h"
#include "vgui/IScheme.h"
#include "../../public/vgui_background_controls.h"
#include "../../public/vgui_smoked_glass.h"

namespace vgui2 {
ISurface* surface();
IPanel* ipanel();
ISchemeManager* scheme();
namespace smoked {

inline Color ColorOf(cse::ui::Rgba color) {
    return Color(color.r, color.g, color.b, color.a);
}

inline float PixelScale() {
    int width = 0;
    int height = 0;
    surface()->GetScreenSize(width, height);
    constexpr float ReferenceHeight = 1080.0f;
    constexpr float MinimumScale = 0.75f;
    constexpr float MaximumScale = 1.0f;
    return (std::clamp)(height / ReferenceHeight, MinimumScale, MaximumScale);
}

inline int Pixels(float value) {
    return static_cast<int>(std::lround(value * PixelScale()));
}

inline float ResourceScale() {
    // Legacy form rows are 24 units; match the shared 32-pixel control height at the reference density.
    constexpr float LegacyRowHeight = 24.0f;
    return PixelScale() * static_cast<float>(cse::ui::ControlHeight) / LegacyRowHeight;
}

inline int ResourcePixels(int value) {
    return static_cast<int>(std::lround(static_cast<float>(value) * ResourceScale()));
}

inline int NormalizedPixels(int value) {
    return static_cast<int>(std::lround(static_cast<float>(value) / ResourceScale()));
}

inline void RefreshResourceBounds(Panel* panel) {
    const auto found = cse::ui::ControlsResourceGeometry().find(panel);
    if (found == cse::ui::ControlsResourceGeometry().end()) {
        return;
    }
    const auto& source = found->second;
    int screenWidth = 0;
    int screenHeight = 0;
    surface()->GetScreenSize(screenWidth, screenHeight);
    const auto position = [panel](const std::string& value, int extent, int current) {
        if (value.empty()) {
            return current;
        }
        const char* number = value.c_str();
        const char anchor = static_cast<char>(*number & 0xDF);
        if (anchor == 'R' || anchor == 'C') {
            ++number;
        }
        const int offset = panel->GetProportionalScaledValue(std::atoi(number));
        return anchor == 'R' ? extent - offset : (anchor == 'C' ? extent / 2 + offset : offset);
    };
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
    panel->GetBounds(x, y, width, height);
    panel->SetBounds(position(source.x, screenWidth, x), position(source.y, screenHeight, y),
        source.hasWidth ? panel->GetProportionalScaledValue(source.width) : width,
        source.hasHeight ? panel->GetProportionalScaledValue(source.height) : height);
}

inline bool CompactLayout() {
    int width = 0;
    int height = 0;
    surface()->GetScreenSize(width, height);
    constexpr int CompactScreenHeight = 600;
    return height < CompactScreenHeight;
}

inline HFont LayoutFont(Panel* panel, HFont candidate) {
    const auto selection = cse::ui::ControlsFonts().find(panel);
    return selection != cse::ui::ControlsFonts().end() && candidate == selection->second.themed
        ? static_cast<HFont>(selection->second.original) : candidate;
}

template<typename Action, typename Item>
inline bool CenterCaption(Item* panel) {
    // Message-map names are inherited by image/map buttons with deliberate custom alignment.
    return cse::ui::ControlsSmokedGlass() && typeid(*panel) == typeid(Action) &&
        panel->GetImageCount() == 1 && panel->GetImageAtIndex(0) == panel->GetTextImage();
}

inline bool DocumentLayout(Panel* panel) {
    return !std::strcmp(panel->GetName(), "VGUITextWindow") || !std::strcmp(panel->GetName(), "ClientMOTD");
}

inline bool OptionsLayout(Panel* panel) {
    return !std::strcmp(panel->GetName(), "OptionsDialog");
}

inline bool ReadableFormText(Panel* panel) {
    if (!std::strcmp(panel->GetClassName(), "MenuItem")) {
        return true;
    }
    for (Panel* parent = panel->GetParent(); parent; parent = parent->GetParent()) {
        if (!std::strcmp(parent->GetName(), "Interface") || !std::strcmp(parent->GetName(), "bottombar")) {
            return true;
        }
    }
    return false;
}

// Scheme fonts stay untouched. Only ordinary UI text is replaced; symbol/custom fonts keep their owner.
inline HFont TextFont(Panel* panel, IScheme* scheme, HFont candidate, bool title = false, int maximumTall = 0) {
    auto& selections = cse::ui::ControlsFonts();
    const auto previous = selections.find(panel);
    HFont original = candidate;
    if (previous != selections.end() && candidate == previous->second.themed) {
        original = static_cast<HFont>(previous->second.original);
    }
    if (!cse::ui::ControlsSmokedGlass()) {
        selections.erase(panel);
        return original;
    }
    bool ordinary = title;
    bool small = false;
    bool bold = false;
    constexpr const char* TextAliases[]{"Default", "DefaultSmall", "DefaultVerySmall", "DefaultLarge",
        "DefaultBold", "DefaultSmallBold", "MenuLarge", "MenuItemFont", "LabelFont"};
    for (const char* alias : TextAliases) {
        if (original && original == scheme->GetFontEx(alias, panel->IsProportional(), panel->IsHDProportional())) {
            ordinary = true;
            small = std::strstr(alias, "Small") != nullptr;
            bold = std::strstr(alias, "Bold") != nullptr;
            break;
        }
    }
    // Dense resource-based pages need their original tab metrics on small screens.
    if (!ordinary || !original || (CompactLayout() && !std::strcmp(panel->GetName(), "tab"))) {
        selections.erase(panel);
        return original;
    }
    constexpr int minimumBodyHeight = 12;
    const int minimumTitleHeight = 18;
    const int minimumSmallHeight = 11;
    constexpr int TextVerticalPadding = 2;
    constexpr int NormalWeight = 400;
    constexpr int EmphasisWeight = 600;
    constexpr int FontAntialias = 0x010; // ISurface026's antialiased glyph flag.
    const bool formText = ReadableFormText(panel);
    const int minimumHeight = title ? minimumTitleHeight : (small ? minimumSmallHeight : minimumBodyHeight);
    int tall = (std::max)(minimumHeight, Pixels(title ? 24.0f : (formText ? 20.0f : (small ? 14.0f : 16.0f))));
    if (!title) {
        if (!formText) {
            tall = (std::min)(tall, (std::max)(minimumSmallHeight, surface()->GetFontTall(original)));
        }
        if (panel->GetTall() > 0) {
            tall = (std::min)(tall, (std::max)(minimumSmallHeight, panel->GetTall() - TextVerticalPadding));
        }
    }
    if (maximumTall > 0) {
        tall = (std::min)(tall, (std::max)(minimumBodyHeight, maximumTall));
    }
    struct FontCache {
        std::uint64_t generation = 0;
        std::map<std::pair<int, int>, HFont> fonts;
    };
    static FontCache cache;
    if (cache.generation != cse::ui::ControlsGeneration()) {
        cache.fonts.clear();
        cache.generation = cse::ui::ControlsGeneration();
    }
    const int weight = bold ? EmphasisWeight : NormalWeight;
    HFont& font = cache.fonts[{tall, weight}];
    if (!font) {
        const HFont created = surface()->CreateFont();
        if (surface()->AddGlyphSetToFont(created, "Arial", tall, weight, 0, 0, FontAntialias, 0, 0)) {
            font = created;
        }
    }
    if (!font) {
        return original;
    }
    if (!title && panel->GetTall() > 0 && surface()->GetFontTall(font) > panel->GetTall()) {
        return original;
    }
    selections[panel] = {original, font};
    return font;
}

template<typename Item>
void FitCaption(Item* item) {
    if (!cse::ui::ControlsSmokedGlass() || item->GetWide() <= 0 ||
        !std::strcmp(item->GetClassName(), "MenuItem") || !std::strcmp(item->GetClassName(), "RadioButton") ||
        !std::strcmp(item->GetName(), "tab")) {
        return;
    }
    const auto selection = cse::ui::ControlsFonts().find(item);
    if (selection == cse::ui::ControlsFonts().end()) {
        return;
    }
    constexpr int CaptionCells = 1024;
    wchar_t text[CaptionCells]{};
    item->GetText(text, CaptionCells);
    int width = 0;
    int height = 0;
    surface()->GetTextSize(item->GetFont(), text, width, height);
    if (width <= 0) {
        return;
    }
    constexpr int HorizontalPadding = 8;
    const int availableWidth = (std::max)(1, item->GetWide() - Pixels(HorizontalPadding));
    if (width > availableWidth) {
        constexpr int MinimumFittedHeight = 12;
        const HFont original = static_cast<HFont>(selection->second.original);
        const int fontTall = surface()->GetFontTall(item->GetFont());
        int fittedTall = (std::max)(MinimumFittedHeight,
            static_cast<int>(std::floor(static_cast<float>(fontTall) * availableWidth / width)));
        do {
            item->SetFont(TextFont(item, vgui2::scheme()->GetIScheme(item->GetScheme()), item->GetFont(),
                false, fittedTall));
            surface()->GetTextSize(item->GetFont(), text, width, height);
        } while (width > availableWidth && fittedTall-- > MinimumFittedHeight);
        if (width > availableWidth) {
            int originalWidth = 0;
            int originalHeight = 0;
            surface()->GetTextSize(original, text, originalWidth, originalHeight);
            if (originalWidth < width) {
                item->SetFont(original);
            }
        }
    }
}

inline int MenuHeight(Panel*, int original) {
    return cse::ui::ControlsSmokedGlass() ? Pixels(36.0f) : original;
}

template<typename Item>
int MenuWidth(Item* item, int original) {
    if (!cse::ui::ControlsSmokedGlass()) {
        return original;
    }
    constexpr int MenuTextCells = 1024;
    wchar_t text[MenuTextCells]{};
    item->GetText(text, MenuTextCells);
    int width = 0;
    int height = 0;
    surface()->GetTextSize(item->GetFont(), text, width, height);
    return (std::max)(original, width) + Pixels(40);
}

template<typename Item>
bool MenuSeparator(Item* item) {
    if (item->IsCheckable() || item->HasMenu() || item->GetImageCount() > 1) {
        return false;
    }
    constexpr int TextCells = 1024;
    wchar_t text[TextCells]{};
    item->GetText(text, TextCells);
    for (const wchar_t character : text) {
        if (character && !std::iswspace(character)) {
            return false;
        }
    }
    return true;
}

template<typename Item>
void MenuAvailability(Item* item) {
    auto& availability = cse::ui::ControlsSeparators();
    if (cse::ui::ControlsSmokedGlass() && MenuSeparator(item)) {
        availability.emplace(item, item->IsEnabled());
        item->SetEnabled(false);
    } else {
        const auto original = availability.find(item);
        if (original != availability.end()) {
            item->SetEnabled(original->second);
            availability.erase(original);
        }
    }
}

template<typename Item>
int MenuRowHeight(Item* item) {
    return MenuSeparator(item) ? Pixels(12) : (std::max)(Pixels(36),
        surface()->GetFontTall(item->GetFont()) + Pixels(12));
}

template<typename MenuType>
void RestoreMenuRows(MenuType* menu) {
    auto& rows = cse::ui::ControlsMenuRows();
    for (int index = 0; index < menu->GetItemCount(); ++index) {
        auto* item = menu->GetMenuItem(menu->GetMenuID(index));
        const auto row = rows.find(item);
        if (row != rows.end()) {
            const bool visible = row->second.logical;
            rows.erase(row);
            item->SetVisible(visible);
        }
    }
}

template<typename MenuType>
void ScrollMenuRows(MenuType* menu, int first, int count) {
    RestoreMenuRows(menu);
    auto& rows = cse::ui::ControlsMenuRows();
    for (int index = 0; index < menu->GetItemCount(); ++index) {
        auto* item = menu->GetMenuItem(menu->GetMenuID(index));
        if (!item) {
            continue;
        }
        const bool visible = item->IsVisible();
        const bool inRange = index >= first && index < first + count;
        item->SetVisible(visible && inRange);
        rows[item] = {visible, inRange};
    }
}

template<typename MenuType>
int MenuVisibleLimit(MenuType* menu, int current) {
    auto& limits = cse::ui::ControlsMenuLimits();
    auto entry = limits.find(menu);
    if (!cse::ui::ControlsSmokedGlass()) {
        const int requested = entry == limits.end() ? current : entry->second.requested;
        limits.erase(menu);
        return requested;
    }
    const int requested = entry == limits.end() || current != entry->second.applied
        ? current : entry->second.requested;
    int height = Pixels(36);
    for (int row = 0; row < menu->GetItemCount(); ++row) {
        auto* item = menu->GetMenuItem(menu->GetMenuID(row));
        if (item && item->IsVisible()) {
            height = (std::max)(height, MenuRowHeight(item));
        }
    }
    int screenWide = 0;
    int screenTall = 0;
    surface()->GetScreenSize(screenWide, screenTall);
    const int available = (std::max)(1, (screenTall - Pixels(24)) / height);
    const int applied = (std::max)(1, (std::min)(requested, available));
    limits[menu] = {requested, applied};
    return applied;
}

template<typename MenuType>
int MenuTextInset(MenuType* menu) {
    for (int row = 0; row < menu->GetItemCount(); ++row) {
        auto* item = menu->GetMenuItem(menu->GetMenuID(row));
        if (item && item->IsCheckable()) {
            return Pixels(34);
        }
    }
    return Pixels(16);
}

template<typename MenuType>
void LayoutMenu(MenuType* menu, int fixedWidth, int minimumWidth, int scrollWidth) {
    const int padding = Pixels(6);
    const int textInset = MenuTextInset(menu);
    int screenWide = 0;
    int screenTall = 0;
    surface()->GetScreenSize(screenWide, screenTall);
    int contentWide = 0;
    for (int row = 0; row < menu->GetItemCount(); ++row) {
        auto* item = menu->GetMenuItem(menu->GetMenuID(row));
        if (!item) {
            continue;
        }
        MenuAvailability(item);
        constexpr int TextCells = 1024;
        wchar_t text[TextCells]{};
        item->GetText(text, TextCells);
        int width = 0;
        int height = 0;
        surface()->GetTextSize(item->GetFont(), text, width, height);
        const int trailing = item->HasMenu() ? Pixels(30) : Pixels(16);
        contentWide = (std::max)(contentWide, width + textInset + trailing);
    }
    constexpr int MinimumMenuWidth = 120;
    int width = fixedWidth > 0 ? fixedWidth : (std::max)({minimumWidth, Pixels(MinimumMenuWidth),
        contentWide + padding + padding + scrollWidth});
    width = (std::min)(width, (std::max)(1, screenWide - padding - padding));
    const int itemWide = (std::max)(0, width - padding - padding - scrollWidth);
    int top = padding;
    for (int row = 0; row < menu->GetItemCount(); ++row) {
        auto* item = menu->GetMenuItem(menu->GetMenuID(row));
        if (!item || !item->IsVisible()) {
            continue;
        }
        const int tall = MenuRowHeight(item);
        item->SetBounds(padding, top, itemWide, tall);
        item->SetTextInset(textInset, 0);
        item->InvalidateLayout(false, false);
        top += tall;
    }
    menu->SetSize(width, top + padding);
}

inline Color SchemeColor(const char* name, Color original) {
    cse::ui::Rgba color{};
    if (cse::ui::ControlsSmokedGlass() && cse::ui::SmokedSchemeColor(name,
        {original.r(), original.g(), original.b(), original.a()}, color)) {
        return ColorOf(color);
    }
    return original;
}

inline cse::ui::MaterialRole Role(PanelMessageMap* map) {
    for (; map; map = map->baseMap) {
        const auto role = cse::ui::SmokedRole(map->pfnClassName ? map->pfnClassName() : nullptr);
        if (role != cse::ui::MaterialRole::None) {
            return role;
        }
    }
    return cse::ui::MaterialRole::None;
}

inline void Fill(int x, int y, int width, int height, float radius, Color color) {
    if (width <= 0 || height <= 0 || color.a() == 0) {
        return;
    }
    surface()->DrawSetColor(color);
    cse::ui::VisitRoundedFill(width, height, radius, [x, y](int x0, int y0, int x1, int y1) {
        surface()->DrawFilledRect(x + x0, y + y0, x + x1, y + y1);
    });
}

inline void Outline(int width, int height, float radius, Color color, int x = 0, int y = 0) {
    if (width <= 0 || height <= 0) {
        return;
    }
    surface()->DrawSetColor(color);
    cse::ui::VisitRoundedOutline(width, height, radius, [x, y](int x0, int y0, int x1, int y1) {
        surface()->DrawFilledRect(x + x0, y + y0, x + x1, y + y1);
    });
}

// Small circles need coverage at diagonal edges; one-pixel rounded-box outlines leave visible gaps.
inline void Circle(int x, int y, int diameter, Color color, float strokeWidth = 0.0f) {
    constexpr float HalfPixel = 0.5f;
    const float center = diameter * HalfPixel;
    const float radius = center - HalfPixel;
    const float innerRadius = radius - strokeWidth;
    for (int row = 0; row < diameter; ++row) {
        for (int column = 0; column < diameter; ++column) {
            const float dx = column + HalfPixel - center;
            const float dy = row + HalfPixel - center;
            const float distance = std::sqrt(dx * dx + dy * dy);
            float coverage = (std::clamp)(radius + HalfPixel - distance, 0.0f, 1.0f);
            if (strokeWidth > 0.0f) {
                coverage *= (std::clamp)(distance - innerRadius + HalfPixel, 0.0f, 1.0f);
            }
            const int alpha = static_cast<int>(std::lround(color.a() * coverage));
            if (alpha > 0) {
                surface()->DrawSetColor(Color(color.r(), color.g(), color.b(), alpha));
                surface()->DrawFilledRect(x + column, y + row, x + column + 1, y + row + 1);
            }
        }
    }
}

inline void Chevron(int width, int height, int direction, Color color, bool vertical = false, int x = 0, int y = 0) {
    constexpr int MinimumHalfSize = 2;
    constexpr int HeightDivisor = 6;
    const int half = (std::max)(MinimumHalfSize, (std::min)(width, height) / HeightDivisor);
    const int centerX = x + width / 2;
    const int centerY = y + height / 2;
    surface()->DrawSetColor(color);
    if (vertical) {
        surface()->DrawLine(centerX - half, centerY - direction * half / 2, centerX, centerY + direction * half / 2);
        surface()->DrawLine(centerX, centerY + direction * half / 2, centerX + half, centerY - direction * half / 2);
    } else {
        surface()->DrawLine(centerX - direction * half / 2, centerY - half, centerX + direction * half / 2, centerY);
        surface()->DrawLine(centerX + direction * half / 2, centerY, centerX - direction * half / 2, centerY + half);
    }
}

inline float Radius(Panel* panel, cse::ui::MaterialRole role) {
    (void)panel;
    const float radius = role == cse::ui::MaterialRole::Window || role == cse::ui::MaterialRole::Menu
        ? cse::ui::SurfaceRadius : cse::ui::ControlRadius;
    return static_cast<float>(Pixels(radius));
}

// Custom dialog shells retain their resource geometry and share the same material.
inline void Decorations(std::initializer_list<Panel*> panels) {
    auto& visibility = cse::ui::ControlsDecorations();
    for (Panel* panel : panels) {
        if (cse::ui::ControlsSmokedGlass()) {
            visibility.emplace(panel, panel->IsVisible());
            panel->SetVisible(false);
        } else {
            const auto original = visibility.find(panel);
            if (original != visibility.end()) {
                panel->SetVisible(original->second);
                visibility.erase(original);
            }
        }
    }
}

inline void Surface(Panel* panel, const cse::ui::BackdropRect& local,
    cse::ui::MaterialRole role = cse::ui::MaterialRole::Window) {
    cse::ui::BackdropRect bounds{};
    ipanel()->GetAbsPos(panel->GetVPanel(), bounds.x, bounds.y);
    bounds.x += local.x;
    bounds.y += local.y;
    bounds.width = local.width;
    bounds.height = local.height;
    int right = 0;
    int bottom = 0;
    cse::ui::BackdropRect clip{};
    ipanel()->GetClipRect(panel->GetVPanel(), clip.x, clip.y, right, bottom);
    clip.width = right - clip.x;
    clip.height = bottom - clip.y;
    const auto* style = cse::ui::ControlsStyle();
    auto backdrop = cse::ui::SmokedBackdrop;
    if (role == cse::ui::MaterialRole::Menu) {
        // The captured world supplies translucency; cover foreground controls behind a popup completely.
        backdrop.opacityTop = 1.0f;
        backdrop.opacityBottom = 1.0f;
    }
    if (!style || !style->DrawSurface(&bounds, &clip, &backdrop, Radius(panel, role))) {
        const Color fallback = ColorOf(cse::ui::SurfaceFallback);
        Fill(local.x, local.y, bounds.width, bounds.height, Radius(panel, role), fallback);
    }
    Fill(local.x, local.y, bounds.width, bounds.height, Radius(panel, role), ColorOf(cse::ui::Glaze));
    Outline(bounds.width, bounds.height, Radius(panel, role), ColorOf(cse::ui::Edge), local.x, local.y);
}

inline void Surface(Panel* panel, cse::ui::MaterialRole role = cse::ui::MaterialRole::Window) {
    Surface(panel, {0, 0, panel->GetWide(), panel->GetTall()}, role);
}

inline bool Background(Panel* panel, PanelMessageMap* map) {
    if (!cse::ui::ControlsSmokedGlass()) {
        return false;
    }
    const auto role = Role(map);
    const Color color = panel->GetBgColor();
    const bool material = role == cse::ui::MaterialRole::Menu ||
        (role == cse::ui::MaterialRole::Window && color.a() != 0);
    if (material) {
        Surface(panel, role);
        return true;
    }
    if (color.a() != 0) {
        Fill(0, 0, panel->GetWide(), panel->GetTall(),
            (role == cse::ui::MaterialRole::Control || role == cse::ui::MaterialRole::Content)
                ? Radius(panel, role) : 0.0f, color);
    }
    return true;
}

inline bool Border(Panel* panel, PanelMessageMap* map) {
    if (!cse::ui::ControlsSmokedGlass()) {
        return false;
    }
    const auto role = Role(map);
    if (role == cse::ui::MaterialRole::Label ||
        role == cse::ui::MaterialRole::Container || role == cse::ui::MaterialRole::MenuRow ||
        role == cse::ui::MaterialRole::Window || role == cse::ui::MaterialRole::Menu) {
        return true;
    }
    const auto color = panel->HasFocus() && panel->IsEnabled() ? cse::ui::Accent : cse::ui::Edge;
    Outline(panel->GetWide(), panel->GetTall(), Radius(panel, role), ColorOf(color));
    return true;
}

} // namespace smoked
} // namespace vgui2
