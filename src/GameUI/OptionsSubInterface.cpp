#include "OptionsSubInterface.h"
#include "HudRadarDialog.h"
#include "Button.h"
#include "GameUI_Interface.h"
#include "wrect.h"
#include "cl_dll.h"

#include "ComboBox.h"
#include "Label.h"
#include "TextImage.h"
#include "RadioButton.h"
#include "Menu.h"
#include "../vgui2/controls/SmokedGlass.h"
#include "interface_language.h"
#include "interface_language_preference.h"
#include "localization_controls.h"
#include "../public/vgui_style.h"
#include "vgui/ISystem.h"
#include "vgui/KeyValues.h"
#include "vgui_controls/Controls.h"

#include <algorithm>

namespace {

constexpr char LanguageItemKey[] = "language";
constexpr char UIStyleItemKey[] = "style";
constexpr int UIStyleRows = 2;
constexpr int UIStyleTabPosition = 2;
constexpr int VisibleLanguageRows = static_cast<int>(
    sizeof(cse::interface_language::BundledLanguages) / sizeof(cse::interface_language::BundledLanguages[0])) + 1;
constexpr int LanguageTabPosition = 1;
constexpr int ContentInset = 16;
constexpr int LanguageLabelHeight = 20;
constexpr int LanguageComboHeight = 24;
constexpr int ControlGap = 8;
constexpr int NoticeGap = 12;
constexpr int MinimumNoticeHeight = 20;

class InterfaceStyleChoice : public vgui2::RadioButton {
public:
    InterfaceStyleChoice(vgui2::Panel* parent, const char* name, const char* text,
        vgui2::ComboBox* choices, int index) : RadioButton(parent, name, text), m_choices(choices), m_index(index) {
        KeyValues* settings = new KeyValues("InterfaceStyleChoice");
        settings->SetInt("tabPosition", UIStyleTabPosition);
        settings->SetInt("SubTabPosition", index);
        ApplySettings(settings);
        settings->deleteThis();
        SetVisible(false);
    }

    void SetSelected(bool selected) override {
        RadioButton::SetSelected(selected);
        if (selected) {
            m_choices->ActivateItem(m_choices->GetMenu()->GetMenuID(m_index));
        }
    }

private:
    vgui2::ComboBox* m_choices;
    int m_index;
};

class InterfaceNotice : public vgui2::Label {
public:
    InterfaceNotice(vgui2::Panel* parent, const char* name, const char* text)
        : vgui2::Label(parent, name, text) {
        // Label exposes wrapping through resource settings rather than a public setter.
        KeyValues* settings = new KeyValues("InterfaceNotice", "wrap", 1);
        ApplySettings(settings);
        settings->deleteThis();
        SetContentAlignment(vgui2::Label::a_northwest);
        SetTextInset(0, 0);
        SetTabPosition(0);
    }
};

int LayoutNotice(vgui2::Label* label, int inset, int top, int width, int minimumHeight) {
    label->SetBounds(inset, top, width, minimumHeight);
    label->GetTextImage()->SetSize(width, minimumHeight);
    int contentWidth = 0;
    int contentHeight = 0;
    // GetContentSize initializes the label font; measure wrapped text rather than the previous image height.
    label->GetContentSize(contentWidth, contentHeight);
    label->GetTextImage()->GetContentSize(contentWidth, contentHeight);
    const int height = std::max(minimumHeight, contentHeight);
    label->SetTall(height);
    return height;
}

} // namespace

COptionsSubInterface::COptionsSubInterface(vgui2::Panel* parent)
    : vgui2::PropertyPage(parent, "Interface", true) {
    m_languageLabel = new vgui2::Label(this, "LanguageLabel", "#CSEInterface_Language");
    m_languageLabel->SetTabPosition(0);
    m_languageCombo = new vgui2::ComboBox(this, "Language", VisibleLanguageRows, false);
    m_languageCombo->SetTabPosition(LanguageTabPosition);
    m_languageCombo->AddActionSignalTarget(this);
    m_languageLabel->SetAssociatedControl(m_languageCombo);

    KeyValues* automatic = new KeyValues("Language", LanguageItemKey, "");
    m_languageItemIds.push_back(m_languageCombo->AddItem("#CSEInterface_Automatic", automatic));
    automatic->deleteThis();
    for (const auto& language : cse::interface_language::BundledLanguages) {
        KeyValues* item = new KeyValues("Language", LanguageItemKey, language.id);
        m_languageItemIds.push_back(m_languageCombo->AddItem(language.nativeName, item));
        item->deleteThis();
    }

    m_saveFailed = new InterfaceNotice(this, "SaveFailed", "#CSEInterface_SaveFailed");
    m_launchOverride = new InterfaceNotice(this, "LaunchOverride", "#CSEInterface_LaunchOverride");
    m_uiStyleLabel = new vgui2::Label(this, "UIStyleLabel", "#CSEInterface_Style");
    m_uiStyleLabel->SetTabPosition(0);
    m_uiStyleCombo = new vgui2::ComboBox(this, "UIStyle", UIStyleRows, false);
    m_uiStyleCombo->SetTabPosition(UIStyleTabPosition);
    m_uiStyleCombo->AddActionSignalTarget(this);
    m_uiStyleLabel->SetAssociatedControl(m_uiStyleCombo);
    KeyValues* original = new KeyValues("UIStyle", UIStyleItemKey, cse::ui::OriginalVGuiStyle);
    m_uiStyleItemIds.push_back(m_uiStyleCombo->AddItem("#CSEInterface_StyleOriginal", original));
    original->deleteThis();
    KeyValues* smoked = new KeyValues("UIStyle", UIStyleItemKey, cse::ui::SmokedTacticalGlassVGuiStyle);
    m_uiStyleItemIds.push_back(m_uiStyleCombo->AddItem("#CSEInterface_StyleSmokedTacticalGlass", smoked));
    smoked->deleteThis();
    m_originalStyle = new InterfaceStyleChoice(this, "OriginalStyle", "#CSEInterface_StyleOriginal",
        m_uiStyleCombo, cse::ui::OriginalVGuiStyle);
    m_smokedStyle = new InterfaceStyleChoice(this, "SmokedStyle", "#CSEInterface_StyleSmokedTacticalGlass",
        m_uiStyleCombo, cse::ui::SmokedTacticalGlassVGuiStyle);
    m_radarSettings = new vgui2::Button(this, "RadarSettings", "#GameUI_HudRadarSettings");
    m_radarSettings->SetCommand("RadarSettings");
    m_radarSettings->AddActionSignalTarget(this);
    m_radarSettings->SetTabPosition(UIStyleTabPosition + 1);
    // The shared GameUI is also loaded by Half-Life, which has no advanced radar.
    m_radarSettings->SetEnabled(engine->pfnGetCvarPointer
        && engine->pfnGetCvarPointer(const_cast<char*>("hud_radar_mode")) != nullptr);
    m_saveFailed->SetVisible(false);
    OnResetData();
}

std::string COptionsSubInterface::GetSelectedLanguage() const {
    KeyValues* item = m_languageCombo->GetActiveItemUserData();
    return item ? item->GetString(LanguageItemKey, "") : "";
}

int COptionsSubInterface::GetSelectedUIStyle() const {
    KeyValues* item = m_uiStyleCombo->GetActiveItemUserData();
    return item ? item->GetInt(UIStyleItemKey, cse::ui::SmokedTacticalGlassVGuiStyle)
        : cse::ui::SmokedTacticalGlassVGuiStyle;
}

void COptionsSubInterface::OnApplyChanges() {
    const int selected = GetSelectedUIStyle();
    if (selected != m_originalUIStyle) {
        engine->Cvar_SetValue(const_cast<char*>(cse::ui::VGuiStyleCvar), static_cast<float>(selected));
        m_originalUIStyle = selected;
    }
}

void COptionsSubInterface::OnResetData() {
    const auto* localization = cse::localization::ControlsEngine();
    m_originalLanguage = localization ? localization->Preference() : cse::interface_language::ReadSavedPreference();
    m_languageRequest = cse::localization::InvalidRequest;
    vgui2::ISystem* system = vgui2::system();
    m_originalUIStyle = engine->pfnGetCvarFloat(const_cast<char*>(cse::ui::VGuiStyleCvar)) > 0.0f
        ? cse::ui::SmokedTacticalGlassVGuiStyle : cse::ui::OriginalVGuiStyle;
    for (const int itemId : m_uiStyleItemIds) {
        KeyValues* item = m_uiStyleCombo->GetItemUserData(itemId);
        if (item && item->GetInt(UIStyleItemKey) == m_originalUIStyle) {
            m_uiStyleCombo->ActivateItem(itemId);
            break;
        }
    }

    for (const int itemId : m_languageItemIds) {
        KeyValues* item = m_languageCombo->GetItemUserData(itemId);
        if (item && m_originalLanguage == item->GetString(LanguageItemKey, "")) {
            m_languageCombo->ActivateItem(itemId);
            break;
        }
    }
    m_saveFailed->SetVisible(false);
    m_launchOverride->SetVisible(system && system->CommandLineParamExists("-language"));
    InvalidateLayout(false, false);
}

void COptionsSubInterface::OnTextChanged() {
    m_saveFailed->SetVisible(false);
    // ComboBox reset notifications are queued, so compare stable item data at delivery time.
    if (GetSelectedLanguage() != m_originalLanguage || GetSelectedUIStyle() != m_originalUIStyle) {
        PostActionSignal(new KeyValues("ApplyButtonEnable"));
    }
}

bool COptionsSubInterface::HasLanguageChange() const {
    return GetSelectedLanguage() != m_originalLanguage;
}

bool COptionsSubInterface::ApplyLanguage() {
    if (IsLanguagePending()) {
        return true;
    }
    const std::string selected = GetSelectedLanguage();
    if (selected == m_originalLanguage) {
        return true;
    }
    const auto* localization = cse::localization::ControlsEngine();
    m_languageRequest = localization ? localization->RequestLanguage(selected.c_str(), true)
        : cse::localization::InvalidRequest;
    const bool accepted = IsLanguagePending();
    m_saveFailed->SetVisible(!accepted);
    InvalidateLayout(false, false);
    return accepted;
}

bool COptionsSubInterface::IsLanguagePending() const {
    return m_languageRequest != cse::localization::InvalidRequest;
}

bool COptionsSubInterface::CompleteLanguageRequest(bool& applied) {
    if (!IsLanguagePending()) {
        return false;
    }
    const auto* localization = cse::localization::ControlsEngine();
    cse::localization::RequestResult result{sizeof(result), cse::localization::RequestState::Failed, 0};
    const bool found = localization && localization->GetRequestResult(m_languageRequest, &result);
    if (found && result.state == cse::localization::RequestState::Pending) {
        return false;
    }
    m_languageRequest = cse::localization::InvalidRequest;
    applied = found && result.state == cse::localization::RequestState::Applied;
    if (applied) {
        m_originalLanguage = localization->Preference();
    }
    m_saveFailed->SetVisible(!applied);
    InvalidateLayout(false, false);
    return true;
}

void COptionsSubInterface::OnCommand(const char* command) {
    if (std::string(command) == "RadarSettings") {
        if (m_radarSettings->IsEnabled()) {
            auto* dialog = new CHudRadarDialog(this);
            dialog->DoModal();
        }
        return;
    }
    BaseClass::OnCommand(command);
}

void COptionsSubInterface::PerformLayout() {
    BaseClass::PerformLayout();
    const bool smoked = cse::ui::ControlsSmokedGlass();
    m_uiStyleCombo->SetVisible(!smoked);
    m_originalStyle->SetVisible(smoked);
    m_smokedStyle->SetVisible(smoked);
    if (smoked) {
        using vgui2::smoked::Pixels;
        const int inset = Pixels(12);
        const int gap = Pixels(12);
        const int width = std::max(0, GetWide() - inset - inset);
        const int rowHeight = Pixels(32);
        int contentWidth = 0;
        int contentHeight = 0;
        m_uiStyleLabel->GetContentSize(contentWidth, contentHeight);
        const int labelWidth = std::min(width * 2 / 5, std::max(width / 3, contentWidth + gap));
        const int fieldX = inset + labelWidth + gap;
        const int fieldWidth = std::max(0, width - labelWidth - gap);
        int top = inset;
        m_languageLabel->SetBounds(inset, top, labelWidth, rowHeight);
        m_languageCombo->SetBounds(fieldX, top, fieldWidth, rowHeight);
        top += rowHeight + gap;
        for (vgui2::Label* notice : {m_launchOverride, m_saveFailed}) {
            if (notice->IsVisible()) {
                notice->SetFgColor(vgui2::smoked::ColorOf(cse::ui::SecondaryText));
                top += LayoutNotice(notice, inset, top, width, Pixels(24)) + gap;
            }
        }
        m_uiStyleLabel->SetBounds(inset, top, labelWidth, rowHeight);
        // The hidden combo remains the single selection owner for both presentations.
        m_originalStyle->Button::SetSelected(GetSelectedUIStyle() == cse::ui::OriginalVGuiStyle);
        m_smokedStyle->Button::SetSelected(GetSelectedUIStyle() == cse::ui::SmokedTacticalGlassVGuiStyle);
        m_originalStyle->GetContentSize(contentWidth, contentHeight);
        m_originalStyle->SetFont(vgui2::smoked::TextFont(m_originalStyle,
            vgui2::scheme()->GetIScheme(GetScheme()), m_originalStyle->GetFont()));
        wchar_t originalText[256]{};
        m_originalStyle->GetText(originalText, 256);
        vgui2::surface()->GetTextSize(m_originalStyle->GetFont(), originalText, contentWidth, contentHeight);
        contentWidth += Pixels(cse::ui::RadioDiameter + cse::ui::RadioTextGap);
        const int originalWidth = std::min(fieldWidth, contentWidth + gap);
        m_originalStyle->SetBounds(fieldX, top, originalWidth, rowHeight);
        m_smokedStyle->GetContentSize(contentWidth, contentHeight);
        m_smokedStyle->SetFont(vgui2::smoked::TextFont(m_smokedStyle,
            vgui2::scheme()->GetIScheme(GetScheme()), m_smokedStyle->GetFont()));
        wchar_t smokedText[256]{};
        m_smokedStyle->GetText(smokedText, 256);
        vgui2::surface()->GetTextSize(m_smokedStyle->GetFont(), smokedText, contentWidth, contentHeight);
        contentWidth += Pixels(cse::ui::RadioDiameter + cse::ui::RadioTextGap);
        const bool stacked = originalWidth + gap + contentWidth > fieldWidth;
        m_smokedStyle->SetBounds(stacked ? fieldX : fieldX + originalWidth + gap,
            stacked ? top + rowHeight : top,
            stacked ? fieldWidth : std::max(0, fieldWidth - originalWidth - gap), rowHeight);
        const int radarTop = top + (stacked ? rowHeight * 2 : rowHeight) + gap;
        m_radarSettings->SetBounds(fieldX, radarTop, fieldWidth, rowHeight);
        return;
    }
    const int inset = GetProportionalScaledValue(ContentInset);
    const int width = std::max(0, GetWide() - inset - inset);
    const int labelHeight = GetProportionalScaledValue(LanguageLabelHeight);
    const int comboHeight = GetProportionalScaledValue(LanguageComboHeight);
    const int controlGap = GetProportionalScaledValue(ControlGap);
    const int noticeGap = GetProportionalScaledValue(NoticeGap);
    const int minimumNoticeHeight = GetProportionalScaledValue(MinimumNoticeHeight);
    m_languageLabel->SetBounds(inset, inset, width, labelHeight);
    int top = inset + labelHeight + controlGap;
    m_languageCombo->SetBounds(inset, top, width, comboHeight);
    top += comboHeight + noticeGap;
    if (m_launchOverride->IsVisible()) {
        top += LayoutNotice(m_launchOverride, inset, top, width, minimumNoticeHeight) + noticeGap;
    }
    if (m_saveFailed->IsVisible()) {
        top += LayoutNotice(m_saveFailed, inset, top, width, minimumNoticeHeight) + noticeGap;
    }
    m_uiStyleLabel->SetBounds(inset, top, width, labelHeight);
    top += labelHeight + controlGap;
    m_uiStyleCombo->SetBounds(inset, top, width, comboHeight);
    top += comboHeight + noticeGap;
    m_radarSettings->SetBounds(inset, top, width, comboHeight);
}
