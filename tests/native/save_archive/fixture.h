#pragma once
// Labelled in-memory filesystem leaves. Production owner bodies remain unmodified.
#include <algorithm>
#include <cctype>
#include <climits>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#ifdef _WIN32
// Host_Load's real fail-fast branch needs the SDK constant and compiler intrinsic.
// Preserve the fixture's independently selected 260/1024 host path width.
#pragma push_macro("MAX_PATH")
#pragma push_macro("NOMINMAX")
#pragma push_macro("WIN32_LEAN_AND_MEAN")
#pragma push_macro("FALSE")
#pragma push_macro("TRUE")
#pragma push_macro("ARRAYSIZE")
#undef MAX_PATH
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <intrin.h>
#pragma pop_macro("ARRAYSIZE")
#pragma pop_macro("TRUE")
#pragma pop_macro("FALSE")
#pragma pop_macro("WIN32_LEAN_AND_MEAN")
#pragma pop_macro("NOMINMAX")
#pragma pop_macro("MAX_PATH")
#else
#define __cdecl
#endif
#define _snprintf std::snprintf
#define FILE_COPY_SIZE 1024
#define Q_strstr std::strstr
#define Q_strlen std::strlen
#define Q_strncpy std::strncpy
#define Q_strcasecmp strcmp
#define Q_stricmp strcmp
#define Q_atoi std::atoi
#define FALSE 0
#define TRUE 1
#define STATE_ENDTRAINING 1
#define SAVEGAME_HEADER 0x5641534a
#define SAVEGAME_VERSION 0x71
#define ARRAYSIZE(x) (sizeof(x) / sizeof((x)[0]))
using Bytes = std::vector<unsigned char>;
struct MemoryFile {
    Bytes data;
    unsigned int position = 0;
    bool closed = false;
};
using FileHandle_t = MemoryFile *;
enum FileSystemSeek_t {
    FILESYSTEM_SEEK_HEAD,
    FILESYSTEM_SEEK_CURRENT
};
inline std::map<std::string, MemoryFile> files;
inline std::vector<std::unique_ptr<char[]>> findNames;
inline std::size_t findIndex = 0;
inline int outputOpens = 0;
inline bool failOpen = false;
inline bool failSeek = false;
inline int readBudget = -1;
inline int writeBudget = -1;
inline bool wrongReadPosition = false;
inline bool wrongWritePosition = false;
inline unsigned int fakeSize = 0;
struct ReadObservation {
    unsigned int position;
    int requested;
    int actual;
    int outputs;
};
struct SeekObservation {
    unsigned int before;
    unsigned int target;
    int outputs;
};
inline std::vector<ReadObservation> readObservations;
inline std::vector<SeekObservation> seekObservations;
inline int failReadCall = -1;
inline int wrongReadCall = -1;
inline int failSeekCall = -1;
inline void reset() {
    files.clear();
    findNames.clear();
    findIndex = 0;
    outputOpens = 0;
    failOpen = false;
    failSeek = false;
    readBudget = -1;
    writeBudget = -1;
    wrongReadPosition = false;
    wrongWritePosition = false;
    fakeSize = 0;
    readObservations.clear();
    seekObservations.clear();
    failReadCall = -1;
    wrongReadCall = -1;
    failSeekCall = -1;
}
inline void check(bool value, const char *message) {
    if (!value) {
        throw std::runtime_error(message);
    }
}
inline void addName(const std::string &name, std::size_t allocated = 0) {
    auto buffer = std::make_unique<char[]>(std::max(name.size() + 1, allocated));
    std::memset(buffer.get(), 0, std::max(name.size() + 1, allocated));
    std::memcpy(buffer.get(), name.c_str(), name.size() + 1);
    findNames.push_back(std::move(buffer));
}
inline const char *Sys_FindFirst(const char *, char *) {
    findIndex = 0;
    return findNames.empty() ? nullptr : findNames.front().get();
}
inline const char *Sys_FindNext(char *) {
    return ++findIndex < findNames.size() ? findNames[findIndex].get() : nullptr;
}
inline void Sys_FindClose() {}
inline char *Host_SaveGameDirectory() {
    static char directory[] = "SAVE/";
    return directory;
}
inline void COM_FixSlashes(char *) {}
inline FileHandle_t FS_OpenPathID(const char *path, const char *mode, const char *) {
    if (failOpen) {
        return nullptr;
    }
    if (mode[0] == 'w') {
        ++outputOpens;
        files[path] = {};
    }
    auto found = files.find(path);
    if (found == files.end()) {
        return nullptr;
    }
    found->second.position = 0;
    found->second.closed = false;
    return &found->second;
}
inline void FS_Close(FileHandle_t file) {
    if (file) {
        file->closed = true;
    }
}
inline unsigned int FS_Tell(FileHandle_t file) {
    return file ? file->position : 0;
}
inline unsigned int FS_Size(FileHandle_t file) {
    return fakeSize ? fakeSize : file ? static_cast<unsigned int>(file->data.size()) : 0;
}
inline void FS_Seek(FileHandle_t file, int offset, FileSystemSeek_t type) {
    if (!file || offset < 0) {
        return;
    }
    const auto target = type == FILESYSTEM_SEEK_HEAD ? static_cast<unsigned int>(offset) : file->position + offset;
    const auto call = static_cast<int>(seekObservations.size());
    seekObservations.push_back({file->position, target, outputOpens});
    if (!failSeek && call != failSeekCall) {
        file->position = target;
    }
}
inline int FS_Read(void *output, int size, int count, FileHandle_t file) {
    if (!file || size < 0 || count != 1 || file->position > file->data.size()) {
        return 0;
    }
    int actual = static_cast<int>(std::min<std::size_t>(size, file->data.size() - file->position));
    const auto call = static_cast<int>(readObservations.size());
    if (call == failReadCall && actual != 0) {
        --actual;
    }
    if (readBudget >= 0) {
        actual = std::min(actual, readBudget);
        readBudget -= actual;
    }
    if (actual != 0) {
        std::memcpy(output, file->data.data() + file->position, actual);
    }
    readObservations.push_back({file->position, size, actual, outputOpens});
    if (!wrongReadPosition && call != wrongReadCall) {
        file->position += actual;
    }
    return actual;
}
inline int FS_Write(const void *input, int size, int count, FileHandle_t file) {
    if (!file || size < 0 || count != 1) {
        return 0;
    }
    int actual = size;
    if (writeBudget >= 0) {
        actual = std::min(actual, writeBudget);
        writeBudget -= actual;
    }
    file->data.resize(std::max(file->data.size(), static_cast<std::size_t>(file->position) + actual));
    if (actual) {
        std::memcpy(file->data.data() + file->position, input, actual);
    }
    if (!wrongWritePosition) {
        file->position += actual;
    }
    return actual;
}
inline char *COM_LastFileExtension(char *name) {
    auto *dot = std::strrchr(name, '.');
    return dot ? dot + 1 : nullptr;
}
inline const char *COM_FileExtension(const char *name) {
    auto *dot = std::strrchr(name, '.');
    return dot ? dot + 1 : nullptr;
}
inline void COM_DefaultExtension(char *name, const char *extension) {
    if (!COM_FileExtension(name)) {
        std::strcat(name, extension);
    }
}
// Independent, manually placed disk fields. No production encoding helpers are used.
inline Bytes entry(std::size_t width, const std::string &name, const Bytes &payload, unsigned char padding = 0) {
    Bytes result(width, padding);
    std::copy(name.begin(), name.end(), result.begin());
    result[name.size()] = 0;
    result.push_back(static_cast<unsigned char>(payload.size()));
    result.push_back(static_cast<unsigned char>(payload.size() >> 8));
    result.push_back(static_cast<unsigned char>(payload.size() >> 16));
    result.push_back(static_cast<unsigned char>(payload.size() >> 24));
    result.insert(result.end(), payload.begin(), payload.end());
    return result;
}
inline Bytes archive(std::size_t width, unsigned char padding = 0) {
    Bytes result = entry(width, "first.HL1", {0x11, 0x22, 0x33}, padding);
    auto empty = entry(width, "empty.HL2", {}, padding);
    auto last = entry(width, "last.HL3", {0xfe, 0, 0xed}, padding);
    result.insert(result.end(), empty.begin(), empty.end());
    result.insert(result.end(), last.begin(), last.end());
    return result;
}
#ifndef ARCHIVE_OLD
struct GAME_HEADER {
    int mapCount = 3;
    char mapName[32] = "test";
    char comment[80]{};
};
struct SAVERESTOREDATA {
    char *pCurrentData;
    char **pTokens = nullptr;
    int tokenCount = 0;
    int size = 0;
    int bufferSize = 256;
    int tokenSize = 0;
    char *pBaseData;
};
inline char arena[256]{};
inline SAVERESTOREDATA saveData{arena, nullptr, 0, 0, 256, 0, arena};
inline SAVERESTOREDATA *SaveGamestate() {
    return &saveData;
}
inline SAVERESTOREDATA *SaveInit(int) {
    saveData = {arena, nullptr, 0, 0, 256, 0, arena};
    return &saveData;
}
inline void SaveExit(SAVERESTOREDATA *) {}
inline int declaredCount = 3;
inline int DirectoryCount(const char *) {
    return declaredCount;
}
inline void FS_CreateDirHierarchy(const char *, const char *) {}
inline void fields(SAVERESTOREDATA *, const char *, GAME_HEADER *, int *, int) {}
inline void globals(SAVERESTOREDATA *) {}
inline struct {
    decltype(&fields) pfnSaveWriteFields = fields;
    decltype(&globals) pfnSaveGlobalState = globals;
} gEntityInterface;
inline int gGameHeaderDescription[3]{};
inline struct {
    char name[32] = "test";
} sv;
inline struct {
    bool dll_initialized = true;
    int maxclients = 1;
} svs;
inline struct {
    int demonum = 0;
} cls;
inline int giStateInfo = 0;
inline char com_gamedir[] = "valve";
inline int mapped = 0;
inline void Host_SaveAgeList(const char *, int) {}
inline void Host_InitializeGameDLL() {}
inline void Host_ClearGameState() {}
inline void Host_Map(int, char *, char *, int) {
    ++mapped;
}
inline void SV_InactivateClients() {}
inline void SCR_BeginLoadingPlaque(int) {}
inline void CL_Disconnect() {}
inline void Cbuf_AddText(const char *) {}
inline void Cvar_SetValue(const char *, int) {}
inline void Con_Printf(const char *, ...) {}
inline void Con_DPrintf(const char *, ...) {}
inline int SaveReadHeader(FileHandle_t file, GAME_HEADER *header, int) {
    // Only the envelope/global-state leaves are stubbed; Host_Load's entire archive body is real.
    header->mapCount = declaredCount;
    return FS_Tell(file) == 0;
}
#endif
