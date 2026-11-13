#pragma once

#include <climits>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace {
constexpr std::size_t LegacySaveIntegerBytes = 4;
static_assert(sizeof(int) == LegacySaveIntegerBytes, "Legacy save counts use four-byte signed integers");

bool ReadSaveInteger(FileHandle_t file, std::int32_t &value) {
    unsigned char bytes[LegacySaveIntegerBytes];
    if (FS_Read(bytes, sizeof(bytes), 1, file) != sizeof(bytes)) {
        return false;
    }
    std::uint32_t bits = 0;
    constexpr unsigned int ByteBits = 8;
    for (unsigned int index = 0; index < sizeof(bytes); ++index) {
        bits |= static_cast<std::uint32_t>(bytes[index]) << (index * ByteBits);
    }
    std::memcpy(&value, &bits, sizeof(value));
    return true;
}

SAVERESTOREDATA *AllocateSaveArena(int dataBytes, int tableCount, int tokenCount, int tokenBytes) {
    if (dataBytes < 0 || tableCount < 0 || tokenCount < 0 || tokenBytes < 0) {
        return nullptr;
    }
    // These are native arena sizes. Disk files contain symbolic ETABLE fields, never these rows.
    constexpr std::size_t MaximumAllocationBytes = INT_MAX;
    constexpr std::size_t TableAlignment = alignof(ENTITYTABLE);
    constexpr std::size_t TableOffset =
        (sizeof(SAVERESTOREDATA) + TableAlignment - 1) / TableAlignment * TableAlignment;
    if (static_cast<std::size_t>(tableCount) > (MaximumAllocationBytes - TableOffset) / sizeof(ENTITYTABLE)
        || static_cast<std::size_t>(tokenCount) > MaximumAllocationBytes / sizeof(char *)) {
        return nullptr;
    }
    const auto tableBytes = static_cast<std::size_t>(tableCount) * sizeof(ENTITYTABLE);
    const auto prefixBytes = TableOffset + tableBytes;
    if (static_cast<std::size_t>(tokenBytes) > MaximumAllocationBytes - prefixBytes
        || static_cast<std::size_t>(dataBytes) > MaximumAllocationBytes - prefixBytes - tokenBytes) {
        return nullptr;
    }
    auto *save = static_cast<SAVERESTOREDATA *>(Mem_Calloc(1, prefixBytes + tokenBytes + dataBytes));
    if (!save) {
        return nullptr;
    }
    auto *tokenList = reinterpret_cast<char *>(save) + prefixBytes;
    save->pTable = reinterpret_cast<ENTITYTABLE *>(reinterpret_cast<char *>(save) + TableOffset);
    save->tableCount = tableCount;
    save->tokenCount = tokenCount;
    save->tokenSize = tokenBytes;
    save->bufferSize = dataBytes;
    save->pBaseData = tokenList + tokenBytes;
    save->pCurrentData = save->pBaseData;
    if (tokenCount != 0) {
        save->pTokens = static_cast<char **>(Mem_Calloc(tokenCount, sizeof(char *)));
        if (!save->pTokens) {
            Mem_Free(save);
            return nullptr;
        }
    }
    return save;
}

SAVERESTOREDATA *ReadSaveArena(FileHandle_t file, int dataBytes, int tableCount, int tokenCount, int tokenBytes) {
    if (dataBytes < 0 || tokenBytes < 0 || tokenCount < 0 || tokenCount > tokenBytes) {
        return nullptr;
    }
    const auto position = FS_Tell(file);
    const auto fileBytes = FS_Size(file);
    const auto serializedBytes = static_cast<std::uint64_t>(tokenBytes) + dataBytes;
    if (position > fileBytes || serializedBytes > fileBytes - position) {
        return nullptr;
    }
    auto *save = AllocateSaveArena(dataBytes, tableCount, tokenCount, tokenBytes);
    if (!save) {
        return nullptr;
    }
    const auto discard = [&]() -> SAVERESTOREDATA * {
        Mem_Free(save->pTokens);
        Mem_Free(save);
        return nullptr;
    };
    char *tokenList = save->pBaseData - tokenBytes;
    if (tokenBytes != 0 && FS_Read(tokenList, tokenBytes, 1, file) != tokenBytes) {
        return discard();
    }
    char *cursor = tokenList;
    const char *end = tokenList + tokenBytes;
    for (int index = 0; index < tokenCount; ++index) {
        const auto remaining = static_cast<std::size_t>(end - cursor);
        const auto *terminator = static_cast<const char *>(std::memchr(cursor, 0, remaining));
        if (!terminator) {
            return discard();
        }
        save->pTokens[index] = *cursor ? cursor : nullptr;
        cursor += terminator - cursor + 1;
    }
    if (cursor != end || (dataBytes != 0 && FS_Read(save->pBaseData, dataBytes, 1, file) != dataBytes)) {
        return discard();
    }
    return save;
}
} // namespace

SAVERESTOREDATA *LoadSaveData(const char *level) {
    char path[MAX_PATH];
    _snprintf(path, sizeof(path), "%s%s.HL1", Host_SaveGameDirectory(), level);
    path[sizeof(path) - 1] = '\0';
    COM_FixSlashes(path);
    Con_Printf("Loading game from %s...\n", path);
    const auto file = FS_OpenPathID(path, "rb", "GAMECONFIG");
    if (!file) {
        Con_Printf("ERROR: couldn't open.\n");
        return nullptr;
    }
    enum EnvelopeField {
        Tag, Version, DataBytes, TableCount, TokenCount, TokenBytes, FieldCount
    };
    std::int32_t fields[FieldCount]{};
    for (auto &field : fields) {
        if (!ReadSaveInteger(file, field)) {
            FS_Close(file);
            return nullptr;
        }
    }
    SAVERESTOREDATA *save = nullptr;
    if (fields[Tag] == SAVEFILE_HEADER && fields[Version] <= SAVEGAME_VERSION) {
        save = ReadSaveArena(file, fields[DataBytes], fields[TableCount], fields[TokenCount], fields[TokenBytes]);
    }
    FS_Close(file);
    if (!save) {
        return nullptr;
    }
    Q_strncpy(save->szCurrentMapName, level, sizeof(save->szCurrentMapName) - 1);
    save->szCurrentMapName[sizeof(save->szCurrentMapName) - 1] = '\0';
    save->fUseLandmark = 1;
    VectorClear(save->vecLandmarkOffset);
    gGlobalVariables.pSaveData = save;
    return save;
}
