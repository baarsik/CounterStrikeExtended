#!/usr/bin/env python3
"""Exercise the actual engine draw gates under the default C locale."""
import argparse
import importlib.util
from pathlib import Path
import subprocess

p = argparse.ArgumentParser()
p.add_argument('--root', type=Path, required=True)
p.add_argument('--evidence', type=Path, required=True)
a = p.parse_args()
root = a.root.resolve()
a.evidence.mkdir(parents=True, exist_ok=False)
spec = importlib.util.spec_from_file_location('extract', root / 'tests/native/network_platform/extract_functions.py')
extract = importlib.util.module_from_spec(spec)
spec.loader.exec_module(extract)
text = (root / 'src/engine/vgui2/text_draw.cpp').read_text()
names = ['VGUI2_Draw_Character', 'VGUI2_Draw_CharacterAdd', 'VGUI2_Draw_StringLenW', 'VGUI2_DrawString']
if 'bool VGUI2_IsDrawableCharacter(' in text:
    names.insert(0, 'VGUI2_IsDrawableCharacter')
prefix = r'''
#include <cassert>
#include <climits>
#include <clocale>
#include <cstddef>
#include <cwchar>
#include <cwctype>
#include <string>
#include <vector>
namespace vgui2 { using HFont = unsigned int; }
struct Color { void SetColor(int, int, int, int) {} } _col;
struct Surface {
    std::vector<wchar_t> painted;
    void DrawSetTextFont(unsigned int) {}
    void DrawSetTextPos(int, int) {}
    void DrawSetTextColor(Color) {}
    void DrawSetTextColor(int, int, int, int) {}
    void DrawUnicodeChar(wchar_t ch) { painted.push_back(ch); }
    void DrawUnicodeCharAdd(wchar_t ch) { painted.push_back(ch); }
    void DrawPrintText(const wchar_t* text, int length) { painted.insert(painted.end(), text, text + length); }
    void DrawFlushText() {}
    int GetFontTall(unsigned int) { return 16; }
    void GetCharABCwide(unsigned int, int, int& a, int& b, int& c) { a = 0; b = 8; c = 0; }
} surface;
namespace vgui2 { Surface* surface() { return &::surface; } }
struct BaseSurface { void ResetViewPort() {} } g_BaseUISurface;
wchar_t localized[] = L"Привет, Łódź!";
const wchar_t* VGUI2_Find_String(const char*) { return localized; }
'''
main = r'''
int main() {
    assert(std::setlocale(LC_CTYPE, "C"));
    const wchar_t characters[] = {L'A', L'Ж', L'Ł', L'ą', L'ó'};
    for (wchar_t character : characters) {
        surface.painted.clear();
        assert(VGUI2_Draw_Character(0, 0, character, 1) == 8);
        assert(surface.painted.size() == 1 && surface.painted[0] == character);
        surface.painted.clear();
        assert(VGUI2_Draw_CharacterAdd(0, 0, character, 255, 255, 255, 1) == 8);
        assert(surface.painted.size() == 1 && surface.painted[0] == character);
    }
    for (int character : {0, 9, 10, 31, 127, 159, 0xd800, 0x110000}) {
        surface.painted.clear();
        VGUI2_Draw_Character(0, 0, character, 1);
        assert(surface.painted.empty());
    }
    surface.painted.clear();
    VGUI2_DrawString(0, 0, "fixture", 1);
    assert(std::wstring(surface.painted.begin(), surface.painted.end()) == L"Привет, Łódź!");
    assert(std::wstring(localized) == L"Привет, Łódź!");
}
'''
cpp = a.evidence / 'actual_engine_glyphs.cpp'
cpp.write_text(prefix + '\n\n'.join(extract.extract(text, name) for name in names) + main)
exe = a.evidence / 'actual_engine_glyphs'
build = subprocess.run(['clang++', '-std=c++17', '-fsanitize=address,undefined', '-g', str(cpp), '-o', str(exe)],
                       text=True, capture_output=True)
(a.evidence / 'build.log').write_text(build.stdout + build.stderr)
assert build.returncode == 0, build.stderr
run = subprocess.run([str(exe)], text=True, capture_output=True)
(a.evidence / 'run.log').write_text(run.stdout + run.stderr)
print('Actual engine Unicode draw gates:', 'PASS' if run.returncode == 0 else 'FAIL')
raise SystemExit(run.returncode != 0)
