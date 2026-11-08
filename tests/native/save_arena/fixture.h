#pragma once

#include <algorithm>
#include <climits>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iterator>
#include <map>
#include <vector>

typedef float vec_t;
typedef float vec3_t[3];
#include "../../../GoldSrc-SDK/common/const.h"
#include "../../../GoldSrc-SDK/engine/progdefs.h"
#include "../../../GoldSrc-SDK/engine/edict.h"
#include "../../../GoldSrc-SDK/engine/eiface.h"

namespace {
constexpr std::int32_t SAVEFILE_HEADER = INT32_C(0x564c4156); // Legacy VALV file envelope.
constexpr std::int32_t SAVEGAME_VERSION = INT32_C(0x71);
constexpr std::size_t HeaderIntegers = 6;
constexpr std::size_t HeaderBytes = HeaderIntegers * sizeof(std::int32_t);
constexpr std::size_t FixtureAllocationLimit = 32 * 1024 * 1024;
#if !defined(MAX_PATH)
constexpr std::size_t MAX_PATH = 260;
#endif
struct MemoryFile {
    std::vector<unsigned char> bytes;
    std::size_t cursor = 0;
    unsigned int opens = 0;
    unsigned int closes = 0;
    int reads = 0;
    int shortReadAt = -1;
};
using FileHandle_t = MemoryFile *;
MemoryFile input;
std::map<void *, std::size_t> allocations;
int failAllocation = -1;
int allocationAttempt = 0;
globalvars_t gGlobalVariables{};

void Con_Printf(const char *, ...) {}
const char *Host_SaveGameDirectory() {
    return "owned/";
}
void COM_FixSlashes(char *) {}
void Q_strncpy(char *output, const char *text, std::size_t bytes) {
    std::strncpy(output, text, bytes);
}
void VectorClear(float *value) {
    value[0] = 0.0f;
    value[1] = 0.0f;
    value[2] = 0.0f;
}
FileHandle_t FS_OpenPathID(const char *, const char *, const char *) {
    ++input.opens;
    input.cursor = 0;
    return &input;
}
void FS_Close(FileHandle_t file) {
    ++file->closes;
}
unsigned int FS_Size(FileHandle_t file) {
    return static_cast<unsigned int>(file->bytes.size());
}
unsigned int FS_Tell(FileHandle_t file) {
    return static_cast<unsigned int>(file->cursor);
}
int FS_Read(void *output, int size, int count, FileHandle_t file) {
    if (size < 0 || count != 1) {
        std::abort();
    }
    auto bytes = (std::min)(static_cast<std::size_t>(size), file->bytes.size() - file->cursor);
    if (file->reads++ == file->shortReadAt && bytes != 0) {
        --bytes;
    }
    if (bytes != 0) {
        std::memcpy(output, file->bytes.data() + file->cursor, bytes);
    }
    file->cursor += bytes;
    return static_cast<int>(bytes);
}
void *Mem_Calloc(int count, std::size_t size) {
    if (allocationAttempt++ == failAllocation || count < 0
        || (size != 0 && static_cast<std::size_t>(count) > FixtureAllocationLimit / size)) {
        return nullptr;
    }
    const auto bytes = static_cast<std::size_t>(count) * size;
    void *memory = std::calloc(1, bytes);
    if (memory) {
        allocations[memory] = bytes;
    }
    return memory;
}
void Mem_Free(void *memory) {
    if (memory) {
        if (allocations.erase(memory) != 1) {
            std::abort();
        }
        std::free(memory);
    }
}
void FreeSave(SAVERESTOREDATA *save) {
    if (save) {
        Mem_Free(save->pTokens);
        Mem_Free(save);
    }
    gGlobalVariables.pSaveData = nullptr;
}
} // namespace

#define _snprintf std::snprintf
#include "reader_header.h"
#undef _snprintf
