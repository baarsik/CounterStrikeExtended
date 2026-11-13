#include "hud.h"
#include "cl_util.h"
#include "vgui_TeamFortressViewport.h"
#include "vgui_language.h"
#include <VGUI_TextImage.h>
#include "../common/resource_text.h"
#include <algorithm>
#include <vector>

namespace {
std::vector<LanguageView*>& Views() {
    static std::vector<LanguageView*> views;
    return views;
}

bool ReadLanguageFile(const std::string& path, std::string& text) {
    int length = 0;
    char* loaded = reinterpret_cast<char*>(gEngfuncs.COM_LoadFile(const_cast<char*>(path.c_str()), 5, &length));
    const bool success = loaded && length >= 0
        && cse::resource_text::Decode(loaded, static_cast<std::size_t>(length), text);
    if (loaded) {
        gEngfuncs.COM_FreeFile(loaded);
    }
    return success;
}
}

LanguageView::LanguageView() {
    WatchLanguage();
}

void LanguageView::WatchLanguage() {
    if (!m_watching) {
        Views().push_back(this);
        m_watching = true;
    }
}

LanguageView::~LanguageView() {
    StopWatchingLanguage();
}

void LanguageView::StopWatchingLanguage() {
    auto& views = Views();
    views.erase(std::remove(views.begin(), views.end(), this), views.end());
    m_watching = false;
}

void ResetLanguageViews() {
    while (!Views().empty()) {
        Views().back()->StopWatchingLanguage();
    }
}

void RefreshLanguageViews() {
    // Refresh never constructs/destroys a view or invokes an action signal.
    for (LanguageView* view : Views()) {
        view->RefreshLanguage();
    }
}

std::string LocalizeUi(const char* token, const char* fallback) {
    const auto* message = TextMessageGet(token && token[0] == '#' ? token + 1 : token);
    return message && message->pMessage ? message->pMessage : (fallback ? fallback : "");
}

std::string ResolveCommandLabel(const std::string& file, const std::string& command, int occurrence,
    const std::string& fallback) {
    std::string label = fallback;
    std::string text;
    if (!file.empty() && ReadLanguageFile(file, text) && !text.empty()) {
        char token[1024];
        char* cursor = &text[0];
        std::vector<std::string> tokens;
        while (cursor) {
            cursor = gEngfuncs.COM_ParseFile(cursor, token);
            if (!token[0]) {
                break;
            }
            tokens.emplace_back(token);
        }
        int found = 0;
        for (std::size_t index = 0; index < tokens.size();) {
            if (tokens[index] == "}") {
                ++index;
                continue;
            }
            const std::string prefix = tokens[index];
            if (prefix == "CUSTOM" || prefix == "TOGGLE" || prefix.compare(0, 4, "TEAM") == 0) {
                ++index;
            } else if (prefix == "MAP") {
                index += 2;
            }
            constexpr std::size_t EntryTokens = 3; // Bound key, displayed label, action/submenu.
            if (index + EntryTokens > tokens.size()) {
                break;
            }
            if (tokens[index + 2] == command && found++ == occurrence) {
                label = tokens[index + 1];
                break;
            }
            index += EntryTokens;
        }
    }
    char localized[1024];
    CHudTextMessage::LocaliseTextString(label.c_str(), localized, sizeof(localized));
    return localized;
}

void LocalizedLabel::BindLanguage(const char* token, const char* fontScheme) {
    m_token = token ? token : "";
    m_fontScheme = fontScheme ? fontScheme : "";
    RefreshLanguage();
}

void LocalizedLabel::RefreshLanguage() {
    if (!m_fontScheme.empty() && gViewPort) {
        auto* schemes = gViewPort->GetSchemeManager();
        setFont(schemes->getFont(schemes->getSchemeHandle(m_fontScheme.c_str())));
    }
    if (!m_token.empty()) {
        const std::string text = LocalizeUi(m_token.c_str(), m_token.c_str());
        vgui::Label::setText(static_cast<int>(text.size()) + 1, text.c_str());
    }
    invalidateLayout(false);
}

void LocalizedTextPanel::BindLanguage(const char* token, const char* fontScheme) {
    m_token = token ? token : "";
    m_fontScheme = fontScheme ? fontScheme : "";
    RefreshLanguage();
}

void LocalizedTextPanel::BindFile(const char* path, const char* fallback) {
    m_file = path ? path : "";
    m_fallback = fallback ? fallback : "";
    RefreshLanguage();
}

void LocalizedTextPanel::SetLanguageScroll(vgui::ScrollPanel* scroll) {
    m_scroll = scroll;
}

void LocalizedTextPanel::RefreshLanguage() {
    if (!m_fontScheme.empty() && gViewPort) {
        auto* schemes = gViewPort->GetSchemeManager();
        setFont(schemes->getFont(schemes->getSchemeHandle(m_fontScheme.c_str())));
    }
    if (!m_file.empty()) {
        std::string text;
        if (!ReadLanguageFile(m_file, text)) {
            text = LocalizeUi(m_fallback.c_str(), m_fallback.c_str());
        }
        setText(text.c_str());
    } else if (!m_token.empty()) {
        setText(LocalizeUi(m_token.c_str(), m_token.c_str()).c_str());
    }
    if (m_scroll) {
        int horizontal = 0;
        int vertical = 0;
        m_scroll->getScrollValue(horizontal, vertical);
        getTextImage()->setSize(m_scroll->getClientClip()->getWide(), m_scroll->getClientClip()->getTall());
        int wide = 0;
        int tall = 0;
        getTextImage()->getTextSizeWrapped(wide, tall);
        setSize(wide, tall);
        m_scroll->validate();
        m_scroll->setScrollValue(horizontal, vertical);
    }
    invalidateLayout(false);
}
