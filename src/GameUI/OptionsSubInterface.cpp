#include "OptionsSubInterface.h"
#include "GameUI_Interface.h"
#include "wrect.h"
#include "cl_dll.h"

#include "ComboBox.h"
#include "Label.h"
#include "TextImage.h"
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
    KeyValues* extended = new KeyValues("UIStyle", UIStyleItemKey, cse::ui::ExtendedVGuiStyle);
    m_uiStyleItemIds.push_back(m_uiStyleCombo->AddItem("#CSEInterface_StyleExtended", extended));
    extended->deleteThis();
    m_saveFailed->SetVisible(false);
    OnResetData();
}

std::string COptionsSubInterface::GetSelectedLanguage() const {
    KeyValues* item = m_languageCombo->GetActiveItemUserData();
    return item ? item->GetString(LanguageItemKey, "") : "";
}

int COptionsSubInterface::GetSelectedUIStyle() const {
    KeyValues* item = m_uiStyleCombo->GetActiveItemUserData();
    return item ? item->GetInt(UIStyleItemKey, cse::ui::ExtendedVGuiStyle)
        : cse::ui::ExtendedVGuiStyle;
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
        ? cse::ui::ExtendedVGuiStyle : cse::ui::OriginalVGuiStyle;
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

void COptionsSubInterface::PerformLayout() {
    BaseClass::PerformLayout();
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
}
