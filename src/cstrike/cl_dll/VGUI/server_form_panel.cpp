#include "server_form_panel.h"
#include "server_form_keys.h"
#include "server_form_text.h"

#include "../server_form_client.h"
#include "../../../public/cse_server_forms.h"
#include "../../../public/cse_server_forms_client.h"
#include "Button.h"
#include "CheckButton.h"
#include "Divider.h"
#include "Frame.h"
#include "Label.h"
#include "Menu.h"
#include "PHandle.h"
#include "ProgressBar.h"
#include "RadioButton.h"
#include "SectionedListPanel.h"
#include "Slider.h"
#include "SmokedGlass.h"
#include "ToggleButton.h"
#include "game_controls/iviewport.h"
#include "vgui/IInput.h"
#include "vgui/IPanel.h"
#include "vgui/IScheme.h"
#include "vgui/ISurface.h"
#include "vgui/KeyValues.h"
#include "vgui_controls/ComboBox.h"
#include "vgui_controls/Controls.h"

#include <algorithm>
#include <array>
#include <bitset>
#include <cmath>
#include <string>
#include <utility>
#include <vector>

namespace cse::forms {

namespace {

class FormPanel;
FormPanel* activeForm = nullptr;

void SetCaption(vgui2::Label& label, const std::string& text, char key = '\0') {
    label.SetText(Caption(text, key).c_str());
    label.SetHotkey(0);
}

struct Action {
    FormPanel& form;
    unsigned control;

    void Send(std::string_view value = {}) const;
};

class FormButton final : public vgui2::Button {
public:
    FormButton(vgui2::Panel* parent, Action action)
        : Button(parent, "Action", ""), action_(action) {}

    void DoClick() override {
        Button::DoClick();
        action_.Send();
    }

private:
    Action action_;
};

class FormCheck final : public vgui2::CheckButton {
public:
    FormCheck(vgui2::Panel* parent, Action action)
        : CheckButton(parent, "Check", ""), action_(action) {}

    void DoClick() override {
        CheckButton::DoClick();
        action_.Send(IsSelected() ? "1" : "0");
    }

private:
    Action action_;
};

class FormToggle final : public vgui2::ToggleButton {
public:
    FormToggle(vgui2::Panel* parent, Action action, bool selected)
        : ToggleButton(parent, "Toggle", ""), action_(action) {
        SetSelected(selected);
        ForceDepressed(selected);
    }

    void DoClick() override {
        ToggleButton::DoClick();
        action_.Send(IsSelected() ? "1" : "0");
    }

private:
    Action action_;
};

class FormRadio final : public vgui2::RadioButton {
public:
    FormRadio(vgui2::Panel* parent, Action action)
        : RadioButton(parent, "Radio", ""), action_(action) {}

    void InitializeSelection(bool selected) {
        RadioButton::SetSelected(false);
        if (selected) {
            RadioButton::SetSelected(true);
        }
    }

    void SetSelected(bool selected) override {
        const bool changed = IsSelected() != selected;
        RadioButton::SetSelected(selected);
        if (changed && IsSelected()) {
            action_.Send("1");
        }
    }

    // The owning form updates peers synchronously as several keyboard events can arrive in one frame.
    void OnRadioButtonChecked(int) override {}

private:
    Action action_;
};

class FormSlider final : public vgui2::Slider {
public:
    FormSlider(vgui2::Panel* parent, Action action, const Control& control)
        : Slider(parent, "Slider"), action_(action), reportedValue_(control.initial) {
        SetRange(control.first, control.second);
        SetValue(control.initial, false);
    }

    void SendSliderMovedMessage() override {
        if (!dragging_ && adjustmentKeys_.none()) {
            ReportIfChanged();
        }
    }

    void OnMousePressed(vgui2::MouseCode code) override {
        if (code == vgui2::MOUSE_LEFT) {
            dragging_ = true;
            Slider::OnMousePressed(code);
            // Native Slider captures the thumb, but a track click must also commit on release.
            vgui2::input()->SetMouseCapture(GetVPanel());
        }
    }

    void OnMouseReleased(vgui2::MouseCode code) override {
        if (code == vgui2::MOUSE_LEFT) {
            FinishDrag();
            SendSliderMovedMessage();
        }
    }

    void CancelAdjustment() {
        FinishDrag();
        adjustmentKeys_.reset();
        SetValue(reportedValue_, false);
    }

    void OnKeyCodeTyped(vgui2::KeyCode code) override {
        if (IsAdjustmentKey(code)) {
            adjustmentKeys_.set(code);
        }
        if (code == vgui2::KEY_LEFT || code == vgui2::KEY_RIGHT) {
            int minimum = 0;
            int maximum = 0;
            GetRange(minimum, maximum);
            const int value = GetValue();
            if (code == vgui2::KEY_LEFT && value > minimum) {
                SetValue(value - 1);
            } else if (code == vgui2::KEY_RIGHT && value < maximum) {
                SetValue(value + 1);
            }
        } else {
            Slider::OnKeyCodeTyped(code);
        }
    }

    void OnKeyCodeReleased(vgui2::KeyCode code) override {
        if (IsAdjustmentKey(code)) {
            adjustmentKeys_.reset(code);
            SendSliderMovedMessage();
        }
        Slider::OnKeyCodeReleased(code);
    }

    void OnKillFocus() override {
        adjustmentKeys_.reset();
        SendSliderMovedMessage();
        Slider::OnKillFocus();
    }

private:
    static bool IsAdjustmentKey(vgui2::KeyCode code) {
        return code == vgui2::KEY_LEFT || code == vgui2::KEY_RIGHT || code == vgui2::KEY_HOME ||
            code == vgui2::KEY_END || code == vgui2::KEY_PAGEUP || code == vgui2::KEY_PAGEDOWN;
    }

    void FinishDrag() {
        if (dragging_) {
            dragging_ = false;
            Slider::OnMouseReleased(vgui2::MOUSE_LEFT);
        }
    }

    void ReportIfChanged() {
        const int value = GetValue();
        if (value != reportedValue_) {
            reportedValue_ = value;
            action_.Send(std::to_string(value));
        }
    }

    Action action_;
    int reportedValue_;
    std::bitset<vgui2::KEY_SCROLLLOCKTOGGLE + 1> adjustmentKeys_;
    bool dragging_ = false;
};

class FormSelect : public vgui2::ComboBox {
    DECLARE_CLASS_SIMPLE_NAMED(FormSelect, vgui2::ComboBox, "ComboBox");

public:
    FormSelect(vgui2::Panel* parent, Action action, const Control& control)
        : ComboBox(parent, "Select", static_cast<int>(control.options.size()), false), action_(action) {
        for (int index = 0; index < GetChildCount(); ++index) {
            if (auto* arrow = dynamic_cast<vgui2::ComboBoxButton*>(GetChild(index))) {
                // Let the field handle both the caption and arrow as one click target.
                arrow->SetMouseInputEnabled(false);
                arrow->SetKeyBoardInputEnabled(false);
                break;
            }
        }
        for (const auto& option : control.options) {
            const int row = static_cast<int>(options_.size());
            options_.push_back(WideText(option));
            GetMenu()->AddMenuItem("Option", Caption(option).c_str(),
                new KeyValues("ServerFormSelection", "row", row), this, nullptr);
        }
        SetText(options_[control.first].c_str());
    }

    void OnMousePressed(vgui2::MouseCode code) override {
        if (code == vgui2::MOUSE_LEFT && IsEnabled()) {
            // VGUI already resolved the click to this field inside the modal form.
            RequestFocus(0);
            DoClick();
        }
    }

    void OnKeyTyped(wchar_t) override {}

    void OnKeyCodeTyped(vgui2::KeyCode code) override {
        if (IsDropdownVisible() && (code == vgui2::KEY_UP || code == vgui2::KEY_DOWN)) {
            return;
        }
        ComboBox::OnKeyCodeTyped(code);
    }

private:
    MESSAGE_FUNC_INT(OnSelection, "ServerFormSelection", row) {
        if (row >= 0 && static_cast<std::size_t>(row) < options_.size()) {
            SetText(options_[row].c_str());
            HideMenu();
            RequestFocus(0);
            action_.Send(std::to_string(row));
        }
    }

    Action action_;
    std::vector<std::wstring> options_;
};

class FormList : public vgui2::SectionedListPanel {
    DECLARE_CLASS_SIMPLE_NAMED(FormList, vgui2::SectionedListPanel, "SectionedListPanel");

public:
    FormList(vgui2::Panel* parent, Action action, const Control& control)
        : SectionedListPanel(parent, "ListBox"), action_(action) {
        AddSection(kOptionsSection, "");
        AddColumnToSection(kOptionsSection, kCaption, "", 0, GetWide());
        SetVerticalScrollbar(true);
        int initial = -1;
        for (std::size_t index = 0; index < control.options.size(); ++index) {
            auto* row = new KeyValues("Option");
            row->SetWString(kCaption, Caption(control.options[index]).c_str());
            row->SetInt(kIndex, static_cast<int>(index));
            const int id = AddItem(kOptionsSection, row);
            row->deleteThis();
            if (index == static_cast<std::size_t>(control.first)) {
                initial = id;
            }
        }
        SetSelectedItem(initial);
        AddActionSignalTarget(this);
    }

protected:
    void OnSizeChanged(int wide, int tall) override {
        SectionedListPanel::OnSizeChanged(wide, tall);
        constexpr int kScrollbarAndInsets = 24;
        ModifyColumn(kOptionsSection, kCaption, L"", (std::max)(1, wide - kScrollbarAndInsets));
    }

private:
    MESSAGE_FUNC_INT(OnSelection, "ItemSelected", itemID) {
        if (itemID >= 0 && itemID == GetSelectedItem()) {
            if (auto* row = GetItemData(itemID)) {
                action_.Send(std::to_string(row->GetInt(kIndex)));
            }
        }
    }

    static constexpr int kOptionsSection = 0;
    static constexpr char kCaption[] = "caption";
    static constexpr char kIndex[] = "index";
    Action action_;
};

class FormText final : public vgui2::TextEntry {
public:
    FormText(vgui2::Panel* parent, Action action, const std::string& initial)
        : TextEntry(parent, "Text"), action_(action) {
        SetAllowNonAsciiCharacters(true);
        SetMaximumCharCount(kMaxValueBytes);
        SetText(WideText(initial).c_str());
    }

    void OnKeyCodeTyped(vgui2::KeyCode code) override {
        if (!IsEnter(code)) {
            TextEntry::OnKeyCodeTyped(code);
            return;
        }
        if (!submitted_) {
            submitted_ = true;
            std::array<wchar_t, kMaxValueBytes + 1> input{};
            std::array<char, kMaxValueBytes + 1> value{};
            GetText(input.data(), static_cast<int>(input.size()));
            // The converter writes only whole UTF-8 characters into the zeroed buffer.
            if (!legacy_text::WideCellsToUtf8(input.data(), value.data(), static_cast<int>(value.size()))) {
                SetText(WideText(value.data()).c_str());
                GotoTextEnd();
            }
            action_.Send(value.data());
        }
    }

    void OnKeyCodeReleased(vgui2::KeyCode code) override {
        if (IsEnter(code)) {
            submitted_ = false;
        }
        TextEntry::OnKeyCodeReleased(code);
    }

    void OnKillFocus() override {
        submitted_ = false;
        TextEntry::OnKillFocus();
    }

private:
    static bool IsEnter(vgui2::KeyCode code) {
        return code == vgui2::KEY_ENTER || code == vgui2::KEY_PAD_ENTER;
    }

    Action action_;
    bool submitted_ = false;
};

class FormImage final : public vgui2::Panel {
public:
    FormImage(vgui2::Panel* parent, const Image& image)
        : Panel(parent, "Image"), image_(image) {
        SetPaintBackgroundEnabled(false);
    }

    ~FormImage() override {
        if (texture_) {
            vgui2::surface()->DeleteTextureByID(texture_);
        }
    }

    void Paint() override {
        auto* surface = vgui2::surface();
        if (!texture_) {
            texture_ = surface->CreateNewTextureID(true);
            if (!texture_) {
                return;
            }
            surface->DrawSetTextureRGBA(texture_, image_.pixels.data(), image_.width, image_.height, true, true);
        } else if (!surface->IsTextureIDValid(texture_)) {
            surface->DrawSetTextureRGBA(texture_, image_.pixels.data(), image_.width, image_.height, true, true);
        }
        const double scale = (std::min)(static_cast<double>(GetWide()) / image_.width,
            static_cast<double>(GetTall()) / image_.height);
        const int width = (std::max)(1, static_cast<int>(image_.width * scale));
        const int height = (std::max)(1, static_cast<int>(image_.height * scale));
        const int x = (GetWide() - width) / 2;
        const int y = (GetTall() - height) / 2;
        constexpr int FullIntensity = 255;
        surface->DrawSetColor(FullIntensity, FullIntensity, FullIntensity, FullIntensity);
        surface->DrawSetTexture(texture_);
        surface->DrawTexturedRect(x, y, x + width, y + height);
    }

private:
    Image image_;
    int texture_ = 0;
};

class FormGroup final : public vgui2::Panel {
public:
    FormGroup(vgui2::Panel* parent, const std::string& caption)
        : Panel(parent, "Group"), title_(new vgui2::Label(this, "GroupTitle", "")) {
        SetPaintBackgroundEnabled(false);
        SetCaption(*title_, caption);
        title_->SetMouseInputEnabled(false);
        SetZPos(-1);
    }

    void ApplySchemeSettings(vgui2::IScheme* scheme) override {
        Panel::ApplySchemeSettings(scheme);
        SetBorder(scheme->GetBorder("ButtonBorder"));
    }

    void PerformLayout() override {
        Panel::PerformLayout();
        constexpr int TitleInset = 10;
        constexpr int TitleHeight = 30;
        title_->SetBounds(TitleInset, 0, (std::max)(0, GetWide() - TitleInset * 2),
            (std::min)(GetTall(), TitleHeight));
    }

private:
    vgui2::Label* title_;
};

struct FormWidget {
    vgui2::Panel* panel = nullptr;
    vgui2::Label* shortcut = nullptr;
};

class FormPanel final : public vgui2::Frame {
public:
    FormPanel(vgui2::VPANEL parent, const Form& form, std::uint32_t token, ImageLookupFn imageLookup)
        : Frame(nullptr, "ServerForm", false, false), form_(form), token_(token) {
        SetParent(parent);
        SetScheme("ClientScheme");
        SetTitle(FormWindowTitle(form.title).c_str(), false);
        SetMenuButtonVisible(false);
        SetMinimizeButtonVisible(false);
        SetMaximizeButtonVisible(false);
        SetCloseButtonVisible(true);
        SetMoveable(false);
        SetSizeable(false);
        SetMinimumSize(1, 1);

        widgets_.resize(form.controls.size());
        std::array<int, kMaxControls> groupTabPositions{};
        for (std::size_t index = 0; index < form.controls.size(); ++index) {
            const auto& control = form.controls[index];
            const auto id = static_cast<unsigned>(index + 1);
            const Action action{*this, id};
            auto& widget = widgets_[index];
            switch (control.type) {
            case ControlType::Label: {
                auto* label = new vgui2::Label(this, "Label", "");
                SetCaption(*label, control.text);
                widget.panel = label;
                break;
            }
            case ControlType::Button: {
                auto* button = new FormButton(this, action);
                SetCaption(*button, control.text, control.key);
                widget.panel = button;
                break;
            }
            case ControlType::Check: {
                auto* check = new FormCheck(this, action);
                SetCaption(*check, control.text, control.key);
                check->SetSelected(control.first != 0);
                widget.panel = check;
                break;
            }
            case ControlType::Toggle: {
                auto* toggle = new FormToggle(this, action, control.first != 0);
                SetCaption(*toggle, control.text, control.key);
                widget.panel = toggle;
                break;
            }
            case ControlType::Radio: {
                auto* radio = new FormRadio(this, action);
                SetCaption(*radio, control.text, control.key);
                auto& groupTabPosition = groupTabPositions[control.first];
                if (!groupTabPosition) {
                    groupTabPosition = id;
                }
                radio->SetTabPosition(groupTabPosition);
                radio->SetSubTabPosition(id);
                radio->InitializeSelection(control.second != 0);
                widget.panel = radio;
                continue;
            }
            case ControlType::Slider:
                widget.panel = new FormSlider(this, action, control);
                break;
            case ControlType::Select:
                widget.panel = new FormSelect(this, action, control);
                break;
            case ControlType::List:
                widget.panel = new FormList(this, action, control);
                break;
            case ControlType::Progress: {
                auto* progress = new vgui2::ProgressBar(this, "ProgressBar");
                progress->SetProgress(static_cast<float>(control.initial - control.first) /
                    static_cast<float>(control.second - control.first));
                widget.panel = progress;
                break;
            }
            case ControlType::Text:
                widget.panel = new FormText(this, action, control.text);
                break;
            case ControlType::Image:
                if (const auto* image = imageLookup ? imageLookup(control.text) : nullptr) {
                    widget.panel = new FormImage(this, *image);
                }
                break;
            case ControlType::Divider:
                widget.panel = new vgui2::Divider(this, "Divider");
                break;
            case ControlType::Group:
                widget.panel = new FormGroup(this, control.text);
                break;
            }
            if (widget.panel) {
                SetWidgetInput(control, widget, static_cast<int>(id));
            }
        }
    }

    ~FormPanel() override {
        if (activeForm == this) {
            ReleaseModal();
            activeForm = nullptr;
        }
    }

    void ApplySchemeSettings(vgui2::IScheme* scheme) override {
        Frame::ApplySchemeSettings(scheme);
        // ClientScheme's generic panel color is transparent. Use the window surface for modal forms.
        const Color fallback = vgui2::smoked::ColorOf(cse::ui::SurfaceFallback);
        const Color background = GetSchemeColor("WindowBgColor", fallback, scheme);
        SetBgColor(background.a() ? background : fallback);
    }

    void Present() {
        if (IsVisible()) {
            return;
        }
        previousModal_.Set(vgui2::input()->GetAppModalSurface());
        MakeReadyForUse();
        FitToViewport();
        Activate();
        vgui2::input()->SetAppModalSurface(GetVPanel());
    }

    void Conceal() {
        if (IsVisible()) {
            for (std::size_t index = 0; index < widgets_.size(); ++index) {
                if (form_.controls[index].type == ControlType::Select) {
                    static_cast<FormSelect*>(widgets_[index].panel)->HideMenu();
                } else if (form_.controls[index].type == ControlType::Slider) {
                    static_cast<FormSlider*>(widgets_[index].panel)->CancelAdjustment();
                }
            }
            ReleaseModal();
            SetVisible(false);
        }
    }

    bool ConsumeKeyPress(int key, bool repeat, bool shortcutModifierDown) {
        if (!OwnsInput()) {
            return false;
        }
        if (repeat) {
            return true;
        }
        if (key == K_ESCAPE) {
            for (std::size_t index = 0; index < widgets_.size(); ++index) {
                if (form_.controls[index].type == ControlType::Select) {
                    auto* select = static_cast<FormSelect*>(widgets_[index].panel);
                    if (select->IsDropdownVisible()) {
                        select->HideMenu();
                        select->RequestFocus(0);
                        return true;
                    }
                }
            }
            OnClose();
            return true;
        }

        const char shortcut = FormHotkey(key);
        if (shortcut && !shortcutModifierDown && !WidgetOwnsTyping()) {
            for (std::size_t index = 0; index < widgets_.size(); ++index) {
                if (form_.controls[index].key == shortcut) {
                    ActivateShortcut(form_.controls[index].type, widgets_[index].panel);
                    break;
                }
            }
        }
        return true;
    }

    void Report(unsigned control, std::string_view value) {
        if (activeForm == this && OwnsInput()) {
            const auto& source = form_.controls[control - 1];
            if (source.type == ControlType::Radio) {
                ClearOtherRadios(source.first, control - 1);
            }
            SendFormAction(token_, control, value);
        }
    }

    void OnClose() override {
        if (activeForm == this) {
            const auto token = token_;
            activeForm = nullptr;
            Conceal();
            MarkForDeletion();
            SendFormAction(token, 0, {});
        }
    }

    void OnThink() override {
        Frame::OnThink();
        FitToViewport();
    }

    void OnKeyCodePressed(vgui2::KeyCode) override {}
    void OnKeyTyped(wchar_t) override {}

    void OnKeyCodeTyped(vgui2::KeyCode code) override {
        if (code == vgui2::KEY_TAB) {
            Panel::OnKeyCodeTyped(code);
        }
    }

private:
    void ClearOtherRadios(int group, std::size_t selected) {
        for (std::size_t index = 0; index < widgets_.size(); ++index) {
            const auto& control = form_.controls[index];
            if (index != selected && control.type == ControlType::Radio && control.first == group) {
                auto* radio = static_cast<FormRadio*>(widgets_[index].panel);
                if (radio->IsSelected()) {
                    radio->SetSelected(false);
                }
            }
        }
    }

    bool OwnsInput() {
        return IsVisible() && vgui2::ipanel()->IsVisible(GetVParent()) &&
            vgui2::input()->GetAppModalSurface() == GetVPanel();
    }

    void ReleaseModal() {
        if (vgui2::input()->GetAppModalSurface() == GetVPanel()) {
            vgui2::input()->SetMouseCapture(0);
            const auto previous = previousModal_.Get();
            if (previous && vgui2::ipanel()->IsVisible(previous)) {
                vgui2::input()->SetAppModalSurface(previous);
            } else {
                vgui2::input()->ReleaseAppModalSurface();
            }
        }
    }

    bool WidgetOwnsTyping() {
        const auto focus = vgui2::input()->GetFocus();
        for (std::size_t index = 0; index < widgets_.size(); ++index) {
            const auto& widget = widgets_[index];
            switch (form_.controls[index].type) {
            case ControlType::Text:
                if (widget.panel->GetVPanel() == focus) {
                    return true;
                }
                break;
            case ControlType::Select:
                if (static_cast<FormSelect*>(widget.panel)->IsDropdownVisible()) {
                    return true;
                }
                break;
            default:
                break;
            }
        }
        return false;
    }

    static void ActivateShortcut(ControlType type, vgui2::Panel* panel) {
        switch (type) {
        case ControlType::Button:
        case ControlType::Check:
        case ControlType::Toggle:
        case ControlType::Radio:
            static_cast<vgui2::Button*>(panel)->DoClick();
            break;
        case ControlType::Select:
            panel->RequestFocus(0);
            static_cast<FormSelect*>(panel)->ShowMenu();
            break;
        case ControlType::Slider:
        case ControlType::Text:
        case ControlType::List:
            panel->RequestFocus(0);
            break;
        default:
            break;
        }
    }

    void SetWidgetInput(const Control& control, FormWidget& widget, int tabPosition) {
        switch (control.type) {
        case ControlType::Button:
        case ControlType::Check:
        case ControlType::Toggle:
            widget.panel->SetTabPosition(tabPosition);
            break;
        case ControlType::Slider:
        case ControlType::Select:
        case ControlType::Text:
        case ControlType::List:
            widget.panel->SetTabPosition(tabPosition);
            if (control.key) {
                widget.shortcut = new vgui2::Label(this, "Shortcut", "");
                SetCaption(*widget.shortcut, {}, control.key);
                widget.shortcut->SetContentAlignment(vgui2::Label::a_east);
                widget.shortcut->SetMouseInputEnabled(false);
                widget.shortcut->SetKeyBoardInputEnabled(false);
            }
            break;
        default:
            widget.panel->SetMouseInputEnabled(false);
            widget.panel->SetKeyBoardInputEnabled(false);
            break;
        }
    }

    void FitToViewport() {
        int viewportWidth = 0;
        int viewportHeight = 0;
        vgui2::ipanel()->GetSize(GetVParent(), viewportWidth, viewportHeight);
        if (viewportWidth <= 0 || viewportHeight <= 0) {
            vgui2::surface()->GetScreenSize(viewportWidth, viewportHeight);
        }

        int left = 0;
        int top = 0;
        int clientWidth = 0;
        int clientHeight = 0;
        GetClientArea(left, top, clientWidth, clientHeight);
        const int chromeWidth = GetWide() - clientWidth;
        const int chromeHeight = GetTall() - clientHeight;
        constexpr int ScreenMargin = 8;
        const int availableWidth = (std::max)(1, viewportWidth - 2 * ScreenMargin - chromeWidth);
        const int availableHeight = (std::max)(1, viewportHeight - 2 * ScreenMargin - chromeHeight);
        const double scale = (std::min)({1.0, static_cast<double>(availableWidth) / form_.width,
            static_cast<double>(availableHeight) / form_.height});
        const auto scaled = [scale](int value) { return static_cast<int>(std::lround(value * scale)); };
        const int width = scaled(form_.width) + chromeWidth;
        const int height = scaled(form_.height) + chromeHeight;
        const int x = (viewportWidth - width) / 2;
        const int y = (viewportHeight - height) / 2;
        int currentX = 0;
        int currentY = 0;
        GetPos(currentX, currentY);
        if (GetWide() != width || GetTall() != height || currentX != x || currentY != y) {
            SetBounds(x, y, width, height);
        }

        constexpr int ShortcutWidth = 30;
        for (std::size_t index = 0; index < widgets_.size(); ++index) {
            const auto& control = form_.controls[index];
            const auto& widget = widgets_[index];
            if (!widget.panel) {
                continue;
            }
            const int controlX = left + scaled(control.x);
            const int controlY = top + scaled(control.y);
            int controlWidth = (std::max)(1, scaled(control.x + control.width) - scaled(control.x));
            const int controlHeight = (std::max)(1, scaled(control.y + control.height) - scaled(control.y));
            if (widget.shortcut) {
                const bool showShortcut = controlWidth >= ShortcutWidth * 2;
                widget.shortcut->SetVisible(showShortcut);
                if (showShortcut) {
                    controlWidth -= ShortcutWidth;
                    widget.shortcut->SetBounds(controlX + controlWidth, controlY, ShortcutWidth, controlHeight);
                }
            }
            widget.panel->SetBounds(controlX, controlY, controlWidth, controlHeight);
        }
    }

    Form form_;
    std::vector<FormWidget> widgets_;
    std::uint32_t token_;
    vgui2::VPanelHandle previousModal_;
};

void Action::Send(std::string_view value) const {
    form.Report(control, value);
}

class ServerFormInput final : public ClientInput {
public:
    bool ConsumeKeyPress(int key, bool repeat, bool shortcutModifierDown) override {
        return activeForm && activeForm->ConsumeKeyPress(key, repeat, shortcutModifierDown);
    }

    void EndServerFormsSession() override {
        cse::forms::EndServerFormsSession();
    }
};

EXPOSE_SINGLE_INTERFACE(ServerFormInput, ClientInput, ClientInputVersion);

}

void ShowFormPanel(const Form& form, std::uint32_t token, ImageLookupFn imageLookup) {
    CloseFormPanel();
    const auto viewport = gViewPortInterface ? gViewPortInterface->GetViewPortPanel() : 0;
    const auto parent = viewport ? vgui2::ipanel()->GetParent(viewport) : 0;
    if (parent) {
        activeForm = new FormPanel(parent, form, token, imageLookup);
        if (vgui2::ipanel()->IsVisible(parent)) {
            activeForm->Present();
        }
    }
}

void CloseFormPanel() {
    if (auto* form = std::exchange(activeForm, nullptr)) {
        form->Conceal();
        delete form;
    }
}

void SetFormPanelClientUIActive(bool active) {
    if (activeForm) {
        if (active) {
            activeForm->Present();
        } else {
            activeForm->Conceal();
        }
    }
}

}
