#!/usr/bin/env python3
"""Compile actual GameUI owner methods against observable state/service doubles."""
from pathlib import Path
import argparse
import re
import subprocess

ROOT = Path(__file__).resolve().parents[3]
parser = argparse.ArgumentParser()
parser.add_argument('--baseline', action='store_true', help='Run the task stale-cache regression against the pre-localization commit')
args = parser.parse_args()
OUT = ROOT / 'build/live-localization/gameui-owner-tests'
OUT.mkdir(parents=True, exist_ok=True)


def source(name):
    if args.baseline:
        return subprocess.check_output(['git', 'show', '538661e0ef6e5d0384bff9f85d0aa451cb93c09e:src/GameUI/' + name], cwd=ROOT).decode('cp1252')
    return (ROOT / 'src/GameUI' / name).read_text(encoding='cp1252')


def function(text, signature):
    start = text.index(signature)
    opening = text.index('{', start)
    depth = 1
    end = opening + 1
    while depth:
        depth += (text[end] == '{') - (text[end] == '}')
        end += 1
    return text[start:end] + '\n'


def run(name, code):
    path = OUT / (name + '.cpp')
    path.write_text(code)
    binary = OUT / name
    subprocess.run(['c++', '-std=c++17', '-Wall', '-Wextra', '-Werror', '-Wno-deprecated-declarations', '-fsanitize=address,undefined',
                    '-g', str(path), '-o', str(binary)], check=True)
    result = subprocess.run([str(binary)], capture_output=True, text=True)
    (OUT / (name + '.log')).write_text(result.stdout + result.stderr)
    if args.baseline:
        assert result.returncode != 0, 'Expected stale-cache regression to fail on HEAD'
        print(name + ': RED, stale task cache reproduced')
    else:
        if result.returncode:
            print(result.stdout + result.stderr)
        result.check_returncode()
        print(name + ': PASS')


def tasks():
    header = source('CareerGameTasks.h')
    declaration = header[header.index('class CCareerUITask'):]
    declaration = declaration.replace(' : public ICareerTask', '').replace(' override', '')
    body = re.sub(r'^#include.*\n', '', source('CareerGameTasks.cpp'), flags=re.M)
    prelude = r'''
#include <algorithm>
#include <cassert>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cwchar>
#include <map>
#include <string>
#include <vector>
#define _snwprintf swprintf
std::string parsed;
bool SharedTokenWaiting(const char* p) { while (*p == ' ') ++p; return *p; }
const char* SharedParse(const char* p) {
    while (*p == ' ') ++p;
    parsed.clear();
    while (*p && *p != ' ') parsed += *p++;
    return p;
}
const char* SharedGetToken() { return parsed.c_str(); }
struct Localize {
    std::map<std::string, std::wstring> values;
    wchar_t* Find(const char* token) {
        const auto found = values.find(token);
        return found == values.end() ? nullptr : found->second.data();
    }
    void ConvertANSIToUnicode(const char* text, wchar_t* output, int bytes) {
        const int cells = bytes / sizeof(wchar_t);
        int index = 0;
        for (; text[index] && index < cells - 1; ++index) output[index] = text[index];
        output[index] = 0;
    }
    void ConstructString(wchar_t* output, int bytes, wchar_t* format, int count, ...) {
        std::vector<const wchar_t*> params;
        va_list args;
        va_start(args, count);
        for (int index = 0; index < count; ++index) params.push_back(va_arg(args, const wchar_t*));
        va_end(args);
        std::wstring value;
        for (const wchar_t* p = format ? format : L""; *p; ++p) {
            if (p[0] == '%' && p[1] == 's' && p[2] >= '1' && p[2] <= '9' && p[2] - '1' < count) {
                const wchar_t* parameter = params[p[2] - '1'];
                value += parameter ? parameter : L"";
                p += 2;
            } else value += *p;
        }
        const size_t cells = static_cast<size_t>(bytes) / sizeof(wchar_t);
        std::wcsncpy(output, value.c_str(), cells - 1);
        output[cells - 1] = 0;
    }
} provider;
namespace vgui2 { Localize* localize() { return &provider; } }
void Language(const wchar_t* weapon, const wchar_t* wrapper, const wchar_t* completion) {
    provider.values = {
        {"#Career_Weapon_knife", weapon}, {"#Career_Weapon_hegren", weapon},
        {"#Career_Weapon_grenade", weapon},
        {"#Career_Task_kill_multi", L"%s2 targets with %s1"},
        {"#Career_Task_kill", L"one target with %s1"},
        {"#Career_Task_survive", wrapper}, {"#Career_Task_inarow", wrapper},
        {"#Career_Task_Done", completion}, {"#Career_Task_winfast", L"win fast"}
    };
}
'''
    hook = '' if not args.baseline else 'void Refresh(CCareerUITask&) {}\n'
    if not args.baseline:
        hook = 'void Refresh(CCareerUITask& task) { task.RefreshLanguagePresentation(); }\n'
    harness = r'''
int main() {
    Language(L"knife", L"survive %s1", L"done: %s1");
    CCareerUITask task("kill knife 3 survive");
    assert(std::wstring(task.GetLocalizedTaskName()) == L"survive 3 targets with knife");
    task.Set(true);
    task.StartRound();
    task.SetPartial(2);
    Language(L"nóż", L"przetrwaj %s1", L"gotowe: %s1");
    Refresh(task);
    assert(std::wstring(task.GetLocalizedTaskName()) == L"przetrwaj 3 targets with nóż");
    assert(std::wstring(task.GetLocalizedTaskNameWithCompletion()) == L"gotowe: przetrwaj 3 targets with nóż");
    assert(task.IsComplete() && task.IsCompletedThisMatch() && !task.IsCompletedThisRound());
    assert(task.GetPartial() == 2 && task.GetRepeat() == 3 && task.MustSurvive() && !task.InARow());
    assert(std::strcmp(task.GetTaskName(), "kill") == 0 && std::strcmp(task.GetWeaponName(), "knife") == 0);
    CCareerUITask consecutive("kill grenade 2 inarow");
    assert(consecutive.InARow() && !consecutive.MustSurvive());
    assert(std::strcmp(consecutive.GetWeaponName(), "hegren") == 0);
    consecutive.SetPartial(1);
    for (int index = 0; index < 80; ++index) {
        Language(index % 2 ? L"ナイフ" : L"knife", L"%s1 / 生存", L"完了 %s1");
        Refresh(task);
        Refresh(consecutive);
        assert(task.GetPartial() == 2 && task.IsComplete());
        assert(consecutive.GetPartial() == 1 && !consecutive.IsComplete() && consecutive.InARow());
    }
    const std::wstring longWeapon(400, L'界');
    Language(longWeapon.c_str(), L"%s1", L"%s1 完了");
    Refresh(task);
    assert(std::wstring(task.GetLocalizedTaskName()).find(longWeapon) != std::wstring::npos);
    wchar_t reordered[64] = {};
    ConstructStringWithReordering(reordered, sizeof(reordered), (wchar_t*)L"%s2 / %s1 / %s2", 2, L"one", L"two");
    assert(std::wstring(reordered) == L"two / one / two");
    wchar_t bounded[4] = {};
    ConstructStringWithReordering(bounded, sizeof(bounded), (wchar_t*)L"%s1", 1, L"abcdef");
    assert(std::wstring(bounded) == L"abc");
    ConstructStringWithReordering(bounded, sizeof(bounded), (wchar_t*)L"%s", 0);
    assert(std::wstring(bounded) == L"%s");
    CCareerUITask fast("winfast");
    Refresh(fast);
    assert(fast.IsMaxRoundTime());
}
'''
    run('tasks-red' if args.baseline else 'tasks', prelude + declaration + body + hook + harness)


def keyboard():
    text = source('OptionsSubKeyboard.cpp')
    method = function(text, 'void COptionsSubKeyboard::ParseActionDescriptions(')
    harness = r'''
#include <cassert>
#include <cstring>
#include <cstdio>
#include <map>
#include <string>
#include <vector>
#include <strings.h>
#define stricmp strcasecmp
constexpr int KEYBIND_PATH_MAX = 128;
struct KeyValues {
    std::map<std::string, std::string> values;
    explicit KeyValues(const char*) {}
    void SetString(const char* name, const char* value) { values[name] = value; }
    void deleteThis() { delete this; }
};
struct List {
    std::vector<KeyValues> items;
    std::wstring section;
    int selected = 2;
    int scroll = 117;
    bool capturing = true;
    int layout = 0;
    int GetItemCount() { return static_cast<int>(items.size()); }
    int GetItemIDFromRow(int row) { return row; }
    KeyValues* GetItemData(int id) { return &items.at(id); }
    int GetColumnWidthBySection(int, int) { return 226; }
    void ModifyColumn(int, const char*, wchar_t* text, int) { section = text; }
    void InvalidateItem(int) { ++layout; }
    void AddSection(int, const char*, void*) { assert(false); }
    void AddColumnToSection(int, const char*, const char*, int, int, int) { assert(false); }
    void AddItem(int, KeyValues*) { assert(false); }
};
struct File { std::string input; size_t Size(void*) { return input.size(); }
    void* Open(const char*, const char*, void*) { return this; }
    void Read(void* dest, int size, void*) { std::memcpy(dest, input.data(), size); }
    void Close(void*) {}
} file;
using FileHandle_t = void*;
struct CUtlBuffer { std::vector<char> data; CUtlBuffer(int, int count, bool): data(count) {} void* Base() {return data.data();} };
struct Localize { void ConvertANSIToUnicode(const char* input, wchar_t* output, int) {
    while (*input) *output++ = static_cast<unsigned char>(*input++);
    *output = 0;
}} localizer;
namespace vgui2 { struct SectionedListPanel { enum { COLUMN_BRIGHT = 1 }; };
File* filesystem() { return &file; } Localize* localize() { return &localizer; } }
struct Engine { char* COM_ParseFile(char* input, char* output) {
    while (*input == ' ' || *input == '\n') ++input;
    const bool quoted = *input == '"';
    if (quoted) ++input;
    while (*input && (quoted ? *input != '"' : *input != ' ' && *input != '\n')) *output++ = *input++;
    *output = 0;
    if (quoted && *input) ++input;
    return input;
}} engineService;
Engine* engine = &engineService;
char token[512];
struct COptionsSubKeyboard {
    List* m_pKeyBindList;
    std::string savedBindings = "snapshot", pendingUnbinds = "MOUSE1";
    int GetProportionalScaledValue(int value) { return value; }
    KeyValues* GetItemForBinding(const char* binding) {
        for (auto& item : m_pKeyBindList->items) if (item.values["Binding"] == binding) return &item;
        return nullptr;
    }
    void ParseActionDescriptions(bool presentationOnly);
};
'''
    main = r'''
int main() {
    List list;
    KeyValues attack("Item");
    attack.SetString("Binding", "+attack"); attack.SetString("Action", "Fire");
    attack.SetString("Key", "q"); attack.SetString("AltKey", "MOUSE1");
    attack.SetString("Code", "Q"); attack.SetString("AltCode", "MOUSE1");
    list.items.push_back(attack);
    COptionsSubKeyboard page{&list};
    file.input = "blank \"Combat actions\"\n+attack \"Shoot updated\"\n+new \"Unowned action\"\n";
    page.ParseActionDescriptions(true);
    assert(list.items.size() == 1 && list.items[0].values["Action"] == "Shoot updated");
    assert(list.items[0].values["Key"] == "q" && list.items[0].values["AltKey"] == "MOUSE1");
    assert(list.items[0].values["Code"] == "Q" && list.items[0].values["AltCode"] == "MOUSE1");
    assert(list.section == L"Combat actions");
    assert(list.selected == 2 && list.scroll == 117 && list.capturing);
    assert(page.savedBindings == "snapshot" && page.pendingUnbinds == "MOUSE1");
    assert(list.layout == 1);
}
'''
    # The real filesystem returns an int, as does CUtlBuffer's constructor.
    harness = harness.replace('size_t Size(void*) { return input.size(); }', 'int Size(void*) { return static_cast<int>(input.size()); }')
    run('keyboard', harness + method + main)


def browser():
    text = source('native/ServerBrowserDialog.cpp')
    body = function(text, 'void ServerBrowserDialog::RefreshLanguagePresentation()')
    harness = r'''
#include <cassert>
#include <string>
#include <vector>
struct Detail { int refreshed = 0; void RefreshLanguagePresentation() { ++refreshed; } };
struct Weak { Detail* panel; Detail* Get() { return panel; } };
struct ServerBrowserDialog {
    std::vector<Weak> detailsDialogs_;
    Weak addServerDialog_;
    std::string shownStatus_ = "#sorting";
    bool viewportDirty_ = false;
    std::string query = "active generation", input = "#typed map", password = "secret";
    int selected = 17, offset = 400, generation = 9, callbacks = 3, filters = 7;
    int summaries = 0;
    void UpdateFilterSummary() { ++summaries; }
    void RefreshLanguagePresentation();
};
'''
    main = r'''
int main() {
    Detail details;
    Detail add;
    ServerBrowserDialog dialog{{{&details}, {nullptr}}, {&add}};
    dialog.RefreshLanguagePresentation();
    assert(dialog.viewportDirty_);
    assert(dialog.summaries == 1 && details.refreshed == 1 && add.refreshed == 1);
    assert(dialog.query == "active generation" && dialog.input == "#typed map" && dialog.password == "secret");
    assert(dialog.selected == 17 && dialog.offset == 400 && dialog.generation == 9);
    assert(dialog.callbacks == 3 && dialog.filters == 7);
}
'''
    run('browser', harness + body + main)


tasks()
if not args.baseline:
    keyboard()
    browser()
