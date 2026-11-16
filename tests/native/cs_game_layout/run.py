#!/usr/bin/env python3
"""Exercise scoreboard column budgeting from its production owner with controlled screen metrics."""
from pathlib import Path
import subprocess
root = Path(__file__).resolve().parents[3]
out = root / 'build/smoked-game-layouts-20261115/native'
out.mkdir(parents=True, exist_ok=True)
def function(source, signature):
    start = source.index(signature)
    brace = source.index('{', start)
    depth, end = 1, brace + 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end] + '\n'
source = (root / 'src/cstrike/cl_dll/VGUI/cstrikeclientscoreboard.cpp').read_text(encoding='latin1')
harness = r'''
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cwchar>
constexpr int ScoreboardFontHeight=16;
namespace cse::ui { bool glass = false; bool ControlsSmokedGlass() { return glass; } }
namespace vgui2 {
using HFont=int;
struct IScheme { HFont GetFont(const char*,bool) {return 1;} } fonts;
struct Scheme { int GetProportionalScaledValue(int n) { return n * 3; }
    IScheme* GetIScheme(int) {return &fonts;} } instance;
struct Surface {void GetTextSize(HFont,const wchar_t* text,int& w,int& h) {w=std::wcslen(text)*7;h=16;} } screen;
Surface* surface() {return &screen;}
Scheme* scheme() { return &instance; }
namespace smoked { float scale = 1; int Pixels(float n) { return std::lround(n * scale); }
template<typename T> HFont TextFont(T*,IScheme*,HFont n,bool,int) {return n;}
}
}
struct List { int width = 968; int GetWide() { return width; } } list;
struct CCSClientScoreBoardDialog {
    enum { CSTRIKE_MIN_WIDTH=38, CSTRIKE_NAME_WIDTH=209, CSTRIKE_ACCOUNT_WIDTH=57,
           CSTRIKE_CLASS_WIDTH=56, CSTRIKE_SPACER1_WIDTH=5, CSTRIKE_SCORE_WIDTH=45,
           CSTRIKE_DEATH_WIDTH=46, CSTRIKE_PING_WIDTH=50, CSTRIKE_HEALTH_WIDTH=41 };
    bool m_bShortHeaders=false, m_bHealthColumnEnabled=false, m_bMoneyColumnEnabled=false;
    int m_nAvatarColumnWidth=0;
    List* m_pPlayerList=&list;
    int GetScheme() {return 0;}
    const wchar_t* GetScoreHeader() {return L"Score";}
    const wchar_t* GetDeathHeader() {return L"Deaths";}
    const wchar_t* GetPingHeader() {return L"Latency";}
    const wchar_t* GetHealthHeader() {return L"Health";}
    const wchar_t* GetMoneyHeader() {return L"Money";}
    int GetHeaderWidthNoScale(int maximum, int minimum);
    int GetHeaderWidth(int maximum, int minimum);
    int GetNameHeaderWidth();
};
'''
for name in ['GetHeaderWidthNoScale', 'GetHeaderWidth', 'GetNameHeaderWidth']:
    harness += function(source, 'int CCSClientScoreBoardDialog::' + name + '(')
harness += r'''
int main() {
    CCSClientScoreBoardDialog board;
    assert(board.GetHeaderWidth(45,0)==135);
    assert(board.GetNameHeaderWidth()==921);
    cse::ui::glass=true;
    for (float scale : {0.75f, 1.0f}) {
        vgui2::smoked::scale=scale;
        for (int width : {592, 968}) {
            list.width=width;
            for (bool shortHeaders : {false,true}) for (bool health : {false,true})
            for (bool money : {false,true}) for (int avatar : {0,32}) {
                board.m_bShortHeaders=shortHeaders;
                board.m_bHealthColumnEnabled=health; board.m_bMoneyColumnEnabled=money;
                board.m_nAvatarColumnWidth=avatar;
                const int total=board.GetNameHeaderWidth()+avatar+5+vgui2::smoked::Pixels(56)+
                    board.GetHeaderWidth(45,0)+board.GetHeaderWidth(46,0)+board.GetHeaderWidth(50,0)+
                    (health?board.GetHeaderWidth(41,0):0)+(money?board.GetHeaderWidth(57,57):0);
                if (total > width-24 || board.GetNameHeaderWidth()<120) {
                    std::fprintf(stderr,"column budget: width=%d total=%d name=%d scale=%.2f\n",
                                 width,total,board.GetNameHeaderWidth(),scale);
                    return 1;
                }
            }
        }
    }
    puts("scoreboard: compact/high resolution, optional columns, avatars and Original budgets passed");
}
'''
(out / 'scoreboard.cpp').write_text(harness)
subprocess.run(['c++','-std=c++17','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',str(out/'scoreboard.cpp'),'-o',str(out/'scoreboard')],check=True)
subprocess.run([str(out/'scoreboard')],check=True)
