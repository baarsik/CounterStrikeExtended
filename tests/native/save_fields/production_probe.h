#pragma once

// Run through the actual rebuilt game DLL's SaveWriteFields/SaveReadFields API.
// Uses owned records and an in-memory legacy token stream, with no game world.
inline bool CheckProductionSaveFields(const DLL_FUNCTIONS &api) {
    struct Snapshot {
        std::uint32_t before;
        bool defusing;
        bool planted;
        unsigned short event;
        std::int64_t wide;
        std::int64_t counters[3];
        std::uint32_t after;
    };
    TYPEDESCRIPTION fields[] = {
        DEFINE_FIELD(Snapshot, defusing, FIELD_BOOLEAN),
        DEFINE_FIELD(Snapshot, planted, FIELD_BOOLEAN),
        DEFINE_FIELD(Snapshot, event, FIELD_INTEGER),
        DEFINE_FIELD(Snapshot, wide, FIELD_INTEGER),
        DEFINE_ARRAY(Snapshot, counters, FIELD_INTEGER, 2)
    };
    constexpr std::uint32_t beforeGuard = UINT32_C(0x12345678);
    constexpr std::uint32_t afterGuard = UINT32_C(0x87654321);
    constexpr std::int64_t tailGuard = INT64_C(987654321);
    Snapshot source{beforeGuard, true, false, 0x1234, -123, {-7, 16, tailGuard}, afterGuard};
    std::array<char, 4096> bytes{};
    std::array<char *, 128> tokens{};
    SAVERESTOREDATA save{};
    save.pBaseData = bytes.data();
    save.pCurrentData = bytes.data();
    save.bufferSize = static_cast<int>(bytes.size());
    save.tokenCount = static_cast<int>(tokens.size());
    save.pTokens = tokens.data();
    api.pfnSaveWriteFields(&save, "CSE_NATIVE_FIELDS", &source, fields, static_cast<int>(std::size(fields)));
    constexpr std::size_t groupCountOffset = 2 * sizeof(std::uint16_t);
    std::uint32_t count;
    std::memcpy(&count, bytes.data() + groupCountOffset, sizeof(count));
    constexpr std::uint32_t expectedNonemptyFields = 4;
    if (count != expectedNonemptyFields) {
        std::fprintf(stderr, "False bool included neighbouring bytes in production save (fields=%u)\n", count);
        return false;
    }
    Snapshot restored{beforeGuard, false, true, 0, 0, {0, 0, tailGuard}, afterGuard};
    save.bufferSize = save.size;
    save.size = 0;
    save.pCurrentData = bytes.data();
    api.pfnSaveReadFields(&save, "CSE_NATIVE_FIELDS", &restored, fields, static_cast<int>(std::size(fields)));
    if (!restored.defusing || restored.planted || restored.event != source.event || restored.wide != source.wide
        || restored.counters[0] != -7 || restored.counters[1] != 16 || restored.counters[2] != tailGuard
        || restored.before != beforeGuard || restored.after != afterGuard) {
        std::fprintf(stderr, "Production native-width save fields failed round trip or changed neighbours\n");
        return false;
    }
    // Build an older one-field group with false's adjacent-byte contamination.
    constexpr std::size_t legacyCount = 1;
    constexpr std::size_t headerBytes = 2 * sizeof(std::uint16_t);
    constexpr std::uint16_t integerBytes = sizeof(std::uint32_t);
    std::uint16_t fieldToken = 0;
    while (fieldToken < tokens.size() && tokens[fieldToken]) {
        ++fieldToken;
    }
    if (fieldToken == tokens.size()) {
        return false;
    }
    tokens[fieldToken] = const_cast<char *>(fields[1].fieldName);
    const std::uint32_t oldCount = static_cast<std::uint32_t>(legacyCount);
    std::memcpy(bytes.data() + headerBytes, &oldCount, sizeof(oldCount));
    std::memcpy(bytes.data() + headerBytes + sizeof(oldCount), &integerBytes, sizeof(integerBytes));
    std::memcpy(bytes.data() + headerBytes + sizeof(oldCount) + sizeof(integerBytes), &fieldToken, sizeof(fieldToken));
    constexpr std::uint32_t oldFalse = UINT32_C(0x00000100);
    std::memcpy(bytes.data() + 2 * headerBytes + sizeof(oldCount), &oldFalse, sizeof(oldFalse));
    restored.planted = true;
    restored.event = 0x4321;
    save.bufferSize = static_cast<int>(2 * headerBytes + sizeof(oldCount) + sizeof(oldFalse));
    save.size = 0;
    save.pCurrentData = bytes.data();
    api.pfnSaveReadFields(&save, "CSE_NATIVE_FIELDS", &restored, fields, static_cast<int>(std::size(fields)));
    return !restored.defusing && !restored.planted && restored.event == 0 && restored.wide == 0
        && restored.counters[0] == 0 && restored.counters[1] == 0 && restored.counters[2] == tailGuard
        && restored.before == beforeGuard && restored.after == afterGuard;
}
