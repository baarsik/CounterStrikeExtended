#include "OptionsSubInterface.h"

#include "ComboBox.h"
#include "Label.h"
#include "TextImage.h"
#include "interface_language.h"
#include "interface_language_preference.h"
#include "vgui/ISystem.h"
#include "vgui/KeyValues.h"
#include "vgui_controls/Controls.h"

#include <algorithm>

namespace {

constexpr char LanguageItemKey[] = "language";
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

    m_restartRequired = new InterfaceNotice(this, "RestartRequired", "#CSEInterface_RestartRequired");
    m_saveFailed = new InterfaceNotice(this, "SaveFailed", "#CSEInterface_SaveFailed");
    m_launchOverride = new InterfaceNotice(this, "LaunchOverride", "#CSEInterface_LaunchOverride");
    m_saveFailed->SetVisible(false);
    OnResetData();
}

std::string COptionsSubInterface::GetSelectedLanguage() const {
    KeyValues* item = m_languageCombo->GetActiveItemUserData();
    return item ? item->GetString(LanguageItemKey, "") : "";
}

void COptionsSubInterface::OnResetData() {
    m_originalLanguage = cse::interface_language::ReadSavedPreference();
    vgui2::ISystem* system = vgui2::system();

    for (const int itemId : m_languageItemIds) {
        KeyValues* item = m_languageCombo->GetItemUserData(itemId);
        if (item && m_originalLanguage == item->GetString(LanguageItemKey, "")) {
            m_languageCombo->ActivateItem(itemId);
            break;
        }
    }
    m_restartRequired->SetVisible(true);
    m_saveFailed->SetVisible(false);
    m_launchOverride->SetVisible(system && system->CommandLineParamExists("-language"));
    InvalidateLayout(false, false);
}

void COptionsSubInterface::OnTextChanged() {
    m_saveFailed->SetVisible(false);
    // ComboBox reset notifications are queued, so compare stable item data at delivery time.
    if (GetSelectedLanguage() != m_originalLanguage) {
        PostActionSignal(new KeyValues("ApplyButtonEnable"));
    }
}

bool COptionsSubInterface::ApplyLanguage() {
    const std::string selected = GetSelectedLanguage();
    if (selected == m_originalLanguage) {
        return true;
    }
    vgui2::ISystem* system = vgui2::system();
    if (!system || !system->SetRegistryString(cse::interface_language::PreferenceKey, selected.c_str())) {
        m_saveFailed->SetVisible(true);
        InvalidateLayout(false, false);
        return false;
    }
    m_originalLanguage = selected;
    m_saveFailed->SetVisible(false);
    return true;
}

void COptionsSubInterface::OnApplyChanges() {
    ApplyLanguage();
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
    top += LayoutNotice(m_restartRequired, inset, top, width, minimumNoticeHeight) + noticeGap;
    top += LayoutNotice(m_launchOverride, inset, top, width, minimumNoticeHeight) + noticeGap;
    LayoutNotice(m_saveFailed, inset, top, width, minimumNoticeHeight);
}
