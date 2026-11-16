#!/usr/bin/env python3
"""Run the production team and buy layout owners against a minimal VGUI geometry surface."""
from pathlib import Path
import subprocess
root=Path(__file__).resolve().parents[3]
out=root/'build/smoked-game-layouts-20261115/native'
out.mkdir(parents=True,exist_ok=True)
def function(source,signature):
    start=source.index(signature); brace=source.index('{',start); depth,end=1,brace+1
    while depth:
        depth+=(source[end]=='{')-(source[end]=='}');end+=1
    return source[start:end]+'\n'
harness=r'''
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstring>
#include <cstdlib>
#include <map>
#include <string>
#include <vector>
#include <initializer_list>
#define stricmp strcasecmp
#define strnicmp strncasecmp
namespace cse::ui {
constexpr int WindowInset=16, WindowHeaderHeight=48, ControlHeight=32;
bool glass=false, styleChanged=false;
bool ControlsStyleChanged(const void*) {return styleChanged;}
bool ControlsSmokedGlass() { return glass; }
struct Geometry { std::string x,y; int width=0,height=0; };
std::map<const void*,Geometry> geometry;
auto& ControlsResourceGeometry() { return geometry; }
}
namespace vgui2 {
using HFont=int;
struct IScheme {};
struct Scheme { IScheme value; IScheme* GetIScheme(int) { return &value; } } manager;
Scheme* scheme() { return &manager; }
struct Surface { int w=2560,h=1440;void GetScreenSize(int& x,int& y) {x=w;y=h;}
    void GetProportionalBase(int& x,int& y) {x=640;y=480;}
} screen;
Surface* surface() { return &screen; }
struct Panel {
    std::string name;int x=0,y=0,w=0,h=0;bool visible=true;
    std::vector<Panel*> children;
    Panel* parent=nullptr;
    Panel* GetParent() {return parent;}
    virtual ~Panel()=default;
    Panel(const char* n=""):name(n) {}
    virtual void PerformLayout() {}
    virtual void OnSizeChanged(int,int) {++panelSizeCalls;}
    int panelSizeCalls=0;
    const char* GetName() { return name.c_str(); }
    int GetChildCount() { return children.size(); }
    Panel* GetChild(int n) { return children[n]; }
    Panel* FindChildByName(const char* n,bool) {
        for(auto* c:children) if(!strcasecmp(c->GetName(),n)) return c;
        return nullptr;
    }
    void GetPos(int& a,int& b) {a=x;b=y;}
    void SetPos(int a,int b) {x=a;y=b;}
    int GetWide() { return w; } int GetTall() { return h; } int GetScheme() { return 0; }
    void SetBounds(int a,int b,int c,int d) { x=a;y=b;w=c;h=d; }
    bool IsVisible() { return visible; }void SetVisible(bool v) { visible=v; }
    void InvalidateLayout(bool,bool) {}
    void SetMouseInputEnabled(bool state) {mouse=state;}
    bool mouse=false;
};
struct Label:Panel {using Panel::Panel; int font=1;int GetFont(){return font;}void SetFont(int n){font=n;} };
struct Button:Label {using Label::Label;};
struct List:Panel {
    int contentHeight=0;bool scroll=false;
    void GetContentSize(int& x,int& y) {x=w;y=contentHeight;}
    void SetVerticalScrollbar(bool state) {scroll=state;}
};
struct EditablePanel:Panel {using Panel::Panel;};
struct WizardSubPanel:EditablePanel {using EditablePanel::EditablePanel;};
struct WizardPanel:EditablePanel { Panel* page=nullptr;Panel* GetCurrentSubPanel(){return page;}
    virtual void GetClientArea(int& x,int&,int& width,int& height) {x=5;width=w-10;height=h;}
};
namespace smoked {
float scale=1;
int Pixels(float n) {return std::lround(n*scale);}
HFont TextFont(Panel*,IScheme*,HFont n,bool,int=0) {return n;}
bool DocumentLayout(Panel*) {return false;}
void Decorations(std::initializer_list<Panel*> panels) {for(auto* panel:panels) assert(panel);}
void RefreshResourceBounds(Panel* panel) {
    const auto found=cse::ui::geometry.find(panel);
    if(found!=cse::ui::geometry.end()) {
        const auto& source=found->second;
        panel->SetBounds(std::atoi(source.x.c_str()),std::atoi(source.y.c_str()),source.width,source.height);
    }
}
}
}
struct CTeamMenu:vgui2::Panel {virtual void ApplySchemeSettings(vgui2::IScheme*) {}};
struct CCSTeamMenu:CTeamMenu {
    void ApplySchemeSettings(vgui2::IScheme*) override;
    void PerformLayout() override;
};
struct CClassMenu:vgui2::Panel {virtual void ApplySchemeSettings(vgui2::IScheme*) {}};
struct CCSClassMenu:CClassMenu {
    struct ClassHelperPanel:vgui2::EditablePanel {void PerformLayout() override;};
    vgui2::Panel info;
    vgui2::Panel* m_pPanel=&info;
    void ApplySchemeSettings(vgui2::IScheme*) override;
    void PerformLayout() override;
};
struct CBuyMenu:vgui2::WizardPanel {
    void GetClientArea(int& x,int& y,int& width,int& height) override;
    void PerformLayout() override;
};
struct CBuySubMenu:vgui2::WizardSubPanel {
    using WizardSubPanel::WizardSubPanel;
    struct ClassHelperPanel:vgui2::Panel {};
    vgui2::Panel info;
    vgui2::Panel* m_pPanel=&info;
    void RestoreOriginalLayout();
    void PerformLayout() override;
};
'''
harness+=r'''
int fullscreenFits=0;
void FitToFullScreen(vgui2::Panel* panel) {
    assert(!cse::ui::glass && panel->w==64 && panel->h==24);
    ++fullscreenFits;
    panel->SetBounds(0,0,vgui2::screen.w,vgui2::screen.h);
}
'''
harness+=function((root/'src/cstrike/cl_dll/VGUI/buysubmenu.cpp').read_text(encoding='latin1'),
                  'void CBuySubMenu::RestoreOriginalLayout()')
header=(root/'src/cstrike/cl_dll/VGUI/smoked_game_layout.h').read_text()
harness+='\n'.join(line for line in header.splitlines() if not line.startswith('#'))+'\n'
background=(root/'src/cstrike/cl_dll/VGUI/backgroundpanel.cpp').read_text(encoding='latin1')
start=background.index('    const auto resourcePanel =',background.index('::LayoutBackground('))
end=background.index('\n    };',start)+len('\n    };')
harness+='struct BackgroundPredicate:vgui2::Panel { bool Accept(vgui2::Panel* panel, bool restoreResources=true) {\n'
harness+=background[start:end]+'\nreturn resourcePanel(panel);\n}};\n'

harness+=r'''
struct Viewport {
    bool centered=false;
    bool IsSpectatorGUIVisible(){return false;}
    bool HasCenteredPersistentWindows(){return centered;}
} viewport;
using CounterStrikeViewport=Viewport;
auto* gViewPortInterface=&viewport;
struct CClientScoreBoardDialog:vgui2::Panel {
    int legacySizeCalls=0;
    void OnSizeChanged(int,int) override {++legacySizeCalls;}
};
struct CCSClientScoreBoardDialog:CClientScoreBoardDialog {
    enum {CSTRIKE_CLASS_WIDTH=56};
    vgui2::List list;
    vgui2::List* m_pPlayerList=&list;
    void* m_pShortHeaders=nullptr;
    int headers=0,columns=0;
    void OnSizeChanged(int,int) override;
    void UpdateHeaders(bool) {assert(m_pShortHeaders);++headers;}
    int GetNameHeaderWidth() {assert(m_pShortHeaders);return 200;}
    void UpdateColumnInAllSections(const char*,const wchar_t*,int,bool) {assert(m_pShortHeaders);++columns;}
    void PerformLayout() override;
};
'''
harness+=function((root/'src/cstrike/cl_dll/VGUI/backgroundpanel.cpp').read_text(encoding='latin1'),
                  'int GetAlternateProportionalValueFromNormal(')
harness+=function((root/'src/cstrike/cl_dll/VGUI/cstriketeammenu.cpp').read_text(encoding='latin1'),
                  'void CCSTeamMenu::ApplySchemeSettings(')
harness+=function((root/'src/cstrike/cl_dll/VGUI/cstrikeclientscoreboard.cpp').read_text(encoding='latin1'),
                  'void CCSClientScoreBoardDialog::PerformLayout()')
harness+=function((root/'src/cstrike/cl_dll/VGUI/cstrikeclientscoreboard.cpp').read_text(encoding='latin1'),
                  'void CCSClientScoreBoardDialog::OnSizeChanged(')
harness+=function((root/'src/cstrike/cl_dll/VGUI/buymenu.cpp').read_text(encoding='latin1'),
                  'void CBuyMenu::GetClientArea(')
for signature in ['void CCSClassMenu::ApplySchemeSettings(', 'void CCSClassMenu::PerformLayout()',
                  'void CCSClassMenu::ClassHelperPanel::PerformLayout()']:
    harness+=function((root/'src/cstrike/cl_dll/VGUI/cstrikeclassmenu.cpp').read_text(encoding='latin1'),signature)
for filename,signature in [('cstriketeammenu.cpp','void CCSTeamMenu::PerformLayout()'),('buymenu.cpp','void CBuyMenu::PerformLayout()'),('buysubmenu.cpp','void CBuySubMenu::PerformLayout()')]:
    harness+=function((root/'src/cstrike/cl_dll/VGUI'/filename).read_text(encoding='latin1'),signature)
harness+=r'''
void contained(vgui2::Panel& parent,vgui2::Panel& child) {
    assert(child.x>=0 && child.y>=0 && child.x+child.w<=parent.w && child.y+child.h<=parent.h);
}
int main() {
    for (auto size : {std::pair{640,480},std::pair{2560,1440}}) {
        vgui2::screen.w=size.first;vgui2::screen.h=size.second;
        vgui2::smoked::scale=size.second<600?0.75f:1.0f;
        CCSTeamMenu team;
        vgui2::Label title("joinTeam");vgui2::Panel map("MapInfo");
        vgui2::Button t("terbutton"),ct("ctbutton"),vip("vipbutton"),automatic("autobutton"),spec("specbutton"),cancel("CancelButton");
        vip.visible=false;team.children={&title,&map,&t,&ct,&vip,&automatic,&spec,&cancel};
        team.SetBounds(76,0,552,448);title.SetBounds(0,22,500,48);
        cse::ui::glass=false;team.PerformLayout();assert(team.x==76 && team.w==552 && title.w==500);
        cse::ui::glass=true;team.PerformLayout();
        for(auto* c:team.children) if(c->visible) contained(team,*c);
        assert(t.y+ t.h<ct.y && ct.y+ct.h<automatic.y && map.x>t.x+t.w);
        CBuyMenu buy;CBuySubMenu page("mainmenu");buy.page=&page;buy.PerformLayout();
        assert(buy.x>=0 && buy.y>=0 && buy.x+buy.w<=size.first && buy.y+buy.h<=size.second);
        contained(buy,page);
        cse::ui::glass=false;
        int x=0,y=54,w=0,h=0;buy.GetClientArea(x,y,w,h);assert(y==0);
        cse::ui::glass=true;
        vgui2::Label buyTitle("Title"),category("selectCategory");
        vgui2::Button buttons[8];
        page.children={&buyTitle,&category};
        for(int i=0;i<8;++i) {
            buttons[i].name="category"+std::to_string(i);
            page.children.push_back(&buttons[i]);
            cse::ui::geometry[&buttons[i]].y=std::to_string(116+i*32);
        }
        vgui2::Button cancelBuy("CancelButton"),autobuy("AutobuyButton"),rebuy("RebuyButton");
        cse::ui::geometry[&cancelBuy].y="380";cse::ui::geometry[&autobuy].y="116";cse::ui::geometry[&rebuy].y="148";
        page.children.insert(page.children.end(),{&cancelBuy,&autobuy,&rebuy});
        CBuySubMenu::ClassHelperPanel detail;page.children.push_back(&detail);
        page.PerformLayout();
        for(auto* c:page.children) if(c->visible) contained(page,*c);
        assert(!detail.visible && autobuy.x>buttons[0].x+buttons[0].w);
        assert(rebuy.y>autobuy.y && cancelBuy.y>buttons[7].y+buttons[7].h);
        for(int i=1;i<8;++i) assert(buttons[i].y>buttons[i-1].y+buttons[i-1].h);
        const auto previous=buttons[0].y;page.PerformLayout();assert(buttons[0].y==previous);
        cse::ui::glass=false;page.PerformLayout();assert(buttons[0].y==previous);
        for(int i=0;i<8;++i) {
            cse::ui::geometry[&buttons[i]]={"76",std::to_string(116+i*32),148,20};
        }
        page.info.SetBounds(244,116,400,380);
        page.RestoreOriginalLayout();
        assert(buttons[0].x==76 && buttons[0].y==116 && buttons[0].w==148 && buttons[0].h==20);
        assert(detail.x==244 && detail.y==116 && detail.w==400 && detail.h==380);
        assert(page.x==0 && page.y==0 && page.w==size.first && page.h==size.second);
        cse::ui::glass=true;
        CCSClientScoreBoardDialog scoreboard;
        scoreboard.PerformLayout();assert(scoreboard.columns==0 && scoreboard.headers==0);
        scoreboard.OnSizeChanged(616,456);assert(scoreboard.legacySizeCalls==0 && scoreboard.panelSizeCalls==1);
        scoreboard.m_pShortHeaders=&scoreboard;
        scoreboard.list.contentHeight=2000;scoreboard.PerformLayout();
        assert(scoreboard.mouse && scoreboard.list.mouse && scoreboard.list.scroll && scoreboard.columns==2);
        contained(scoreboard,scoreboard.list);
        scoreboard.list.contentHeight=100;scoreboard.PerformLayout();assert(!scoreboard.mouse);
        cse::ui::glass=false;scoreboard.PerformLayout();assert(!scoreboard.list.scroll);
        scoreboard.OnSizeChanged(520,340);assert(scoreboard.legacySizeCalls==1);
        CCSClassMenu selector;
        vgui2::Label classTitle("joinClass"),description("infolabel");
        vgui2::Panel portrait("classimage");
        vgui2::Button urban("urban"),gign("gign"),automaticClass("autoselect_ct"),classCancel("CancelButton");
        CCSClassMenu::ClassHelperPanel classPage;
        classPage.children={&portrait,&description};
        selector.children={&classTitle,&urban,&gign,&automaticClass,&classCancel,&classPage};
        cse::ui::geometry[&urban]={"0","116",148,20};
        cse::ui::geometry[&gign]={"0","212",148,20};
        cse::ui::geometry[&automaticClass]={"0","276",148,20};
        cse::ui::geometry[&classCancel]={"0","340",148,20};
        cse::ui::geometry[&portrait]={"0","0",256,196};
        cse::ui::geometry[&description]={"0","228",300,80};
        selector.SetBounds(76,0,552,448);selector.PerformLayout();assert(selector.x==76 && selector.w==552);
        cse::ui::glass=true;selector.PerformLayout();classPage.PerformLayout();
        for(auto* c:selector.children) if(c->visible) contained(selector,*c);
        contained(classPage,portrait);contained(classPage,description);
        assert(urban.x>0 && urban.y+urban.h<gign.y && automaticClass.y+automaticClass.h<classCancel.y);
        assert(description.y>portrait.y+portrait.h);
        assert(std::abs(portrait.w*196-portrait.h*256)<=256);
        cse::ui::glass=false;cse::ui::styleChanged=true;
        selector.SetPos(76,0);selector.info.SetBounds(168,116,400,380);
        selector.ApplySchemeSettings(nullptr);
        assert(classPage.x==168 && classPage.y==116 && classPage.w==400 && classPage.h==380);
        assert(selector.x==76+(size.first-GetAlternateProportionalValueFromNormal(640))/2);
        // VGUI delivers the shell resize after scheme replay; fixed resource anchors must survive it.
        cse::ui::geometry[&selector.info]={"168","116",400,380};
        selector.info.SetBounds(207,48,333,388);
        classPage.SetBounds(207,48,333,388);
        selector.PerformLayout();
        assert(classPage.x==168 && classPage.y==116 && classPage.w==400 && classPage.h==380);
        cse::ui::styleChanged=true;
        team.SetPos(76,0);viewport.centered=false;team.ApplySchemeSettings(nullptr);assert(team.x==76);
        team.SetPos(76,0);viewport.centered=true;team.ApplySchemeSettings(nullptr);
        assert(team.x==76+(size.first-GetAlternateProportionalValueFromNormal(640))/2);
        cse::ui::styleChanged=false;
        BackgroundPredicate background;
        vgui2::Label decoration("caption");decoration.parent=&background;
        CBuyMenu runtimeMenu;runtimeMenu.parent=&background;
        vgui2::Button runtimeControl("action");runtimeControl.parent=&runtimeMenu;
        assert(background.Accept(&decoration));
        assert(!background.Accept(&runtimeMenu) && !background.Accept(&runtimeControl));
        cse::ui::geometry.clear();
    }
}
'''
(out/'menus.cpp').write_text(harness)
subprocess.run(['c++','-std=c++17','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',str(out/'menus.cpp'),'-o',str(out/'menus')],check=True)
subprocess.run([str(out/'menus')],check=True)
print('production team/buy owners: containment, action order, auxiliary columns, idempotence and Original delegation passed')
