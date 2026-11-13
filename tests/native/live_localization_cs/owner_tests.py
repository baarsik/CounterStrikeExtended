#!/usr/bin/env python3
"""Exercise the production legacy menu retranslation path with controlled text providers."""
from pathlib import Path
import subprocess
import sys
root = Path(__file__).resolve().parents[3]
out = root / 'build/live-localization/cs-owner-tests'
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

source = (root / 'src/cstrike/cl_dll/menu.cpp').read_text(encoding='latin1')
body = function(source, 'void CHudMenu::RefreshLanguage(')
harness = r'''
#include <cassert>
#include <cstring>
#include <cstdlib>
#include <string>
constexpr int MAX_MENU_STRING = 512;
char g_szMenuString[MAX_MENU_STRING];
char g_szPrelocalisedMenuString[MAX_MENU_STRING];
int conversions = 0;
struct TextMessage { char* BufferedLocaliseTextString(const char* text) {
    static char translated[MAX_MENU_STRING];
    std::strcpy(translated, std::strcmp(text, "#Menu") == 0 ? "Polskie menu" : text);
    return translated;
}};
struct Hud { TextMessage m_TextMessage; } gHUD;
int KB_ConvertString(char* input, char** output) {
    ++conversions;
    *output = static_cast<char*>(std::malloc(std::strlen(input) + 1));
    std::strcpy(*output, input);
    return 1;
}
struct CHudMenu {
    bool m_fWaitingForMore = false;
    int m_bitsValidSlots = 7;
    float m_flShutoffTime = 123.0f;
    int m_fMenuDisplayed = 1;
    void RefreshLanguage();
};
'''
harness += body + r'''
int main() {
    CHudMenu menu;
    std::strcpy(g_szPrelocalisedMenuString, "#Menu");
    std::strcpy(g_szMenuString, "English menu");
    menu.RefreshLanguage();
    assert(std::strcmp(g_szMenuString, "Polskie menu") == 0);
    assert(menu.m_bitsValidSlots == 7 && menu.m_flShutoffTime == 123.0f && menu.m_fMenuDisplayed == 1);
    assert(conversions == 1);
    menu.m_fWaitingForMore = true;
    std::strcpy(g_szPrelocalisedMenuString, "#incomplete");
    menu.RefreshLanguage();
    assert(std::strcmp(g_szMenuString, "Polskie menu") == 0 && conversions == 1);
}
'''
(out / 'menu.cpp').write_text(harness)
subprocess.run(['c++', '-std=c++17', '-Wall', '-Wextra', '-Werror', '-fsanitize=address,undefined',
                str(out / 'menu.cpp'), '-o', str(out / 'menu')], check=True)
subprocess.run([str(out / 'menu')], check=True)
