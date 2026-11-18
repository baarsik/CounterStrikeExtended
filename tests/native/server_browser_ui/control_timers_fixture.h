#pragma once

#include "AnimatingImagePanel.h"
#include "Button.h"
#include "ScrollBar.h"
#include "TextEntry.h"
#include "Tooltip.h"
#include "system_fixture.h"

namespace control_timers_fixture {

template <typename Tag, typename Tag::Type Member>
struct ReadMember {
    friend typename Tag::Type Read(Tag) {
        return Member;
    }
};

struct Caret {
    using Type = bool vgui2::TextEntry::*;
    friend Type Read(Caret);
};

template struct ReadMember<Caret, &vgui2::TextEntry::_cursorBlink>;

struct Frame {
    using Type = int vgui2::AnimatingImagePanel::*;
    friend Type Read(Frame);
};

template struct ReadMember<Frame, &vgui2::AnimatingImagePanel::m_iCurrentImage>;

class Animation final : public vgui2::AnimatingImagePanel {
public:
    using AnimatingImagePanel::AnimatingImagePanel;
    using AnimatingImagePanel::OnTick;
};

inline void Run(vgui2::Panel *parent, const std::string &name) {
    auto &clock = system_fixture::current->timeSecondsOverride;
    constexpr double DaySeconds = 86400.0;
    constexpr double SignedLimitSeconds = 2147483648.0 / 1000.0;
    constexpr double WrapSeconds = 2.0 * SignedLimitSeconds;
    constexpr double BeforeBoundary = 0.050;
    for (double start : {1.0, SignedLimitSeconds - BeforeBoundary, 25.0 * DaySeconds,
                         WrapSeconds - BeforeBoundary, 50.0 * DaySeconds}) {
        clock = start;
        if (name == "control-timer-caret") {
            vgui2::TextEntry entry(parent, "TimedCaret");
            entry.ResetCursorBlink();
            clock = start + 0.399;
            entry.OnKeyFocusTicked();
            Require(!(entry.*Read(Caret{})), "caret blinked before its delay");
            clock = start + 0.402;
            entry.OnKeyFocusTicked();
            Require(entry.*Read(Caret{}), "caret did not blink after its delay");
        } else if (name == "control-timer-animation") {
            Animation animation(parent, "TimedAnimation");
            animation.AddImage(nullptr);
            animation.AddImage(nullptr);
            animation.StartAnimation();
            animation.OnTick();
            Require(animation.*Read(Frame{}) == 1, "first animation frame did not advance");
            clock = start + 0.098;
            animation.OnTick();
            Require(animation.*Read(Frame{}) == 1, "animation advanced before its frame interval");
            clock = start + 0.102;
            animation.OnTick();
            Require(animation.*Read(Frame{}) == 0, "animation did not advance to its next frame");
        } else if (name == "control-timer-scrollbar") {
            vgui2::ScrollBar bar(parent, "TimedScroll", true);
            bar.SetRange(0, 100);
            bar.SetRangeWindow(1);
            bar.SetButtonPressedScrollValue(1);
            bar.OnMouseFocusTicked();
            bar.GetButton(1)->ForceDepressed(true);
            bar.OnMouseFocusTicked();
            Require(bar.GetValue() == 1, "scrollbar did not step on first press");
            clock = start + 0.398;
            bar.OnMouseFocusTicked();
            Require(bar.GetValue() == 1, "held scrollbar repeated before its delay");
            clock = start + 0.402;
            bar.OnMouseFocusTicked();
            bar.OnMouseFocusTicked();
            Require(bar.GetValue() == 2, "held scrollbar did not repeat");
        } else {
            Require(name == "control-timer-tooltip", "unknown control timer scenario");
            vgui2::Panel anchor(parent, "TimedTooltip");
            auto *tip = anchor.GetTooltip();
            tip->SetText("Delayed tooltip");
            tip->ResetDelay();
            tip->SetTooltipDelay(1);
            tip->ShowTooltip(&anchor);
            auto *window = anchor.FindChildByName("tooltip", false);
            Require(window && !window->IsVisible(), "tooltip displayed before its delay");
            clock = start + 0.502;
            tip->PerformLayout();
            Require(window->IsVisible(), "tooltip did not appear after its delay");
            tip->HideTooltip();
        }
    }
}
}
