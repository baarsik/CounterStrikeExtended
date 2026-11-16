#pragma once

#include "PropertyPage.h"
#include "localization_service.h"

#include <string>
#include <vector>

namespace vgui2 {
class Button;
class ComboBox;
class Label;
class RadioButton;
}

class COptionsSubInterface : public vgui2::PropertyPage {
    DECLARE_CLASS_SIMPLE_NAMED(COptionsSubInterface, vgui2::PropertyPage, "PropertyPage");

public:
    explicit COptionsSubInterface(vgui2::Panel* parent);

    bool ApplyLanguage();
    bool HasLanguageChange() const;
    bool IsLanguagePending() const;
    bool CompleteLanguageRequest(bool& applied);

protected:
    void OnResetData() override;
    void OnApplyChanges() override;
    void PerformLayout() override;
    MESSAGE_FUNC(OnTextChanged, "TextChanged");
    void OnCommand(const char* command) override;

private:
    std::string GetSelectedLanguage() const;
    int GetSelectedUIStyle() const;

    vgui2::Button* m_radarSettings;
    vgui2::ComboBox* m_languageCombo;
    vgui2::Label* m_languageLabel;
    vgui2::Label* m_saveFailed;
    vgui2::Label* m_launchOverride;
    vgui2::ComboBox* m_uiStyleCombo;
    vgui2::Label* m_uiStyleLabel;
    vgui2::RadioButton* m_originalStyle;
    vgui2::RadioButton* m_smokedStyle;
    int m_originalUIStyle = 0;
    std::vector<int> m_languageItemIds;
    std::vector<int> m_uiStyleItemIds;
    std::string m_originalLanguage;
    std::uint64_t m_languageRequest = cse::localization::InvalidRequest;
};
