from pathlib import Path
import subprocess
root=Path(__file__).resolve().parents[3]
out=root/'build/live-localization/cs-menu-tree-tests';out.mkdir(parents=True,exist_ok=True)
s=(root/'src/cstrike/vgui2/game_controls/CreateCommandMenu.cpp').read_text(encoding='latin1')
a=s.index('void RefreshCommandMenuText(');b=s.index('{',a)+1;d=1;e=b
while d:
 d+=(s[e]=='{')-(s[e]=='}');e+=1
body=s[a:e]
h=r'''
#include <cassert>
#include <cwchar>
#include <string>
#include <vector>
namespace vgui2 {
 struct Menu;
 struct MenuItem {
  std::wstring text;int action;Menu* child=nullptr;
  void GetText(wchar_t* out,int size) {std::wcsncpy(out,text.c_str(),size - 1);}
  void SetText(const wchar_t* value) {text=value;}
  Menu* GetMenu() {return child;}
 };
 struct Menu {
  std::vector<MenuItem*> items;int active=1;bool visible=true;int invalidations=0;
  int GetItemCount() {return static_cast<int>(items.size());}
  int GetMenuID(int row) {return row;}
  MenuItem* GetMenuItem(int row) {return items[row];}
  void InvalidateLayout(bool,bool) {++invalidations;}
 };
}
'''+body+r'''
int main() {
 vgui2::MenuItem nestedOld{L"Old weapon",42};
 vgui2::MenuItem nestedNew{L"Nowa broń",0};
 vgui2::Menu oldSub{{&nestedOld},0,true};
 vgui2::Menu newSub{{&nestedNew},0,false};
 vgui2::MenuItem oldRoot{L"Old category",17,&oldSub};
 vgui2::MenuItem newRoot{L"Nowa kategoria",0,&newSub};
 vgui2::Menu oldMenu{{&oldRoot},0,true};
 vgui2::Menu translated{{&newRoot},0,false};
 RefreshCommandMenuText(&oldMenu,&translated);
 assert(oldRoot.text==L"Nowa kategoria" && nestedOld.text==L"Nowa broń");
 assert(oldRoot.action==17 && nestedOld.action==42 && oldRoot.child==&oldSub);
 assert(oldMenu.visible && oldSub.visible && oldMenu.active==0 && oldSub.active==0);
 assert(oldMenu.items[0]==&oldRoot && oldSub.items[0]==&nestedOld);
}
'''
(out/'menu.cpp').write_text(h)
subprocess.run(['c++','-std=c++17','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',str(out/'menu.cpp'),'-o',str(out/'menu')],check=True)
subprocess.run([str(out/'menu')],check=True)
