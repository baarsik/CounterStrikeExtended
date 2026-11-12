#pragma once
#include "ServerBrowserDialog.h"
#include "Label.h"
#include "ListPanel.h"
#include "PropertySheet.h"
#include "PropertyPage.h"
#include "ScrollBar.h"
#include "ComboBox.h"
#include "ToggleButton.h"
#include "Menu.h"
#include "MenuItem.h"
#include "vgui/IVGui.h"
#include "vgui/ISurface.h"
#include "vgui/IInput.h"
#include "vgui/IPanel.h"
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
inline void FocusVisible(vgui2::Panel &expected) {
    Require(vgui2::input()->GetFocus() == expected.GetVPanel(),
            (std::string("unexpected focus owner; expected ") + expected.GetName()).c_str());
    Require(expected.IsEnabled() && expected.IsVisible(), "focused control is hidden or disabled");
    int x = 0;
    int y = 0;
    expected.LocalToScreen(x, y);
    int left = 0;
    int top = 0;
    int right = 0;
    int bottom = 0;
    vgui2::ipanel()->GetClipRect(expected.GetVPanel(), left, top, right, bottom);
    Require(expected.GetWide() > 0 && expected.GetTall() > 0 && left <= x && top <= y &&
                right >= x + expected.GetWide() && bottom >= y + expected.GetTall(),
            (std::string("focused control is clipped: ") + expected.GetName()).c_str());
}
inline void Check(cse::server_browser::ServerBrowserDialog &dialog, int screenWidth, int screenHeight) {
    Require(dialog.GetWide() <= screenWidth && dialog.GetTall() <= screenHeight,
            "frame exceeds the engine workspace");
    Require(!dialog.IsProportional() && !dialog.IsHDProportional(), "browser inherited custom HD form scaling");
    Require(!dialog.FindChildByName("BrowserOverflow", true) && !dialog.FindChildByName("TableOverflow", true),
            "main page retains whole-form scrolling");
    auto *tabs = dynamic_cast<vgui2::PropertySheet *>(Child(dialog, "GameTabs"));
    Require(tabs && tabs->GetParent() == &dialog && tabs->GetNumPages() == 6, "original sheet parentage changed");
    Require(Child(dialog, "StatusLabel")->GetParent() == &dialog, "status is no longer a frame child");
    auto *page = tabs->GetActivePage();
    auto *list = dynamic_cast<vgui2::ListPanel *>(Child(*page, "gamelist"));
    Require(list && list->GetParent() == page, "gamelist is not a direct page child");
    auto *toggle = dynamic_cast<vgui2::ToggleButton *>(Child(*page, "Filter"));
    Require(toggle != nullptr, "original Filter toggle is absent");
    const int expectedHeight = page->GetTall() - (toggle->IsSelected() ? 144 : 52);
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
    list->GetBounds(x, y, width, height);
    Require(x == 0 && y == 8 && width == page->GetWide() && height == expectedHeight,
            "gamelist differs from original 624x278 resource anchors");
    for (const char *name : {"ConnectButton", "RefreshButton", "RefreshQuickButton", "Filter", "FilterString"}) {
        auto *control = Child(*page, name);
        Require(control->GetParent() == page, "action or filter summary left the page");
        control->GetPos(x, y);
        Require(y == page->GetTall() - 34, "action strip lost original bottom anchor");
    }
    for (const char *name : {"GameFilter", "MapFilter", "PingFilter", "SecureFilter", "ServerFullFilterCheck",
             "ServerEmptyFilterCheck", "NoPasswordFilterCheck", "ValidSteamAccountFilterCheck"}) {
        Require(Child(*page, name)->IsVisible() == toggle->IsSelected(), "filter toggle does not swap resources");
    }
    Require(!Child(*page, "GameFilter")->IsEnabled(), "active mod must disable game selection");
    for (const char *name : {"NameFilter", "RegionFilter", "PasswordFilter", "Apply", "FavoriteAddress"}) {
        Require(!dialog.FindChildByName(name, true), "custom-only control remains in main browser");
    }
    auto *internal = Child(*list, "VertScrollBar");
    Require(!internal->IsVisible(), "viewport has two effective scrollbars");
    Require(Child(*list, "LogicalScroll")->GetParent() == list, "logical scrollbar escaped gamelist");
    size_t allocated = 0;
    for (int item = list->FirstItem(); item != list->InvalidItemID(); item = list->NextItem(item)) {
        ++allocated;
    }
    Require(allocated <= dialog.WindowSize(), "layout allocated rows beyond bounded viewport");
}
inline void Key(vgui2::Panel &panel, vgui2::KeyCode key) {
    vgui2::ivgui()->PostMessage(panel.GetVPanel(), new KeyValues("KeyCodeTyped", "code", key), 0, 0);
    vgui2::ivgui()->RunFrame();
}
}
