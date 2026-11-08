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

inline bool CheckProductionStringFields(const DLL_FUNCTIONS &api, const enginefuncs_t &engine,
                                       const globalvars_t &globals) {
    struct Strings {
        std::uint32_t before;
        string_t names[3];
        string_t model;
        string_t sound;
        std::uint32_t after;
    };
    constexpr std::uint32_t guard = UINT32_C(0x1234abcd);
    Strings source{guard, {engine.pfnAllocString("mutable-name"), 0, engine.pfnAllocString("tail")},
                   engine.pfnAllocString("models/fixture.mdl"), engine.pfnAllocString("fixture.wav"), guard};
    if (sizeof(string_t) > sizeof(std::uint32_t) && source.names[0] <= UINT32_MAX) {
        std::fprintf(stderr, "Production string fixture requires high relative offsets\n");
        return false;
    }
    TYPEDESCRIPTION fields[] = {
        DEFINE_ARRAY(Strings, names, FIELD_STRING, 2),
        DEFINE_FIELD(Strings, model, FIELD_MODELNAME),
        DEFINE_FIELD(Strings, sound, FIELD_SOUNDNAME)
    };
    std::array<char, 4096> bytes{};
    std::array<char *, 128> tokens{};
    SAVERESTOREDATA save{};
    save.pBaseData = bytes.data();
    save.pCurrentData = bytes.data();
    save.bufferSize = static_cast<int>(bytes.size());
    save.tokenCount = static_cast<int>(tokens.size());
    save.pTokens = tokens.data();
    api.pfnSaveWriteFields(&save, "CSE_NATIVE_STRINGS", &source, fields, static_cast<int>(std::size(fields)));
    // Inspect the independent legacy envelope: four-byte group count, then bounded NUL-terminated text.
    constexpr std::size_t headerBytes = 2 * sizeof(std::uint16_t);
    std::size_t cursor = headerBytes;
    std::uint32_t count = 0;
    std::memcpy(&count, bytes.data() + cursor, sizeof(count));
    cursor += sizeof(count);
    if (count != std::size(fields)) {
        return false;
    }
    const char names[] = "mutable-name\0"; // The second name is an empty string.
    const char model[] = "models/fixture.mdl";
    const char sound[] = "fixture.wav";
    const void *expected[] = {names, model, sound};
    const std::size_t expectedBytes[] = {sizeof(names), sizeof(model), sizeof(sound)};
    for (std::size_t index = 0; index < std::size(fields); ++index) {
        std::uint16_t payloadBytes = 0;
        std::memcpy(&payloadBytes, bytes.data() + cursor, sizeof(payloadBytes));
        cursor += headerBytes;
        if (payloadBytes != expectedBytes[index]
            || std::memcmp(bytes.data() + cursor, expected[index], payloadBytes) != 0) {
            std::fprintf(stderr, "Native string escaped the legacy symbolic envelope\n");
            return false;
        }
        cursor += payloadBytes;
    }
    if (cursor != static_cast<std::size_t>(save.size)) {
        return false;
    }
    Strings restored{guard, {0, source.names[0], source.names[2]}, 0, 0, guard};
    save.bufferSize = save.size;
    save.size = 0;
    save.pCurrentData = bytes.data();
    api.pfnSaveReadFields(&save, "CSE_NATIVE_STRINGS", &restored, fields, static_cast<int>(std::size(fields)));
    return restored.before == guard && restored.after == guard && restored.names[1] == 0
        && restored.names[2] == source.names[2]
        && std::strcmp(CseStringAddress(globals.pStringBase, restored.names[0]), "mutable-name") == 0
        && std::strcmp(CseStringAddress(globals.pStringBase, restored.model), model) == 0
        && std::strcmp(CseStringAddress(globals.pStringBase, restored.sound), sound) == 0;
}

inline bool CheckProductionEntityFields(const DLL_FUNCTIONS &api, const enginefuncs_t &engine, edict_t *entities) {
    struct References {
        std::uint32_t before;
        EOFFSET offsets[3];
        EOFFSET missing;
        std::uint32_t after;
    };
    constexpr std::uint32_t guard = UINT32_C(0x1234abcd);
    constexpr EOFFSET tailGuard = 77;
    References source{guard, {engine.pfnEntOffsetOfPEntity(entities + 1), 0, tailGuard},
                      engine.pfnEntOffsetOfPEntity(entities + 2), guard};
    TYPEDESCRIPTION fields[] = {
        DEFINE_ARRAY(References, offsets, FIELD_ENTITY, 2),
        DEFINE_FIELD(References, missing, FIELD_ENTITY)
    };
    std::array<char, 4096> bytes{};
    std::array<char *, 128> tokens{};
    std::array<ENTITYTABLE, 2> entityTable{};
    entityTable[0].id = 0;
    entityTable[0].pent = entities;
    entityTable[1].id = 1;
    entityTable[1].pent = entities + 1;
    SAVERESTOREDATA save{};
    save.pBaseData = bytes.data();
    save.pCurrentData = bytes.data();
    save.bufferSize = static_cast<int>(bytes.size());
    save.tokenCount = static_cast<int>(tokens.size());
    save.pTokens = tokens.data();
    save.tableCount = static_cast<int>(entityTable.size());
    save.pTable = entityTable.data();
    api.pfnSaveWriteFields(&save, "CSE_NATIVE_ENTITIES", &source, fields, static_cast<int>(std::size(fields)));
    constexpr std::size_t headerBytes = 2 * sizeof(std::uint16_t);
    constexpr std::uint32_t expectedFields = 2;
    std::uint32_t count = 0;
    std::memcpy(&count, bytes.data() + headerBytes, sizeof(count));
    if (count != expectedFields) {
        return false;
    }
    // Offsets become signed four-byte entity-table IDs, including world zero and missing minus one.
    const std::int32_t expectedIds[] = {1, 0, -1};
    const std::size_t fieldCounts[] = {2, 1};
    std::size_t cursor = headerBytes + sizeof(count);
    std::size_t nextId = 0;
    for (const auto fieldCount : fieldCounts) {
        std::uint16_t payloadBytes = 0;
        std::memcpy(&payloadBytes, bytes.data() + cursor, sizeof(payloadBytes));
        cursor += headerBytes;
        const auto expectedBytes = fieldCount * sizeof(std::int32_t);
        if (payloadBytes != expectedBytes
            || std::memcmp(bytes.data() + cursor, expectedIds + nextId, expectedBytes) != 0) {
            std::fprintf(stderr, "Native entity offset escaped the legacy entity-ID envelope\n");
            return false;
        }
        cursor += payloadBytes;
        nextId += fieldCount;
    }
    if (cursor != static_cast<std::size_t>(save.size)) {
        return false;
    }
    // Restore against a different pool mapping: only the IDs survive the file boundary.
    entityTable[0].pent = entities + 1;
    entityTable[1].pent = entities + 2;
    References restored{guard, {-1, -1, tailGuard}, -1, guard};
    save.bufferSize = save.size;
    save.size = 0;
    save.pCurrentData = bytes.data();
    api.pfnSaveReadFields(&save, "CSE_NATIVE_ENTITIES", &restored, fields, static_cast<int>(std::size(fields)));
    return restored.before == guard && restored.after == guard && restored.offsets[2] == tailGuard
        && restored.offsets[0] == engine.pfnEntOffsetOfPEntity(entities + 2)
        && restored.offsets[1] == engine.pfnEntOffsetOfPEntity(entities + 1) && restored.missing == 0;
}
