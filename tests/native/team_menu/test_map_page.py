#!/usr/bin/env python3
"""Run actual team-menu page methods with controlled filesystem/UI services."""
from pathlib import Path
import argparse, hashlib, importlib.util, json, subprocess
p=argparse.ArgumentParser()
p.add_argument('--root',type=Path,required=True)
p.add_argument('--evidence',type=Path,required=True)
a=p.parse_args()
a.root=a.root.resolve()
a.evidence.mkdir(parents=True,exist_ok=False)
extractor=a.root/'tests/native/network_platform/extract_functions.py'
spec=importlib.util.spec_from_file_location('extract',extractor)
m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
source=a.root/'src/cstrike/vgui2/game_controls/teammenu.cpp'
viewport=a.root/'src/cstrike/vgui2/game_controls/vgui_TeamFortressViewport.cpp'
header=a.root/'src/cstrike/public/game_controls/teammenu.h'
text=source.read_text(encoding='cp1252')
methods=[m.extract(text,'CTeamMenu::'+n) for n in ['LoadMapPage','Update','Activate']]
if 'void CTeamMenu::InvalidateMapPage(' in text:
 methods.append(m.extract(text,'CTeamMenu::InvalidateMapPage'))
methods.append(m.extract(viewport.read_text(encoding='cp1252'),'TeamFortressViewport::OnLevelChange'))
stubs=a.evidence/'stubs';stubs.mkdir()
(stubs/'Frame.h').write_text('''#pragma once
#define DECLARE_CLASS_SIMPLE_NAMED(...)
#define MESSAGE_FUNC_INT(name, message, argument) void name(int argument)
namespace vgui2 {
class IScheme {};
using KeyCode = int;
class Panel {};
class Frame {
public:
 Frame(Panel* = nullptr, const char* = "", bool = false, bool = false) {}
 virtual ~Frame() = default;
 virtual void ApplySchemeSettings(IScheme*) {}
 virtual void Activate() {}
 virtual void OnKeyCodePressed(KeyCode) {}
 void SetVisible(bool) {}
};
}
''')
(stubs/'utlvector.h').write_text('#pragma once\ntemplate<class T> class CUtlVector {};\n')
prefix=r'''
#include <algorithm>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <memory>
#include <string>
#include <vector>
#if __has_include("resource_text.h")
#include "resource_text.h"
#endif
#include "teammenu.h"
#define _snprintf std::snprintf
namespace vgui2 {
class RichText {
public:
 int replacements = 0;
 int scrollResets = 0;
 std::string text;
 void SetText(const char* s) { text = s; ++replacements; }
 void SetText(const wchar_t*) { text = "wide"; ++replacements; }
 void GotoTextStart() { ++scrollResets; }
};
class Label {
public:
 int updates = 0;
 void SetText(const char*) { ++updates; }
};
}
struct OpenFile { std::string bytes; bool closed = false; };
using FileHandle_t = OpenFile*;
struct FileSystem {
 std::map<std::string, std::string> files;
 std::vector<std::unique_ptr<OpenFile>> handles;
 int exists = 0;
 int opens = 0;
 int reads = 0;
 int closes = 0;
 bool failOpen = false;
 bool shortRead = false;
 bool FileExists(const char* name) { ++exists; return files.count(name) != 0; }
 FileHandle_t Open(const char* name, const char*, void*) {
  ++opens;
  if (failOpen || !files.count(name)) { return nullptr; }
  handles.push_back(std::make_unique<OpenFile>(OpenFile{files.at(name)}));
  return handles.back().get();
 }
 unsigned Size(FileHandle_t f) { assert(f && !f->closed); return static_cast<unsigned>(f->bytes.size()); }
 int Read(void* target, int size, FileHandle_t f) {
  assert(f && !f->closed); ++reads;
  int count = shortRead && size ? size - 1 : size;
  std::memcpy(target, f->bytes.data(), static_cast<std::size_t>(count));
  return count;
 }
 void Close(FileHandle_t f) { assert(f && !f->closed); f->closed = true; ++closes; }
} fs;
namespace vgui2 { FileSystem* filesystem() { return &fs; } }
struct GameFunctions { int GetVGUI2KeyCodeForBind(const char*) { return 1; } } gameFunctions;
GameFunctions* gameuifuncs = &gameFunctions;
static vgui2::RichText* page;
static vgui2::Label* label;
static int buttons;
CTeamMenu::CTeamMenu(vgui2::Panel* p) : vgui2::Frame(p) {
 m_pMapInfo = page = new vgui2::RichText;
 m_pMapName = label = new vgui2::Label;
 m_iJumpKey = 0;
 m_iScoreBoardKey = 0;
}
CTeamMenu::~CTeamMenu() { delete m_pMapInfo; delete m_pMapName; }
void CTeamMenu::ApplySchemeSettings(vgui2::IScheme*) {}
void CTeamMenu::OnKeyCodePressed(vgui2::KeyCode) {}
void CTeamMenu::MakeTeamButtons(const char**, int) { ++buttons; }
struct ScoreBoard { void Reset() {} void SetVisible(bool) {} };
struct Spectator { void SetVisible(bool) {} };
class TeamFortressViewport {
public:
 char m_sMapName[64]{};
 float m_flScoreBoardLastUpdated = 0;
 CTeamMenu* m_pTeamMenu;
 ScoreBoard scoreboard;
 Spectator spectator;
 ScoreBoard* m_pClientScoreBoard = &scoreboard;
 Spectator* m_pSpectatorGUI = &spectator;
 void HideAllVGUIMenu() {}
 void OnLevelChange();
};
'''
# The real UI layout/repaint services are not needed for page/content/cache decisions.
# The production methods call these inherited UI services.
frame=(stubs/'Frame.h').read_text().replace(' void SetVisible(bool) {}',' void SetVisible(bool) {}\n void InvalidateLayout(bool, bool) {}\n void Repaint() {}')
(stubs/'Frame.h').write_text(frame)
def cpp_bytes(data):
 return 'std::string("'+''.join('\\x%02x'%b for b in data)+'", '+str(len(data))+')'

manifest=json.loads((a.root/'src/deploy/localization/manifest.json').read_text())
unicode_checks=[]
for entry in manifest['files']:
 if '/maps/' in entry['destination'] and entry['destination'].endswith('.txt'):
  text=(a.root/'src/deploy/localization'/entry['source']).read_text(encoding='utf-8')
  encoding=entry['encoding']
  data=b'\xff\xfe'+text.encode('utf-16-le') if encoding=='utf-16-le-bom' else text.encode(encoding)
  name=entry['destination'].replace('/','_').removesuffix('.txt')
  unicode_checks += ['fs.files["maps/'+name+'.txt"] = '+cpp_bytes(data)+';',
   'update("'+name+'");', 'assert(page->text == '+cpp_bytes(text.encode('utf-8'))+');']
for endian,bom in [('le',b'\xff\xfe'),('be',b'\xfe\xff')]:
 text='Briefing: français 中文 한국어 \U0001f680'
 unicode_checks += ['fs.files["maps/'+endian+'.txt"] = '+cpp_bytes(bom+text.encode('utf-16-'+endian))+';',
  'update("'+endian+'");', 'assert(page->text == '+cpp_bytes(text.encode('utf-8'))+');']
main=r'''
int main(int argc, char** argv) {
 assert(argc == 2);
 CTeamMenu menu(nullptr);
 const char* names[] = {"CT", "T", nullptr};
 auto update = [&](const char* map) { menu.Update(map, true, names, 2); };
 std::string mode = argv[1];
 if (mode == "repeated") {
  fs.files["maps/a.txt"] = "map A text!!";
  for (int i = 0; i < 100; ++i) { update("a"); }
  if (fs.opens != 1 || fs.exists != 1 || page->scrollResets != 1) {
   std::fprintf(stderr, "same-map score updates reloaded page: opens=%d exists=%d resets=%d\n",
    fs.opens, fs.exists, page->scrollResets); return 20;
  }
  assert(label->updates == 100 && buttons == 100 && page->text == "map A text!!");
 } else if (mode == "empty-fallback") {
  fs.files["maps/a.txt"] = "";
  update("a"); update("a");
  assert(fs.opens == 1 && page->text.empty());
  fs.files["maps/default.txt"] = "default info";
  update("missing"); update("missing");
  assert(fs.opens == 2 && page->text == "default info");
  update("other");
  assert(fs.opens == 3 && fs.closes == 3);
 } else if (mode == "retry") {
  update("a");
  fs.files["maps/a.txt"] = "map A text!!";
  fs.failOpen = true; update("a");
  fs.failOpen = false; fs.shortRead = true; update("a");
  assert(page->replacements == 0 && fs.closes == 1);
  fs.shortRead = false; update("a"); update("a");
  assert(page->replacements == 1 && fs.opens == 3 && fs.closes == 2);
 } else if (mode == "level-change") {
  fs.files["maps/a.txt"] = "map A text!!";
  fs.files["maps/b.txt"] = "map B text!!";
  update("a"); update("b"); update("a");
  TeamFortressViewport viewport; viewport.m_pTeamMenu = &menu;
  viewport.OnLevelChange();
  update("a"); update("a");
  assert(fs.opens == 4 && fs.closes == 4);
 } else if (mode == "activation") {
  fs.files["maps/default.txt"] = "default info";
  update("a");
  fs.files["maps/a.txt"] = "map A text!!";
  update("a"); menu.Activate();
  assert(page->text == "map A text!!");
  fs.failOpen = true; menu.Activate();
  fs.failOpen = false; fs.files["maps/a.txt"] = "new A text!!";
  update("a"); update("a");
  assert(page->text == "new A text!!" && fs.opens == 4 && fs.closes == 3);
 } else if (mode == "unicode") {
  UNICODE_CHECKS
 } else { return 2; }
 assert(fs.closes == static_cast<int>(fs.handles.size()));
 return 0;
}
'''.replace('UNICODE_CHECKS','\n  '.join(unicode_checks))
cpp=a.evidence/'actual_page.cpp'
cpp.write_text(prefix+'\n\n'+'\n\n'.join(methods)+'\n'+main)
exe=a.evidence/'actual_page'
command=['clang++','-std=c++17','-fsanitize=address,undefined','-fno-omit-frame-pointer','-g','-I',str(stubs),'-I',str(header.parent),'-I',str(a.root/'src/common'),str(cpp),'-o',str(exe)]
build=subprocess.run(command,capture_output=True,text=True)
(a.evidence/'build.log').write_text(build.stdout+build.stderr)
assert build.returncode==0,build.stderr
results=[]
for mode in ['repeated','empty-fallback','retry','level-change','activation','unicode']:
 r=subprocess.run([str(exe),mode],capture_output=True,text=True)
 results.append({'case':mode,'exit':r.returncode,'output':r.stdout+r.stderr})
report={'sources':{str(x.relative_to(a.root)):hashlib.sha256(x.read_bytes()).hexdigest() for x in [source,viewport,header]},'compile_command':command,'results':results}
(a.evidence/'result.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(results,indent=2))
raise SystemExit(any(r['exit'] for r in results))
