#!/usr/bin/env python3
"""Actual labeled-combo selection methods and CUtlVector, with controlled UI IDs."""
from pathlib import Path
import argparse,hashlib,importlib.util,json,subprocess
p=argparse.ArgumentParser();p.add_argument('--root',type=Path,required=True);p.add_argument('--evidence',type=Path,required=True)
a=p.parse_args();a.root=a.root.resolve();a.evidence.mkdir(parents=True,exist_ok=False)
spec=importlib.util.spec_from_file_location('extract',a.root/'tests/native/network_platform/extract_functions.py')
m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
source=a.root/'src/GameUI/LabeledCommandComboBox.cpp';text=source.read_text(encoding='cp1252')
names=['DeleteAllItems','HasItem','AddItem','ActivateItem','SetInitialItem','GetActiveItemCommand','ApplyChanges','HasBeenModified','Reset']
bodies=[]
for name in names:
 if name == 'HasItem':
  signature='bool CLabeledCommandComboBox::HasItem( const char* text ) const'
  plain=signature.removesuffix(' const')
  body=m.extract(text.replace(signature,plain),'CLabeledCommandComboBox::'+name)
  bodies.append(body.replace(plain,signature,1))
 else:
  bodies.append(m.extract(text,'CLabeledCommandComboBox::'+name))
methods='\n\n'.join(bodies)
stubs=a.evidence/'stubs';stubs.mkdir()
(stubs/'ComboBox.h').write_text('''#pragma once
#define DECLARE_CLASS_SIMPLE(name, parent) using BaseClass = parent
#define MESSAGE_FUNC_CHARPTR(name, message, argument) void name(const char* argument)
namespace vgui2 {
class Panel {};
class ComboBox {
public:
    inline static int active = -1;
    inline static int activations = 0;
    inline static int nextItem = 11;
    ComboBox(Panel*, const char*, int, bool) {}
    virtual ~ComboBox() = default;
    virtual void DeleteAllItems() {}
    virtual void ActivateItem(int item) {
        active = item;
        ++activations;
    }
    int AddItem(const char*, void*) {
        constexpr int ItemIdStride = 7;
        int item = nextItem;
        nextItem += ItemIdStride;
        return item;
    }
};
}
''')
prefix='''
#include <cassert>
#include <climits>
#include <cstring>
#include "cse_assert.h"
#include "LabeledCommandComboBox.h"
#define ASSERT_AT(condition, line) assert(condition)
namespace vgui2 {
struct Localize {
    wchar_t* Find(const char*) { return nullptr; }
    void ConvertUnicodeToANSI(const wchar_t*, char*, int) {}
};
Localize* localize() {
    static Localize value;
    return &value;
}
}
struct Engine {
    void (*pfnClientCmd)(const char*);
};
static const char* applied = nullptr;
static void ApplyCommand(const char* command) {
    applied = command;
}
static Engine engineObject{ApplyCommand};
static Engine* engine = &engineObject;
CLabeledCommandComboBox::CLabeledCommandComboBox(vgui2::Panel* parent, const char* name)
    : BaseClass(parent, name, 6, false), m_iCurrentSelection(-1), m_iStartSelection(-1) {}
CLabeledCommandComboBox::~CLabeledCommandComboBox() = default;
'''
main='''
int main(int argc, char** argv) {
    assert(argc == 2);
    CLabeledCommandComboBox combo(nullptr, "fixture");
    combo.AddItem("Camera", "camera");
    combo.AddItem("Mode", "mode");
    constexpr int FirstItemId = 11;
    constexpr int SecondItemId = 18;
    if (std::strcmp(argv[1], "negative") == 0) {
        combo.SetInitialItem(-1);
        combo.ActivateItem(-1);
        assert(combo.GetActiveItemCommand() == nullptr);
        assert(vgui2::ComboBox::activations == 0);
    } else if (std::strcmp(argv[1], "item-id") == 0) {
        combo.SetInitialItem(1);
        assert(vgui2::ComboBox::active == SecondItemId);
        assert(std::strcmp(combo.GetActiveItemCommand(), "mode") == 0);
        assert(!combo.HasBeenModified());
    } else if (std::strcmp(argv[1], "deleted") == 0) {
        combo.SetInitialItem(0);
        combo.ActivateItem(0);
        combo.DeleteAllItems();
        assert(combo.GetActiveItemCommand() == nullptr);
        int calls = vgui2::ComboBox::activations;
        combo.Reset();
        assert(vgui2::ComboBox::activations == calls);
        combo.AddItem("New", "new");
        assert(combo.GetActiveItemCommand() == nullptr);
        assert(!combo.HasBeenModified());
    } else if (std::strcmp(argv[1], "upper") == 0) {
        combo.ActivateItem(0);
        combo.SetInitialItem(2);
        combo.ActivateItem(INT_MAX);
        assert(vgui2::ComboBox::active == FirstItemId);
        assert(std::strcmp(combo.GetActiveItemCommand(), "camera") == 0);
    } else if (std::strcmp(argv[1], "reset-apply") == 0) {
        combo.SetInitialItem(1);
        combo.ActivateItem(0);
        assert(combo.HasBeenModified());
        combo.Reset();
        assert(vgui2::ComboBox::active == SecondItemId);
        assert(!combo.HasBeenModified());
        combo.ActivateItem(0);
        combo.ApplyChanges();
        assert(std::strcmp(applied, "camera") == 0);
        assert(!combo.HasBeenModified());
    } else {
        return 2;
    }
    return 0;
}
'''
cpp=a.evidence/'actual_combo.cpp';cpp.write_text(prefix+methods+'\n'+main);exe=a.evidence/'actual_combo'
command=['clang++','-std=c++17','-DUTL_ENGINE_ASSERTS=1','-fsanitize=address,undefined','-fno-omit-frame-pointer','-g','-I',str(stubs),'-I',str(source.parent),'-I',str(a.root/'src/engine'),'-I',str(a.root/'src/public'),str(cpp),'-o',str(exe)]
b=subprocess.run(command,capture_output=True,text=True);(a.evidence/'build.log').write_text(b.stdout+b.stderr);assert b.returncode==0,b.stderr
results=[]
for mode in ['negative','item-id','deleted','upper','reset-apply']:
 r=subprocess.run([str(exe),mode],capture_output=True,text=True);results.append({'case':mode,'exit':r.returncode,'output':r.stdout+r.stderr})
(a.evidence/'result.json').write_text(json.dumps({'source_sha256':hashlib.sha256(source.read_bytes()).hexdigest(),'command':command,'results':results},indent=2)+'\n')
print(json.dumps(results,indent=2));raise SystemExit(any(r['exit'] for r in results))
