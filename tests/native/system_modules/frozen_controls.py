"""Run frozen pre-repair production regions; failures are controlled, not UB crashes."""
from pathlib import Path
import subprocess, tempfile, hashlib
root = Path(__file__).resolve().parents[3]
base = '42744025bcb1a874d72dedbfce04ced8722caeaf'
def function(text, signature):
    start = text.index(signature)
    end = text.index('{', start)
    depth = 1
    end += 1
    while depth:
        depth += (text[end] == '{') - (text[end] == '}')
        end += 1
    return text[start:end]
frozen = Path(__file__).with_name('frozen')
add = (frozen/'add.cpp').read_text()
get = (frozen/'get.cpp').read_text()
init = (frozen/'init.cpp').read_text()
preamble = r'''
#include <cstddef>
#include <cstdio>
#include <vector>
struct SystemWrapper;
struct ISystemModule {
 int serial = -1; bool fail = false; ISystemModule* child = nullptr;
 bool Init(SystemWrapper*, int, char*);
};
struct List { std::vector<ISystemModule*> entries; void Init() { entries.clear(); } void AddHead(ISystemModule* p) { entries.push_back(p); } };
ISystemModule bad;
void* factory(const char*, int*) { return &bad; }
struct IBaseSystem {};
struct BaseSystemModule { bool Init(IBaseSystem*, int, char*) { return true; } };
struct EngineWrapper : ISystemModule {};
struct SystemWrapper : BaseSystemModule {
 bool Init(IBaseSystem*, int, char*);
 List m_Commands, m_Libraries, m_Listener;
 ISystemModule* m_EngineWrapper = nullptr;
 int m_State = 0;
 enum { MODULE_RUNNING = 3 };
 struct library_t { const char* name = "fixture"; void* (*createInterfaceFn)(const char*, int*) = factory; } library;
 List m_Modules; unsigned int m_SerialCounter = 1;
 void Printf(const char*, ...) {}
 ISystemModule* FindModule(char*, char*) { return nullptr; }
 library_t* GetLibrary(char*) { return &library; }
 bool AddModule(ISystemModule*, char*);
 ISystemModule* GetModule(char*, char*, char*);
};
bool ISystemModule::Init(SystemWrapper* s, int value, char*) {
 serial = value; if(child) { s->AddModule(child, nullptr); } return !fail;
}
'''
main = r'''
int main() {
 SystemWrapper s;
 s.Init(nullptr, 0, nullptr);
#ifndef _WIN32
 if(!s.m_Modules.entries.empty()) { return 3; }
#endif
 delete s.m_EngineWrapper;
 ISystemModule parent; ISystemModule child; parent.child = &child;
 s.AddModule(&parent, nullptr);
 if(parent.serial != child.serial) { return 1; }
 bad.fail = true;
 if(!s.GetModule(const_cast<char*>("bad"), const_cast<char*>("fixture"), nullptr)) { return 2; }
 std::puts("Frozen controls reproduced missing native engine registration, duplicate serials and failed-Init acquisition");
 return 0;
}
'''
with tempfile.TemporaryDirectory() as path:
    cpp = Path(path)/'frozen.cpp'; cpp.write_text(preamble+add+'\n'+get+'\n'+init+main)
    subprocess.run(['c++','-std=c++17','-fno-rtti',str(cpp),'-o',path+'/frozen'],check=True)
    subprocess.run([path+'/frozen'],check=True)
