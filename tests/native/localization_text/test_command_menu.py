#!/usr/bin/env python3
"""Check the production combo-menu reader and engine byte parser with Korean text."""
import argparse
import importlib.util
import json
from pathlib import Path
import re
import subprocess

parser = argparse.ArgumentParser()
parser.add_argument('--root', type=Path, required=True)
parser.add_argument('--evidence', type=Path, required=True)
parser.add_argument('--source', type=Path, help='Optional pinned CreateCommandMenu.cpp for comparison')
args = parser.parse_args()
root = args.root.resolve()
args.evidence.mkdir(parents=True, exist_ok=False)
spec = importlib.util.spec_from_file_location('extract', root / 'tests/native/network_platform/extract_functions.py')
extract = importlib.util.module_from_spec(spec)
spec.loader.exec_module(extract)
owner = args.source or root / 'src/cstrike/vgui2/game_controls/CreateCommandMenu.cpp'
menu = extract.extract(owner.read_text(encoding='cp1252'), 'CCreateCommandComboMenu::CreateCommandMenu')
parse = extract.extract((root / 'src/engine/common.cpp').read_text(encoding='cp1252'), 'COM_Parse')
stubs = args.evidence / 'stubs'
stubs.mkdir()
(stubs / 'CreateCommandMenu.h').write_bytes((root / 'src/cstrike/public/game_controls/CreateCommandMenu.h').read_bytes())
(stubs / 'utlstack.h').write_text('''#pragma once
#include <vector>
template<class T> struct CUtlStack {
    std::vector<T> items;
    void Push(T value) { items.push_back(value); }
    T Top() { return items.back(); }
    void Pop() { items.pop_back(); }
};
''')
prefix = r'''
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iterator>
#include <string>
#include <strings.h>
#include <vector>
#include "resource_text.h"
#include "CreateCommandMenu.h"
using qboolean = int;
using uchar32 = unsigned int;
constexpr int COM_TOKEN_SIZE = 1024;
int s_com_token_unget = 0;
int com_ignorecolons = 0;
char com_token[COM_TOKEN_SIZE];
// Only ASCII whitespace occurs outside quotes in this official fixture.
bool V_UTF8ToUChar32(const char* data, uchar32* point) { *point = static_cast<unsigned char>(*data); return false; }
const char* Q_UnicodeAdvance(const char* data, int cells) { return data + cells; }
#define _strnicmp strncasecmp
#define _snprintf std::snprintf
#define ASSERT_AT(condition, line) assert(condition)
std::vector<std::string> labels;
namespace vgui2 {
class Panel {};
class MenuItem { public: void SetChecked(bool) {} };
class Menu : public Panel {
public:
    Menu(Panel*, const char*) {}
    void SetVisible(bool) {}
    void AddCascadingMenuItem(const char* text, Panel*, Menu*, void*) { labels.push_back(text); }
    void AddMenuItem(const char* text, const char*, Panel*, void*) { labels.push_back(text); }
    void AddCheckableMenuItem(const char* text, const char*, Panel*, void*) { labels.push_back(text); }
    MenuItem* GetMenuItem(int) { static MenuItem item; return &item; }
    int GetMenuID(int index) { return index; }
    int GetItemCount() { return static_cast<int>(labels.size()); }
};
}
using namespace vgui2;
const char* COM_Parse(const char* data);
struct Client {
    std::string bytes;
    void* allocation = nullptr;
    int frees = 0;
    void* COM_LoadFile(char*, int, int* size) {
        if (size) { *size = static_cast<int>(bytes.size()); }
        allocation = std::malloc(bytes.size() + 1);
        std::memcpy(allocation, bytes.c_str(), bytes.size() + 1);
        return allocation;
    }
    void COM_FreeFile(void* pointer) {
        assert(pointer == allocation && frees == 0);
        std::free(pointer);
        ++frees;
    }
    char* COM_ParseFile(char* data, char* token) {
        const char* result = COM_Parse(data);
        std::strcpy(token, com_token);
        return const_cast<char*>(result);
    }
    const char* GetLevelName() { return ""; }
} client;
struct Viewport { Client* GetClientDllInterface() { return &client; } } viewport;
Viewport* gViewPortInterface = &viewport;
void CCreateCommandComboMenu::CreateCommandComboMenu(char*, ComboBox*, int) {}
Menu* CCreateCommandComboMenu::CreateCustomButton(Panel*, Menu*, char*, char*, char*, bool) { return nullptr; }
bool CCreateCommandComboMenu::IsCustomToken(const char*) { return false; }
class FixtureMenu : public CCreateCommandComboMenu {
public:
    FixtureMenu() { m_iDepth = 0; m_iIgnoring = 0; m_iIgnoreDepth = 0; }
};
'''
main = r'''
int main(int argc, char** argv) {
    assert(argc == 3);
    std::ifstream input(argv[1], std::ios::binary);
    client.bytes.assign(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
    FixtureMenu reader;
    char path[] = "spectcammenu.txt";
    Menu* menu = reader.CreateCommandMenu(path, 0, nullptr);
    assert(menu && client.frees == 1);
    std::ifstream expected(argv[2], std::ios::binary);
    std::string label;
    std::size_t index = 0;
    while (std::getline(expected, label, '\0')) {
        assert(index < labels.size() && labels[index] == label);
        ++index;
    }
    assert(index == 6 && index == labels.size());
    delete menu;
}
'''
cpp = args.evidence / 'actual_menu.cpp'
cpp.write_text(prefix + '\n' + parse + '\n' + menu + '\n' + main)
exe = args.evidence / 'actual_menu'
command = ['clang++', '-std=c++17', '-fsanitize=address,undefined', '-fno-omit-frame-pointer', '-g',
           '-I', str(stubs), '-I', str(root / 'src/common'),
           '-I', str(root / 'src/cstrike/public/game_controls'), str(cpp), '-o', str(exe)]
build = subprocess.run(command, text=True, capture_output=True)
(args.evidence / 'build.log').write_text(build.stdout + build.stderr)
assert build.returncode == 0, build.stderr
text = (root / 'src/deploy/localization/valve_koreana/spectcammenu.txt').read_text(encoding='utf-8')
data = args.evidence / 'spectcammenu.txt'
data.write_bytes(b'\xff\xfe' + text.encode('utf-16-le'))
expected = args.evidence / 'expected.txt'
expected.write_bytes(b''.join(label.encode('utf-8') + b'\0' for label in re.findall(r'^"\d+" "([^"]+)"', text, re.M)))
run = subprocess.run([str(exe), str(data), str(expected)], text=True, capture_output=True)
(args.evidence / 'run.log').write_text(run.stdout + run.stderr)
(args.evidence / 'result.json').write_text(json.dumps({'compile_command': command, 'exit': run.returncode}, indent=2) + '\n')
print('Actual command-menu reader:', 'PASS' if run.returncode == 0 else 'FAIL; see ' + str(args.evidence / 'run.log'))
raise SystemExit(run.returncode != 0)
