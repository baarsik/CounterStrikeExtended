#include <algorithm>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <strings.h>
#include <utility>
#include <vector>
#include "interface_language.h"

std::string savedPreference;
namespace cse {
namespace interface_language {
std::string ReadSavedPreference() {
    return savedPreference;
}
}
}

constexpr int APPID_HALF_LIFE = 70;
const char* currentLanguage = "english";
bool steamAvailable = true;
struct SteamApplication {
    const char* GetCurrentGameLanguage() {
        return currentLanguage;
    }
    bool BIsLowViolence() {
        return false;
    }
} application;
SteamApplication* SteamApps() {
    return steamAvailable ? &application : nullptr;
}
int GetGameAppID() {
    return APPID_HALF_LIFE;
}
void SteamAPI_SetBreakpadAppID(int) {}
bool IsGameSubscribed(const char*) {
    return true;
}
bool BEnabledHDAddon() {
    return false;
}
bool BEnableAddonsFolder() {
    return false;
}
bool bLowViolenceBuild = false;
bool g_bIsDedicatedServer = false;
int com_argc = 0;
char** com_argv = nullptr;
int COM_CheckParm(const char* name) {
    for (int index = 1; index < com_argc; ++index) {
        if (std::strcmp(com_argv[index], name) == 0) {
            return index;
        }
    }
    return 0;
}
void COM_FixSlashes(char* path) {
    for (char* cursor = path; *cursor; ++cursor) {
        if (*cursor == '\\') {
            *cursor = '/';
        }
    }
}
#define Q_strncpy std::strncpy
#define Q_strstr std::strstr
#define _stricmp strcasecmp
char s_pBaseDir[512] = "/runtime";
struct Filesystem {
    std::vector<std::pair<std::string, std::string>> paths;
    void RemoveAllSearchPaths() {
        paths.clear();
    }
    void AddSearchPathNoWrite(const char* path, const char* id) {
        paths.emplace_back(path, id);
    }
    void AddSearchPath(const char* path, const char* id) {
        paths.emplace_back(path, id);
    }
} filesystem;
Filesystem* g_pFileSystem = &filesystem;
std::string liblistLanguage;
void CheckLiblistForFallbackDir(const char*, bool localized, const char* language, bool) {
    liblistLanguage = localized ? language : "english";
}
#include "filesystem.inc"
class BaseUISurface {
public:
    char m_szLanguage[32]{};
    void SetLanguage(const char*);
    const char* GetLanguage() {
        return m_szLanguage;
    }
    void SetEmbeddedPanel(int) {}
    void InitLanguage(int);
};
#include "surface.inc"
struct System {
    int lastCapacity = 0;
    bool oversized = false;
    bool GetCommandLineParamValue(const char* name, char* output, int capacity) {
        lastCapacity = capacity;
        if (oversized) {
            std::memset(output, 'a', static_cast<std::size_t>(capacity));
            return true;
        }
        const int option = COM_CheckParm(name);
        if (!option || option + 1 >= com_argc) {
            return false;
        }
        std::strncpy(output, com_argv[option + 1], static_cast<std::size_t>(capacity - 1));
        output[capacity - 1] = '\0';
        return true;
    }
} commandSystem;
BaseUISurface dedicatedSurface;
namespace vgui2 {
BaseUISurface* surface() {
    return &dedicatedSurface;
}
System* system() {
    return &commandSystem;
}
}
#include "dedicated.inc"

void Require(bool result, const char* message) {
    if (!result) {
        std::fprintf(stderr, "FAIL: %s\n", message);
        std::exit(1);
    }
}
void CheckSelection(const std::vector<std::string>& arguments, const char* steam, const std::string& expected,
    const std::string& saved = {}) {
    savedPreference = saved;
    std::vector<char*> argv;
    for (const auto& argument : arguments) {
        argv.push_back(const_cast<char*>(argument.c_str()));
    }
    com_argc = static_cast<int>(argv.size());
    com_argv = argv.data();
    currentLanguage = steam;
    Require(FileSystem_SetGameDirectory("valve", "cstrike") == 1, "configure real root search paths");
    const auto contains = [&](const std::string& path, const std::string& id) {
        return std::find(filesystem.paths.begin(), filesystem.paths.end(), std::make_pair(path, id)) !=
            filesystem.paths.end();
    };
    if (expected != "english") {
        Require(contains("/runtime/cstrike_" + expected, "GAME"), "ROOT game uses selected language");
        Require(contains("/runtime/valve_" + expected, "DEFAULTGAME"), "ROOT default game agrees");
    } else {
        Require(!contains("/runtime/cstrike_english", "GAME"), "English uses original root paths");
    }
    Require(liblistLanguage == expected, "recursive liblist fallback receives the same language");
    filesystem.paths.clear();
    FileSystem_AddFallbackGameDir("gearbox");
    if (expected != "english") {
        Require(contains("/runtime/gearbox_" + expected, "GAME"), "FALLBACK game agrees with ROOT");
    } else {
        Require(filesystem.paths.size() == 1, "English fallback retains original path");
    }
    BaseUISurface surface;
    surface.InitLanguage(0);
    Require(surface.m_szLanguage == expected, "UI agrees with ROOT and FALLBACK language");
    for (auto initialize : {ApplyNativeDedicatedLanguage, ApplyWindowsDedicatedLanguage}) {
        dedicatedSurface.SetLanguage(steam);
        initialize();
        Require(dedicatedSurface.m_szLanguage == expected, "dedicated consumers use the same validated selection");
        Require(commandSystem.lastCapacity == 128, "dedicated command reader uses bounded 128-byte buffer");
    }
}
int main() {
    CheckSelection({"game"}, "german", "russian", "russian");
    CheckSelection({"game"}, "german", "polish", "PoLiSh");
    CheckSelection({"game", "-language", "english"}, "german", "english", "russian");
    CheckSelection({"game", "-language", "custom_language"}, "german", "custom_language", "polish");
    CheckSelection({"game", "-language", "../invalid"}, "german", "russian", "russian");
    CheckSelection({"game"}, "german", "german", "");
    for (const std::string& invalid : std::vector<std::string>{"../russian", "russian/polish", "custom_language",
        "ru ssian", "русский", std::string(32, 'a'), std::string(128, 'a')}) {
        CheckSelection({"game"}, "german", "german", invalid);
    }
    CheckSelection({"game", "-language", "russian"}, "english", "russian");
    CheckSelection({"game", "-language", "PoLiSh"}, "german", "polish");
    CheckSelection({"game", "-language", "english"}, "german", "english");
    CheckSelection({"game", "-language", "test_language2"}, "french", "test_language2");
    CheckSelection({"game"}, "GERMAN", "german");
    CheckSelection({"game", "-language"}, "german", "german");
    const std::vector<std::string> invalidValues{
        "", "../russian", "russian/polish", "russian\\polish", "ru.ssian", "-console",
        "+map", "ru ssian", "русский", std::string(32, 'a')};
    for (const std::string& invalid : invalidValues) {
        CheckSelection({"game", "-language", invalid}, "german", "german");
    }
    CheckSelection({"game", "-language", std::string(31, 'A')}, "english", std::string(31, 'a'));
    CheckSelection({"game"}, nullptr, "english");
    CheckSelection({"game", "-language", "-console"}, nullptr, "english");
    CheckSelection({"game"}, "../invalid", "english");
    steamAvailable = false;
    CheckSelection({"game", "-language", "russian"}, nullptr, "russian");
    CheckSelection({"game"}, nullptr, "english");
    steamAvailable = true;
    Require(cse::interface_language::ResolveFromSystem("french", static_cast<System*>(nullptr)) == "french",
        "unavailable command service preserves current language");
    char unterminated[32];
    std::memset(unterminated, 'a', sizeof(unterminated));
    BaseUISurface surface;
    surface.SetLanguage(unterminated);
    Require(std::strcmp(surface.GetLanguage(), "english") == 0, "nonterminated full field rejects without overread");
    savedPreference = std::string(128, 'a');
    dedicatedSurface.SetLanguage("french");
    ApplyNativeDedicatedLanguage();
    Require(std::strcmp(dedicatedSurface.GetLanguage(), "french") == 0,
        "overlong dedicated saved value preserves current language");
    savedPreference.clear();
    commandSystem.oversized = true;
    dedicatedSurface.SetLanguage("french");
    ApplyNativeDedicatedLanguage();
    Require(std::strcmp(dedicatedSurface.GetLanguage(), "french") == 0,
        "overlong dedicated option preserves current language");
    std::puts("PASS: ROOT/FALLBACK/UI validated language agreement");
}
