#pragma once

#include "Button.h"
#include "Frame.h"
#include "Image.h"
#include "RadioButton.h"
#include "TextImage.h"
#include "geometry_fixture.h"
#include "check.h"
#include <cmath>
#include <string>
#include <vector>

namespace smoked_metrics_fixture {
class Window : public vgui2::Frame {
public:
    Window(vgui2::Panel* parent, const char* name) : Frame(parent, name, true, false) {}
    using Frame::GetClientArea;
};
class CustomAction : public vgui2::Button {
public:
    explicit CustomAction(vgui2::Panel* parent) : Button(parent, "CustomImageCaption", "Custom") {
        SetContentAlignment(vgui2::Label::a_northwest);
        SetTextInset(8, 8);
    }
};
class Radio : public vgui2::RadioButton {
public:
    explicit Radio(vgui2::Panel* parent) : RadioButton(parent, "GeometryRadio", "Style") {}
    using vgui2::Label::Paint;
};
inline void Run(vgui2::Panel* root, vgui2::HScheme scheme,
    geometry_fixture::BaseUISurface& geometry, float& style) {
    using namespace vgui2;
    int savedWidth = 0;
    int savedHeight = 0;
    surface()->GetScreenSize(savedWidth, savedHeight);
    const float savedStyle = style;
    geometry.Resize(2560, 1440);
    style = 1.0f;
    std::vector<std::string> failures;
    const auto check = [&](bool passed, const char* message) {
        if (!passed) {
            failures.emplace_back(message);
        }
    };
    {
        Panel form(root, "ResourceForm");
        form.SetHDProportional(true);
        Button resource(&form, "ResourceAction", "Apply");
        KeyValues* settings = new KeyValues("ResourceAction");
        settings->SetInt("wide", 100);
        settings->SetInt("tall", 24);
        static_cast<Panel&>(resource).ApplySettings(settings);
        resource.SetScheme(scheme);
        resource.MakeReadyForUse();
        check(resource.GetWide() == 133 && resource.GetTall() == 32,
            "legacy form rows match the shared 32-pixel control height at HD");
        KeyValues* serialized = new KeyValues("ResourceAction");
        static_cast<Panel&>(resource).GetSettings(serialized);
        static_cast<Panel&>(resource).ApplySettings(serialized);
        check(resource.GetWide() == 133 && resource.GetTall() == 32,
            "saving and reapplying Smoked resource settings preserves geometry");
        serialized->deleteThis();
        style = 0.0f;
        resource.InvalidateLayout(true, true);
        check(resource.GetWide() == 200 && resource.GetTall() == 48,
            "Original retains HD resource scaling");
        style = 1.0f;
        resource.InvalidateLayout(true, true);
        check(resource.GetWide() == 133 && resource.GetTall() == 32,
            "switching back to Smoked restores resource geometry without compounding its scale");
        settings->deleteThis();
        style = 1.0f;
    }
    {
        Window ordinary(root, "OrdinaryDialog");
        Window options(root, "OptionsDialog");
        for (Window* frame : {&ordinary, &options}) {
            frame->SetHDProportional(frame == &ordinary);
            frame->SetScheme(scheme);
            frame->SetBounds(0, 0, 640, 400);
            frame->MakeReadyForUse();
            frame->InvalidateLayout(true, false);
            int x = 0;
            int y = 0;
            int width = 0;
            int height = 0;
            frame->GetClientArea(x, y, width, height);
            check(x == 16 && y == 48 && width == 608 && height == 336,
                "all Smoked frames share one header and content inset regardless of proportional mode");
            int titleButtons = 0;
            for (int index = 0; index < frame->GetChildCount(); ++index) {
                auto* button = dynamic_cast<Button*>(frame->GetChild(index));
                if (!button || !button->IsVisible()) {
                    continue;
                }
                wchar_t text[8]{};
                button->GetText(text, 8);
                button->GetPos(x, y);
                if (!text[0] && y < 48) {
                    ++titleButtons;
                    check(button->GetWide() == 20 && button->GetTall() == 20 && y == 14,
                        "window icons use the same size and vertical center");
                }
            }
            check(titleButtons >= 2, "both system-menu and close icons were inspected");
        }
    }
    {
        Window document(root, "VGUITextWindow");
        document.SetScheme(scheme);
        document.SetTitleBarVisible(false);
        document.SetTitle("Map briefing", false);
        document.MakeReadyForUse();
        document.InvalidateLayout(true, false);
        int x = 0;
        int y = 0;
        int width = 0;
        int height = 0;
        document.GetClientArea(x, y, width, height);
        check(y == 48, "Smoked document content leaves room for the shared title header");
        style = 0.0f;
        document.InvalidateLayout(true, true);
        y = 0;
        document.GetClientArea(x, y, width, height);
        check(y == 0, "Original documents retain their titlebar-free client geometry");
        style = 1.0f;
    }
    {
        Button action(root, "CenteredAction", "Apply");
        action.SetScheme(scheme);
        action.SetBounds(0, 0, 200, 40);
        action.SetContentAlignment(Label::a_west);
        action.SetTextInset(12, 5);
        action.MakeReadyForUse();
        action.InvalidateLayout(true, false);
        static_cast<Panel&>(action).Paint();
        int x = 0;
        int y = 0;
        int width = 0;
        int height = 0;
        action.GetTextImage()->GetPos(x, y);
        action.GetTextImage()->GetSize(width, height);
        check(std::abs(x + x + width - 200) <= 2 && std::abs(y + y + height - 40) <= 2,
            "Smoked action captions paint centered despite legacy west alignment and text insets");
        style = 0.0f;
        static_cast<Panel&>(action).Paint();
        action.GetTextImage()->GetPos(x, y);
        check(x == 12, "Original keeps the caller's west alignment and text inset after a style switch");
        style = 1.0f;
    }
    {
        Button empty(root, "NarrowEmptyCaption", "");
        empty.SetScheme(scheme);
        empty.SetBounds(0, 0, 1, 24);
        empty.MakeReadyForUse();
        empty.InvalidateLayout(true, true);
        check(empty.GetFont() != 0, "a narrow empty action caption keeps a valid font without dividing by zero");
    }
    {
        TextImage icon("*");
        Button mixed(root, "TransportImageAction", "Play");
        mixed.SetScheme(scheme);
        mixed.SetBounds(0, 0, 200, 40);
        mixed.SetContentAlignment(Label::a_northwest);
        mixed.SetTextInset(8, 8);
        mixed.MakeReadyForUse();
        icon.SetFont(mixed.GetFont());
        mixed.AddImage(&icon, 0);
        mixed.InvalidateLayout(true, false);
        static_cast<Panel&>(mixed).Paint();
        int x = 0;
        int y = 0;
        mixed.GetTextImage()->GetPos(x, y);
        check(x == 8 && y == 8, "ordinary buttons with transport images keep their explicit image-caption geometry");
    }
    {
        CustomAction custom(root);
        custom.SetScheme(scheme);
        custom.SetBounds(0, 0, 200, 40);
        custom.MakeReadyForUse();
        custom.InvalidateLayout(true, false);
        static_cast<Panel&>(custom).Paint();
        int x = 0;
        int y = 0;
        custom.GetTextImage()->GetPos(x, y);
        check(x == 8 && y == 8, "custom button subclasses retain explicit caption alignment and insets");
    }
    {
        Radio radio(root);
        radio.SetScheme(scheme);
        radio.SetBounds(0, 0, 200, 32);
        radio.MakeReadyForUse();
        for (bool selected : {false, true}) {
            radio.SetSelected(selected);
            radio.InvalidateLayout(true, false);
            geometry.paintedFilledRects.clear();
            geometry.paintedFilledRectColors.clear();
            geometry.observeFilledRects = true;
            radio.Paint();
            geometry.observeFilledRects = false;
            int x = 0;
            int y = 0;
            dynamic_cast<Image*>(radio.GetImageAtIndex(0))->GetPos(x, y);
            const auto covered = [&](int px, int py) {
                for (const auto& rect : geometry.paintedFilledRects) {
                    if (px >= rect[0] && px < rect[2] && py >= rect[1] && py < rect[3]) {
                        return true;
                    }
                }
                return false;
            };
            check(!covered(x + 13, y + 9), "radio ring has a transparent interior instead of a tinted coin face");
            check(covered(x + 9, y + 9) == selected,
                "only a selected radio paints a small central dot");
        }
    }
    style = savedStyle;
    geometry.Resize(savedWidth, savedHeight);
    std::string message;
    for (const auto& failure : failures) {
        message += "\n  " + failure;
    }
    Require(failures.empty(), message.c_str());
}
} // namespace smoked_metrics_fixture
