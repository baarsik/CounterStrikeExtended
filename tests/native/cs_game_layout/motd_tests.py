#!/usr/bin/env python3
"""Exercise the actual MOTD owner against its resource title and the shared theme visibility helper."""
from pathlib import Path
import subprocess

root = Path(__file__).resolve().parents[3]
out = root / 'build/smoked-game-layouts-20261115/native/motd'
out.mkdir(parents=True, exist_ok=True)


def function(source, signature):
    start = source.index(signature)
    opening = source.index('{', start)
    depth = 1
    end = opening + 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end] + '\n'


source = (root / 'src/cstrike/vgui2/game_controls/clientmotd.cpp').read_text(encoding='latin1')
smoked = (root / 'src/vgui2/controls/SmokedGlass.h').read_text()
has_layout = 'void CClientMOTD::PerformLayout(' in source
harness = r'''
#include <algorithm>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <cwchar>
#include <initializer_list>
#include <string>
#include <unordered_map>
#include <vector>
using FileHandle_t = void*;
namespace vgui2 {
struct Panel {
    std::string name;
    std::vector<Panel*> children;
    bool visible = true;
    Panel(Panel* parent, const char* panelName) : name(panelName) {
        if (parent) {
            parent->children.push_back(this);
        }
    }
    virtual ~Panel() {
        for (Panel* child : children) {
            delete child;
        }
    }
    bool IsVisible() const { return visible; }
    void SetVisible(bool value) { visible = value; }
    virtual void PerformLayout() {}
};
struct Label : Panel {
    std::wstring text;
    int x = 0;
    int y = 0;
    int width = 64;
    int height = 24;
    Label(Panel* parent, const char* name, const char*) : Panel(parent, name) {}
    void SetText(const wchar_t* value) { text = value; }
};
struct HTML : Panel {
    std::string url;
    HTML(Panel* parent, const char* name) : Panel(parent, name) {}
    void OpenURL(const char* value, const char*, bool) { url = value; }
};
struct Frame : Panel {
    std::wstring title;
    bool drawTitleBar = true;
    Frame(Panel* parent, const char* name, bool, bool) : Panel(parent, name) {}
    void SetTitle(const char*, bool) { title.clear(); }
    void SetTitle(const wchar_t* value, bool) { title = value; }
    void SetScheme(const char*) {}
    void SetMoveable(bool) {}
    void SetSizeable(bool) {}
    void SetProportional(bool) {}
    void InvalidateLayout(bool, bool) {}
    void Activate() { SetVisible(true); }
    void LoadControlSettings(const char* name, const char*) {
        assert(std::strcmp(name, "Resource/UI/MOTD.res") == 0);
        drawTitleBar = false;
        // BuildGroup applies a resource to an existing named child, creating it only when absent.
        // These are the stock MOTD.res title's text, geometry and visibility.
        Label* label = nullptr;
        for (Panel* child : children) {
            if (child->name == "serverName") {
                label = dynamic_cast<Label*>(child);
                break;
            }
        }
        if (!label) {
            label = new Label(this, "serverName", "");
        }
        label->text = L"Server Name Goes Here +++ :p";
        label->x = 0;
        label->y = 22;
        label->width = 500;
        label->height = 48;
        label->visible = true;
    }
};
struct Localize {
    void ConvertANSIToUnicode(const char* input, wchar_t* output, int bytes) {
        const std::size_t capacity = static_cast<std::size_t>(bytes) / sizeof(wchar_t);
        std::size_t index = 0;
        while (index + 1 < capacity && input[index]) {
            output[index] = static_cast<unsigned char>(input[index]);
            ++index;
        }
        output[index] = L'\0';
    }
    void ConvertUnicodeToANSI(const wchar_t* input, char* output, int capacity) {
        std::size_t index = 0;
        while (index + 1 < static_cast<std::size_t>(capacity) && input[index]) {
            output[index] = static_cast<char>(input[index]);
            ++index;
        }
        output[index] = '\0';
    }
} localizer;
Localize* localize() { return &localizer; }
struct FileSystem {
    bool FileExists(const char*) { return false; }
    void RemoveFile(const char*, const char*) { assert(false); }
    FileHandle_t Open(const char*, const char*, const char*) { assert(false); return nullptr; }
    void Write(const void*, int, FileHandle_t) { assert(false); }
    void Close(FileHandle_t) { assert(false); }
    void GetLocalPath(const char*, char*, int) { assert(false); }
} files;
FileSystem* filesystem() { return &files; }
}
namespace cse::ui {
bool glass = false;
bool ControlsSmokedGlass() { return glass; }
std::unordered_map<vgui2::Panel*, bool>& ControlsDecorations() {
    static std::unordered_map<vgui2::Panel*, bool> visibility;
    return visibility;
}
}
namespace vgui2::smoked {
'''
harness += function(smoked, 'inline void Decorations(')
harness += r'''
}
struct ClientMOTDHTML : vgui2::HTML {
    using vgui2::HTML::HTML;
};
struct GameUIFuncs {
    int GetVGUI2KeyCodeForBind(const char* bind) {
        assert(std::strcmp(bind, "showscores") == 0);
        return 17;
    }
} bindings;
GameUIFuncs* gameuifuncs = &bindings;
struct CClientMOTD : vgui2::Frame {
    vgui2::HTML* m_pMessage = nullptr;
    vgui2::Label* m_pServerName = nullptr;
    bool m_bFileWritten = false;
    char m_szTempFileName[4096]{};
    int m_iScoreBoardKey = 0;
    CClientMOTD(vgui2::Panel*);
    ~CClientMOTD() override;
    bool IsURL(const char*);
    void Activate(const char*, const char*);
    void Activate(const wchar_t*, const wchar_t*);
'''
if has_layout:
    harness += '    void PerformLayout() override;\n'
harness += '};\n'
for signature in ['CClientMOTD::CClientMOTD(', 'CClientMOTD::~CClientMOTD()',
                  'bool CClientMOTD::IsURL(', 'void CClientMOTD::Activate( const char*',
                  'void CClientMOTD::Activate( const wchar_t*']:
    harness += function(source, signature)
if has_layout:
    harness += function(source, 'void CClientMOTD::PerformLayout(')
harness += r'''
void CheckTitle(CClientMOTD& motd, const wchar_t* title) {
    assert(motd.title == title);
    int titleLabels = 0;
    int visibleTitles = (cse::ui::glass || motd.drawTitleBar) && !motd.title.empty() ? 1 : 0;
    for (vgui2::Panel* child : motd.children) {
        auto* label = dynamic_cast<vgui2::Label*>(child);
        if (label && label->name == "serverName") {
            ++titleLabels;
            assert(label->text == title);
            visibleTitles += label->IsVisible() && !label->text.empty() ? 1 : 0;
            // Original's resource title must retain its stock bounds through theme switches.
            assert(label->x == 0 && label->y == 22 && label->width == 500 && label->height == 48);
            assert(label->IsVisible() == !cse::ui::glass);
        }
    }
    assert(titleLabels == 1);
    assert(visibleTitles == 1);
}
int main() {
    for (bool initialGlass : {false, true}) {
        cse::ui::glass = initialGlass;
        {
            CClientMOTD motd(nullptr);
            motd.Activate("Half-Life", "http://example.test/motd");
            motd.PerformLayout();
            CheckTitle(motd, L"Half-Life");
            assert(motd.m_pMessage->url == "http://example.test/motd");
            for (bool glass : {true, false, true, false}) {
                cse::ui::glass = glass;
                motd.PerformLayout();
                CheckTitle(motd, L"Half-Life");
            }
            motd.Activate("#LiteralServer", "http://example.test/other");
            motd.PerformLayout();
            CheckTitle(motd, L"#LiteralServer");
            assert(motd.m_pMessage->url == "http://example.test/other");
            motd.Activate(L"Wide server title", L"http://example.test/wide");
            motd.PerformLayout();
            CheckTitle(motd, L"Wide server title");
            assert(motd.m_pMessage->url == "http://example.test/wide");
        }
        // Panel destruction owns the real helper's external visibility entry cleanup in production.
        cse::ui::ControlsDecorations().clear();
    }
    std::puts("MOTD: one title, resource ownership, ANSI/wide activation and Original/theme restoration passed");
}
'''
(out / 'motd.cpp').write_text(harness)
subprocess.run(['c++', '-std=c++17', '-Wall', '-Wextra', '-Werror', '-fsanitize=address,undefined',
                str(out / 'motd.cpp'), '-o', str(out / 'motd')], check=True)
subprocess.run([str(out / 'motd')], check=True)
