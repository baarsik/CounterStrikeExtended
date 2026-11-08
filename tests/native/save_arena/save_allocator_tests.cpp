#include "fixture.h"

#define CHECK(condition) do { \
    if (!(condition)) { \
        std::fprintf(stderr, "Save allocator check failed at line %d: %s\n", __LINE__, #condition); \
        return 1; \
    } \
} while (false)

int main(int argc, char **argv) {
    constexpr int TokenSlots = 0xFFF;
    constexpr int EntityCount = 3;
    constexpr int DataBytes = 64;
    auto *save = AllocateSaveArena(DataBytes, EntityCount, TokenSlots, 0);
    CHECK(save && save->pTokens);
    CHECK(reinterpret_cast<std::uintptr_t>(save->pTable) % alignof(ENTITYTABLE) == 0);
    CHECK(save->pBaseData == reinterpret_cast<char *>(save->pTable + EntityCount));
    CHECK(save->pCurrentData == save->pBaseData && save->bufferSize == DataBytes);
    for (int index = 0; index < EntityCount; ++index) {
        const auto &entry = save->pTable[index];
        CHECK(entry.id == 0 && entry.pent == nullptr && entry.classname == 0 && entry.flags == 0
            && entry.location == 0 && entry.size == 0);
    }
    for (int index = 0; index < TokenSlots; ++index) {
        CHECK(save->pTokens[index] == nullptr);
    }
    FreeSave(save);
    CHECK(allocations.empty());
    CHECK(!AllocateSaveArena(INT_MAX, INT_MAX, TokenSlots, 0));
    CHECK(!AllocateSaveArena(DataBytes, -1, TokenSlots, 0));
    CHECK(!AllocateSaveArena(DataBytes, EntityCount, INT_MAX, 0));
    auto *previousSave = &input;
    for (const auto failure : {0, 1}) {
        gGlobalVariables.pSaveData = previousSave;
        allocationAttempt = 0;
        failAllocation = failure;
        CHECK(!AllocateSaveArena(DataBytes, EntityCount, TokenSlots, 0));
        failAllocation = -1;
        CHECK(allocations.empty() && gGlobalVariables.pSaveData == previousSave);
    }
    // Files can become short after their size was inspected; both reads must still be checked.
    for (const auto failure : {0, 1}) {
        input = {{'a', 0, 0x12, 0x34}};
        input.shortReadAt = failure;
        CHECK(!ReadSaveArena(&input, 2, 1, 1, 2));
        CHECK(allocations.empty() && input.closes == 0 && gGlobalVariables.pSaveData == previousSave);
    }
    gGlobalVariables.pSaveData = nullptr;
    if (argc == 2) {
        std::ifstream file(argv[1], std::ios::binary);
        CHECK(file);
        input = {std::vector<unsigned char>((std::istreambuf_iterator<char>(file)), {})};
        std::int32_t tag = 0;
        std::int32_t version = 0;
        std::int32_t dataBytes = 0;
        std::int32_t tokenCount = 0;
        std::int32_t tokenBytes = 0;
        constexpr std::int32_t GlobalSaveTag = INT32_C(0x5641534a); // JSAV.
        CHECK(ReadSaveInteger(&input, tag) && tag == GlobalSaveTag);
        CHECK(ReadSaveInteger(&input, version) && version == SAVEGAME_VERSION);
        CHECK(ReadSaveInteger(&input, dataBytes) && ReadSaveInteger(&input, tokenCount)
            && ReadSaveInteger(&input, tokenBytes));
        save = ReadSaveArena(&input, dataBytes, 0, tokenCount, tokenBytes);
        CHECK(save && save->tableCount == 0);
        constexpr std::size_t GlobalHeaderBytes = 5 * LegacySaveIntegerBytes;
        const auto dataOffset = GlobalHeaderBytes + tokenBytes;
        CHECK(input.cursor == dataOffset + dataBytes);
        CHECK(std::memcmp(save->pBaseData, input.bytes.data() + dataOffset, static_cast<std::size_t>(dataBytes)) == 0);
        std::printf("Legacy global header: %d tokens, %d payload bytes\n", tokenCount, dataBytes);
        FreeSave(save);
    }
    std::puts("Native save allocation, short reads and unchanged failure ownership passed");
    return 0;
}
