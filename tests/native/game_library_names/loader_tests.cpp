#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>
#include <strings.h>

// Compile the actual POSIX loader twice. This changes only its platform choice;
// all host headers have already been read. No module or filesystem is accessed.
#if defined(CSE_TEST_LINUX)
#undef __APPLE__
#endif
#include "game_library_name.h"

namespace {
constexpr int MAX_EXT_DLLS = 50;
struct extensiondll_t {
    void* handle;
};
struct ModInfo {
    bool bIsMod;
};
ModInfo gmodinfo{};
extensiondll_t g_rgextdll[MAX_EXT_DLLS]{};
int g_iextdllMac = 0;
bool com_ignorecolons = false;
char com_gamedir[] = "cstrike";
char com_token[1024]{};
int com_argc = 0;
const char* com_argv[3]{};
std::string listing;
std::vector<std::string> queries;
std::vector<std::string> loaded;
std::vector<std::string> metadata;
std::string missing;
std::string wildcardSeen;
int closed = 0;
using FileHandle_t = void*;

void Check(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void SV_ResetModInfo() {
    gmodinfo = {};
}

void Q_strncpy(char* destination, const char* source, std::size_t count) {
    std::strncpy(destination, source, count);
}

int Q_strcasecmp(const char* left, const char* right) {
    return strcasecmp(left, right);
}

void* Q_memset(void* destination, int value, std::size_t count) {
    return std::memset(destination, value, count);
}

std::size_t Q_strlen(const char* value) {
    return std::strlen(value);
}

char* Q_strstr(char* value, const char* part) {
    return std::strstr(value, part);
}

void Q_strcat(char* destination, const char* suffix) {
    std::strcat(destination, suffix);
}

FileHandle_t FS_Open(const char*, const char*) {
    return listing.empty() ? nullptr : &listing;
}

std::size_t FS_Size(FileHandle_t) {
    return listing.size();
}

int FS_Read(void* destination, int size, int, FileHandle_t) {
    std::memcpy(destination, listing.data(), static_cast<std::size_t>(size));
    return size;
}

void FS_Close(FileHandle_t) {
    ++closed;
}

void* Mem_Malloc(int size) {
    return std::malloc(static_cast<std::size_t>(size));
}

[[noreturn]] void Sys_Error(const char*, ...) {
    throw std::runtime_error("Unexpected loader fatal error");
}

// Controlled quoted key/value parser seam; parser behavior is outside this test.
const char* COM_Parse(const char* cursor) {
    com_token[0] = 0;
    if (!cursor) {
        return nullptr;
    }
    while (*cursor && *cursor <= ' ') {
        ++cursor;
    }
    const bool quoted = *cursor == '"';
    if (quoted) {
        ++cursor;
    }
    std::size_t count = 0;
    while (*cursor && (quoted ? *cursor != '"' : *cursor > ' ')) {
        Check(count + 1 < sizeof(com_token), "Fixture token exceeds parser capacity");
        com_token[count++] = *cursor++;
    }
    if (quoted && *cursor == '"') {
        ++cursor;
    }
    com_token[count] = 0;
    return cursor;
}

void DLL_SetModKey(ModInfo*, const char* key, const char* value) {
    metadata.emplace_back(std::string(key) + "=" + value);
}

int COM_CheckParm(const char*) {
    return com_argc == 3 ? 1 : 0;
}

const char* COM_LastFileExtension(const char* value) {
    const char* dot = std::strrchr(value, '.');
    return dot ? dot + 1 : "";
}

const char* FS_GetLocalPath(const char* value, char* destination, int bytes) {
    queries.emplace_back(value);
    if (missing == value) {
        return nullptr;
    }
    std::snprintf(destination, static_cast<std::size_t>(bytes), "/owned/search/%s", value);
    return destination;
}

void Con_DPrintf(const char*, ...) {
}

void LoadThisDll(char* value) {
    loaded.emplace_back(value);
    ++g_iextdllMac;
}

const char* Sys_FindFirst(const char* wildcard, void*) {
    wildcardSeen = wildcard;
    return "hl.native";
}

const char* Sys_FindNext(void*) {
    return nullptr;
}

void Sys_FindClose() {
}

#define _snprintf std::snprintf
#include "actual_loader.inc"

void Run(const std::string& input, const char* overrideName = nullptr) {
    listing = input;
    queries.clear();
    loaded.clear();
    metadata.clear();
    wildcardSeen.clear();
    closed = 0;
    com_argc = overrideName ? 3 : 0;
    com_argv[2] = overrideName;
    char base[] = "/owned/base";
    LoadEntityDLLs(base);
    Check(!com_ignorecolons, "Parser colon state not restored");
    Check(listing.empty() || closed == 1, "Listing handle not closed exactly once");
}
}

int main() {
    try {
        const bool mac = cse::NativeGameLibraryPlatform() == cse::GameLibraryPlatform::MacOS;
        const char* primary = mac ? "gamedll_osx" : "gamedll_linux";
        const char* extension = mac ? ".dylib" : ".so";
        const std::string nativeName = std::string("dlls/cs") + extension;
        const std::string nativeEntry = std::string(primary) + " \"" + nativeName + "\"\n";
        Run("gamedll \"dlls\\mp.dll\"\n" + nativeEntry + "type multiplayer\n");
        Check(queries == std::vector<std::string>{nativeName}, "Explicit native key lost to earlier Windows key");
        Check(loaded == std::vector<std::string>{"/owned/search/" + nativeName}, "VFS resolved path not used");
        Check(std::find(metadata.begin(), metadata.end(), "type=multiplayer") != metadata.end(), "Metadata lost");
        Run(nativeEntry + "gamedll \"dlls\\mp.dll\"\n");
        Check(queries == std::vector<std::string>{nativeName}, "Later Windows key overrides native key");
        Run("gamedll \"dlls\\one.dll\"\ngamedll \"dlls\\two.dll\"\n");
        Check(queries == std::vector<std::string>{std::string("dlls/one") + extension,
            std::string("dlls/two") + extension}, "Fallback order or extension differs");
        Run(nativeEntry, "dlls/custom_mod.dll");
        const std::string overrideName = std::string(mac ? "dlls/custom_mod" : "dlls/custom") + extension;
        Check(queries == std::vector<std::string>{overrideName}, "Command-line override lost");
        Run(std::string(primary) + " dlls/invalid.txt\ngamedll dlls/mp.dll\n");
        Check(queries.empty() && loaded.empty(), "Invalid explicit native key silently falls back");
        for (const char* illegal : {"/absolute.dll", "C:/absolute.dll", "../../two.dll", "\\root.dll"}) {
            Run(std::string("gamedll \"") + illegal + "\"\n");
            Check(queries.empty() && loaded.empty(), "Existing illegal path restriction lost");
        }
        missing = nativeName;
        Run(nativeEntry);
        Check(queries.size() == 1 && loaded.empty(), "Failed VFS resolution passed stale path to loader");
        missing.clear();
        std::string overflow;
        for (int index = 0; index < MAX_EXT_DLLS + 1; ++index) {
            overflow += "gamedll dlls/mod" + std::to_string(index) + ".dll\n";
        }
        Run(overflow);
        Check(queries.size() == MAX_EXT_DLLS, "Fallback storage exceeded existing extension table bound");
        Run("");
        Check(wildcardSeen == std::string("valve/dlls/*") + extension, "Wrong no-liblist wildcard");
        std::puts("PASS actual native metadata selection, override, VFS boundary and bounded fallbacks");
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return EXIT_FAILURE;
    }
}
