#ifndef CSE_HL_VGUI_LANGUAGE_H
#define CSE_HL_VGUI_LANGUAGE_H

#include <VGUI_Label.h>
#include <VGUI_TextPanel.h>
#include <VGUI_ScrollPanel.h>
#include <string>

class LanguageView {
public:
    LanguageView();
    virtual ~LanguageView();
    virtual void RefreshLanguage() = 0;
    void WatchLanguage();
    void StopWatchingLanguage();
private:
    bool m_watching = false;
};

void RefreshLanguageViews();
void ResetLanguageViews();
std::string LocalizeUi(const char* token, const char* fallback);
std::string ResolveCommandLabel(const std::string& file, const std::string& command, int occurrence,
    const std::string& fallback);

class LocalizedLabel : public vgui::Label, public LanguageView {
public:
    using vgui::Label::Label;
    void BindLanguage(const char* token, const char* fontScheme);
    void RefreshLanguage() override;
private:
    std::string m_token;
    std::string m_fontScheme;
};

class LocalizedTextPanel : public vgui::TextPanel, public LanguageView {
public:
    using vgui::TextPanel::TextPanel;
    void BindLanguage(const char* token, const char* fontScheme);
    void BindFile(const char* path, const char* fallback);
    void SetLanguageScroll(vgui::ScrollPanel* scroll);
    void RefreshLanguage() override;
private:
    std::string m_token;
    std::string m_fontScheme;
    std::string m_file;
    std::string m_fallback;
    vgui::ScrollPanel* m_scroll = nullptr;
};

#endif
