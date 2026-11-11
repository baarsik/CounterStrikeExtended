#pragma once
#include "ServerBrowserDialog.h"
#include "Label.h"
#include "ListPanel.h"
#include "PropertySheet.h"
#include "PropertyPage.h"
#include "ScrollBar.h"
#include "ComboBox.h"
#include "Menu.h"
#include "MenuItem.h"
#include "vgui/IVGui.h"
#include "vgui/ISurface.h"
#include "vgui/KeyValues.h"
#include "check.h"
#include <array>
#include <string>

namespace layout_fixture {
inline vgui2::Panel *Child(vgui2::Panel &root, const char *name) {
    auto *child = root.FindChildByName(name, true);
    Require(child != nullptr, (std::string("missing layout control: ") + name).c_str());
    return child;
}
inline void FitsLabel(vgui2::Label *label, const char *name) {
    Require(label != nullptr, "layout control is not an actual Label");
    int width = 0;
    int height = 0;
    label->GetContentSize(width, height);
    height = std::max(height, vgui2::surface()->GetFontTall(label->GetFont()));
    Require(label->GetWide() >= width && label->GetTall() >= height,
            (std::string(name) + " clips actual font content: needs " + std::to_string(width) + "x" +
             std::to_string(height) + " has " + std::to_string(label->GetWide()) + "x" +
             std::to_string(label->GetTall()))
                .c_str());
}
inline void Fits(vgui2::Panel &root, const char *name) {
    FitsLabel(dynamic_cast<vgui2::Label *>(Child(root, name)), name);
}
// Measure the actual focused descendant against the reserved viewport, not its
// immediate content child (which may be an entire sheet or a wide list).
inline void VisibleWithin(vgui2::Panel &target, vgui2::Panel &overflow) {
    auto *horizontal = overflow.FindChildByName("HorizontalOverflow", false);
    auto *vertical = overflow.FindChildByName("VerticalOverflow", false);
    Require(horizontal && vertical, "focus visibility requires the actual overflow bars");
    int x = 0;
    int y = 0;
    target.LocalToScreen(x, y);
    overflow.ScreenToLocal(x, y);
    Require(x >= 0 && y >= 0 && x + target.GetWide() <= horizontal->GetWide() &&
                y + target.GetTall() <= vertical->GetTall(),
            (std::string("focused ") + target.GetName() + " outside " + overflow.GetName() + " viewport: " +
             std::to_string(x) + "," + std::to_string(y) + " size=" + std::to_string(target.GetWide()) + "x" +
             std::to_string(target.GetTall()) + " viewport=" + std::to_string(horizontal->GetWide()) + "x" +
             std::to_string(vertical->GetTall())).c_str());
}
using Tabs = std::array<vgui2::Label *, 6>;
using TabBounds = std::array<std::array<int, 4>, 6>;
inline Tabs LogicalTabs(vgui2::PropertySheet &sheet) {
    Tabs tabs{};
    for (size_t index = 0; index < tabs.size(); ++index) {
        std::array<char, 512> title{};
        Require(sheet.GetTabTitle(static_cast<int>(index), title.data(), static_cast<int>(title.size())),
                "logical browser tab is missing");
        for (int child = 0; child < sheet.GetChildCount(); ++child) {
            auto *label = dynamic_cast<vgui2::Label *>(sheet.GetChild(child));
            if (label && std::string(label->GetName()) == "tab") {
                std::array<char, 512> text{};
                label->GetText(text.data(), static_cast<int>(text.size()));
                if (text == title) {
                    Require(tabs[index] == nullptr, "fixture tab titles must identify one logical page");
                    tabs[index] = label;
                }
            }
        }
        Require(tabs[index] != nullptr, "logical title has no actual tab control");
    }
    return tabs;
}
inline TabBounds Bounds(const Tabs &tabs) {
    TabBounds bounds{};
    for (size_t index = 0; index < tabs.size(); ++index) {
        auto &rect = bounds[index];
        tabs[index]->GetBounds(rect[0], rect[1], rect[2], rect[3]);
    }
    return bounds;
}
inline void LogicalOrder(const Tabs &tabs, bool wrapped) {
    const auto bounds = Bounds(tabs);
    bool sawWrap = false;
    for (size_t index = 1; index < bounds.size(); ++index) {
        const auto &previous = bounds[index - 1];
        const auto &current = bounds[index];
        Require((current[1] == previous[1] && current[0] >= previous[0] + previous[2]) ||
                    (current[1] >= previous[1] + previous[3] && current[0] == 0),
                "visual tabs differ from the six logical page indices");
        sawWrap = sawWrap || current[1] > previous[1];
    }
    Require(sawWrap == wrapped, "tab fixture did not exercise the expected wrapped/single row geometry");
}
inline void Check(cse::server_browser::ServerBrowserDialog &dialog, int screenWidth, int screenHeight) {
    Require(dialog.GetWide() <= screenWidth && dialog.GetTall() <= screenHeight,
            "physical frame extends beyond actual engine workspace");
    for (const char *name :
         {"GameLabel",   "NameLabel",      "MapLabel",         "PingLabel", "PasswordLabel", "SecureLabel",
          "RegionLabel", "NonemptyFilter", "NotFullFilter",    "Apply",     "Refresh",       "Stop",
          "Quick",       "QuickSelected",  "Connect",          "Details",   "Favorite",      "RemoveFavorite",
          "AddAddress",  "StatusLabel",    "ActionStatusLabel"}) {
        Fits(dialog, name);
    }
    for (const char *name : {"GameFilter", "PasswordFilter", "SecureFilter", "RegionFilter"}) {
        auto *combo = dynamic_cast<vgui2::ComboBox *>(Child(dialog, name));
        auto *menu = combo->GetMenu();
        for (int index = 0; index < menu->GetItemCount(); ++index) {
            int width = 0;
            int height = 0;
            menu->GetMenuItem(menu->GetMenuID(index))->GetContentSize(width, height);
            Require(combo->GetWide() >= width + combo->GetTall() && combo->GetTall() >= height,
                    (std::string(name) + " cannot display its localized choices and arrow").c_str());
        }
    }
    auto *tabs = dynamic_cast<vgui2::PropertySheet *>(Child(dialog, "GameTabs"));
    int pageX = 0;
    int pageY = 0;
    tabs->GetActivePage()->GetPos(pageX, pageY);
    int tabCount = 0;
    for (int index = 0; index < tabs->GetChildCount(); ++index) {
        auto *tab = tabs->GetChild(index);
        if (std::string(tab->GetName()) != "tab") {
            continue;
        }
        ++tabCount;
        int x = 0;
        int y = 0;
        tab->GetPos(x, y);
        FitsLabel(dynamic_cast<vgui2::Label *>(tab), "tab");
        Require(y + tab->GetTall() <= pageY && x + tab->GetWide() <= tabs->GetWide(),
                "actual tab overlaps page/header or exceeds sheet width");
    }
    Require(tabCount == 6, "metric sheet lost a retained tab");
    auto *list = dynamic_cast<vgui2::ListPanel *>(Child(dialog, "ServerList"));
    int bottom = 0;
    for (const char *name : {"password", "secure", "name", "game", "map", "players", "ping", "address"}) {
        Fits(*list, name);
        auto *header = Child(*list, name);
        int x = 0;
        int y = 0;
        header->GetPos(x, y);
        bottom = std::max(bottom, y + header->GetTall());
    }
    auto *body = Child(dialog, "BrowserOverflow");
    auto *bar = static_cast<vgui2::ScrollBar *>(body->FindChildByName("VerticalOverflow", false));
    int tabX = 0;
    int tabY = 0;
    tabs->GetPos(tabX, tabY);
    bar->SetValue(tabY + pageY);
    vgui2::surface()->SolveTraverse(dialog.GetVPanel(), false);
    list->Repaint();
    vgui2::surface()->PaintTraverse(dialog.GetVPanel());
    if (list->GetItemCount()) {
        int x = 0;
        int y = 0;
        int width = 0;
        int height = 0;
        Require(list->GetCellBounds(0, 0, x, y, width, height) && y >= bottom,
                ("actual table header overlaps first displayed row: header=" + std::to_string(bottom) +
                 " row=" + std::to_string(y) + " listTall=" + std::to_string(list->GetTall()))
                    .c_str());
    }
    size_t allocated = 0;
    for (int item = list->FirstItem(); item != list->InvalidItemID(); item = list->NextItem(item)) {
        ++allocated;
    }
    Require(allocated <= dialog.WindowSize(), "metric layout allocated more rows than the visible window");
}
inline void Key(vgui2::Panel &panel, vgui2::KeyCode key) {
    vgui2::ivgui()->PostMessage(panel.GetVPanel(), new KeyValues("KeyCodeTyped", "code", key), 0, 0);
    vgui2::ivgui()->RunFrame();
}
}
