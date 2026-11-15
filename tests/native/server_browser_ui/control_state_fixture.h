#pragma once

#include "Button.h"
#include "Menu.h"
#include "MenuItem.h"
#include "TextEntry.h"
#include "SmokedGlass.h"
#include "check.h"
#include "geometry_fixture.h"
#include "vgui/IScheme.h"
#include <algorithm>
#include <string>
#include <vector>

namespace control_state_fixture {

class SelectionInput : public vgui2::TextEntry {
public:
    explicit SelectionInput(vgui2::Panel* parent) : TextEntry(parent, "SelectionStateInput") {}
    bool focused = true;
    bool HasFocus() override { return focused; }
    bool GetSelectedRange(int& start, int& end) override {
        start = 1;
        end = 2;
        return true;
    }
};

inline void Run(vgui2::Panel* root, vgui2::HScheme scheme,
    geometry_fixture::BaseUISurface& geometry, float& style) {
    using namespace vgui2;
    const float previousStyle = style;
    std::vector<std::string> failures;
    const auto check = [&](bool passed, const char* message) {
        if (!passed) {
            failures.emplace_back(message);
        }
    };
    style = 1.0f;
    {
        Button ordinary(root, "OrdinaryAction", "Cancel");
        Button defaultAction(root, "DefaultAction", "OK");
        Button apply(root, "ApplyButton", "Apply");
        for (Button* button : {&ordinary, &defaultAction, &apply}) {
            button->SetScheme(scheme);
            button->MakeReadyForUse();
        }
        defaultAction.SetAsDefaultButton(1);
        check(defaultAction.GetButtonFgColor() == ordinary.GetButtonFgColor()
            && defaultAction.GetButtonBgColor() == ordinary.GetButtonBgColor(),
            "idle default action uses the same neutral colors as other idle actions");
        check(apply.GetButtonFgColor() == ordinary.GetButtonFgColor()
            && apply.GetButtonBgColor() == ordinary.GetButtonBgColor(),
            "Apply does not acquire a highlight from its resource name");
        ordinary.SetArmed(true);
        apply.SetArmed(true);
        check(apply.GetButtonFgColor() == ordinary.GetButtonFgColor()
            && apply.GetButtonBgColor() == ordinary.GetButtonBgColor(),
            "Apply follows the same hover state as other actions");
    }
    {
        Menu menu(root, "DisabledStateMenu");
        auto* cascade = new Menu(root, "DisabledStateCascade");
        auto* item = new MenuItem(&menu, "DisabledSystemAction", "Disabled action", cascade, false);
        menu.AddMenuItem(item);
        menu.SetScheme(scheme);
        menu.MakeReadyForUse();
        item->MakeReadyForUse();
        item->SetBounds(0, 0, 240, 32);
        item->SetEnabled(false);
        item->SetArmed(true);
        geometry.paintedTextColors.clear();
        geometry.paintedLineColors.clear();
        geometry.observeTextColors = true;
        geometry.observeLines = true;
        item->Paint();
        geometry.observeTextColors = false;
        geometry.observeLines = false;
        const Color disabled = smoked::ColorOf(cse::ui::DisabledText);
        check(!geometry.paintedTextColors.empty()
            && std::all_of(geometry.paintedTextColors.begin(), geometry.paintedTextColors.end(),
                [&](Color color) { return color == disabled; }),
            "disabled menu action paints every caption glyph in the disabled foreground");
        check(!geometry.paintedLineColors.empty()
            && std::all_of(geometry.paintedLineColors.begin(), geometry.paintedLineColors.end(),
                [&](Color color) { return color == disabled; }),
            "disabled cascading action also dims its submenu indicator");
        auto* checked = new MenuItem(&menu, "DisabledCheckedAction", "Checked action", nullptr, true);
        menu.AddMenuItem(checked);
        checked->MakeReadyForUse();
        checked->SetBounds(0, 32, 240, 32);
        checked->SetChecked(true);
        checked->SetEnabled(false);
        geometry.paintedLineColors.clear();
        geometry.observeLines = true;
        checked->Paint();
        geometry.observeLines = false;
        check(!geometry.paintedLineColors.empty()
            && std::all_of(geometry.paintedLineColors.begin(), geometry.paintedLineColors.end(),
                [&](Color color) { return color == disabled; }),
            "disabled checked action dims its checkmark with its caption");
        item->SetEnabled(true);
        geometry.paintedTextColors.clear();
        geometry.paintedLineColors.clear();
        geometry.observeTextColors = true;
        geometry.observeLines = true;
        item->Paint();
        geometry.observeTextColors = false;
        geometry.observeLines = false;
        const Color enabled = smoked::ColorOf(cse::ui::Text);
        check(!geometry.paintedTextColors.empty()
            && std::all_of(geometry.paintedTextColors.begin(), geometry.paintedTextColors.end(),
                [&](Color color) { return color == enabled; })
            && !geometry.paintedLineColors.empty()
            && std::all_of(geometry.paintedLineColors.begin(), geometry.paintedLineColors.end(),
                [&](Color color) { return color == enabled; }),
            "reenabled menu action restores caption and submenu indicator foregrounds");
    }
    {
        SelectionInput entry(root);
        entry.SetScheme(scheme);
        entry.SetBounds(0, 0, 240, 32);
        entry.MakeReadyForUse();
        const Color ordinaryText(220, 225, 230, 255);
        entry.SetFgColor(ordinaryText);
        for (bool focused : {true, false}) {
            entry.focused = focused;
            geometry.paintedTextColors.clear();
            geometry.paintedFilledRectColors.clear();
            geometry.observeTextColors = true;
            geometry.observeFilledRects = true;
            surface()->DrawSetTextFont(entry.GetFont());
            surface()->DrawSetTextColor(ordinaryText);
            int x = 0;
            x += entry.DrawChar(L'A', entry.GetFont(), 0, x, 0);
            x += entry.DrawChar(L'B', entry.GetFont(), 1, x, 0);
            entry.DrawChar(L'C', entry.GetFont(), 2, x, 0);
            geometry.observeTextColors = false;
            geometry.observeFilledRects = false;
            const Color selection = smoked::ColorOf(
                focused ? cse::ui::TextSelection : cse::ui::TextSelectionInactive);
            check(geometry.paintedFilledRectColors.size() == 1
                && geometry.paintedFilledRectColors[0] == selection
                && geometry.paintedFilledRectColors[0].a() == 255,
                focused ? "focused text selection uses an opaque background"
                        : "inactive text selection uses an opaque background");
            check(geometry.paintedTextColors.size() == 3
                && geometry.paintedTextColors[0] == ordinaryText
                && geometry.paintedTextColors[1] == smoked::ColorOf(cse::ui::Text)
                && geometry.paintedTextColors[2] == ordinaryText,
                focused ? "focused selection restores the ordinary foreground after its final glyph"
                        : "inactive selection restores the ordinary foreground after its final glyph");
        }
        style = 0.0f;
        entry.InvalidateLayout(true, true);
        entry.focused = true;
        geometry.paintedTextColors.clear();
        geometry.paintedFilledRectColors.clear();
        geometry.observeTextColors = true;
        geometry.observeFilledRects = true;
        entry.DrawChar(L'B', entry.GetFont(), 1, 0, 0);
        geometry.observeTextColors = false;
        geometry.observeFilledRects = false;
        IScheme* original = vgui2::scheme()->GetIScheme(scheme);
        check(geometry.paintedFilledRectColors.size() == 1
            && geometry.paintedFilledRectColors[0] == original->GetColor("SelectionBgColor", Color()),
            "Original retains its scheme selection background");
        check(geometry.paintedTextColors.size() == 1
            && geometry.paintedTextColors[0] == original->GetColor("SelectionFgColor", entry.GetFgColor()),
            "Original retains its scheme selected-text foreground");
    }
    {
        Button original(root, "ApplyButton", "Apply");
        const Color foreground(21, 43, 65, 255);
        const Color background(87, 109, 131, 255);
        original.SetDefaultColor(foreground, background);
        original.SetAsDefaultButton(1);
        check(original.GetButtonFgColor() == foreground && original.GetButtonBgColor() == background,
            "Original retains exact caller-supplied colors for default and Apply actions");
    }
    style = previousStyle;
    std::string message;
    for (const std::string& failure : failures) {
        message += "\n  " + failure;
    }
    Require(failures.empty(), message.c_str());
}

}
