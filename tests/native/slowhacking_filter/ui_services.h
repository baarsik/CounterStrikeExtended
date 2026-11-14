// Only external filesystem/widget services are controlled; option ownership and serialization are production code.
#include "ScriptObject.h"
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
#include <vector>
#define stricmp strcasecmp
#define ASSERT_AT(condition, line) assert(condition)

struct EngineServices {
    std::map<std::string, std::string> values;
    std::vector<std::string> commands;
    const char* pfnGetCvarString(const char* name) {
        return values[name].c_str();
    }
    const char* LocalPlayerInfo_ValueForKey(const char* name) {
        return pfnGetCvarString(name);
    }
    void pfnClientCmd(const char* command) {
        commands.emplace_back(command);
    }
};
static EngineServices engineServices;
static EngineServices* engine = &engineServices;
struct FileSystemServices {
    FileHandle_t Open(const char*, const char*, const char*) {
        return nullptr;
    }
    int Size(FileHandle_t) {
        return 0;
    }
    void Read(void*, int, FileHandle_t) {}
    void Close(FileHandle_t) {}
};
namespace vgui2 {
static FileSystemServices* filesystem() {
    static FileSystemServices services;
    return &services;
}
}
struct ModServices {
    const char* description = "Counter-Strike";
    const char* GetGameDescription() {
        return description;
    }
};
static ModServices mod;
static ModServices* ModInfo() { return &mod; }
static bool savedScriptLoaded = true;
CDescription::CDescription(CPanelListPanel* panel)
    : pObjList(nullptr), m_pszHintText(nullptr), m_pszDescriptionType(nullptr), m_pListPanel(panel) {}
CDescription::~CDescription() {
    while (pObjList) {
        CScriptObject* object = pObjList;
        pObjList = object->pNext;
        delete object;
    }
}
bool CDescription::InitFromFile(char*) {
    return savedScriptLoaded;
}
bool CDescription::ReadFromBuffer(char**) {
    return false;
}
