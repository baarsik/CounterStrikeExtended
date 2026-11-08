#pragma once

// Include after the SDK save interfaces. Shared owned stream for explicit production-DLL fixtures.
#include <array>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <vector>

struct Field {
    std::uint16_t token;
    std::vector<char> bytes;
};

struct Group {
    std::uint16_t token;
    std::vector<Field> fields;
};

struct SavedEntity {
    std::array<char, 65536> bytes{};
    std::array<char *, 1024> tokens{};
    ENTITYTABLE entityTable{};
    SAVERESTOREDATA data{};

    explicit SavedEntity(edict_t &entity) {
        data.pBaseData = bytes.data();
        data.pCurrentData = bytes.data();
        data.bufferSize = static_cast<int>(bytes.size());
        data.tokenCount = static_cast<int>(tokens.size());
        data.pTokens = tokens.data();
        entityTable.pent = &entity;
        data.pTable = &entityTable;
        data.tableCount = 1;
    }

    std::uint16_t Token(const char *name) {
        for (std::size_t index = 0; index < tokens.size(); ++index) {
            if (tokens[index] && std::strcmp(name, tokens[index]) == 0) {
                return static_cast<std::uint16_t>(index);
            }
        }
        for (std::size_t index = 0; index < tokens.size(); ++index) {
            if (!tokens[index]) {
                tokens[index] = const_cast<char *>(name);
                return static_cast<std::uint16_t>(index);
            }
        }
        std::abort();
    }

    template<typename T> T Read(std::size_t &cursor) const {
        if (cursor + sizeof(T) > static_cast<std::size_t>(data.size)) {
            std::abort();
        }
        T value;
        std::memcpy(&value, bytes.data() + cursor, sizeof(value));
        cursor += sizeof(value);
        return value;
    }

    std::vector<Group> Decode() const {
        std::vector<Group> groups;
        std::size_t cursor = 0;
        while (cursor < static_cast<std::size_t>(data.size)) {
            if (Read<std::uint16_t>(cursor) != sizeof(std::int32_t)) {
                std::abort();
            }
            Group group{Read<std::uint16_t>(cursor), {}};
            const auto count = Read<std::uint32_t>(cursor);
            for (std::uint32_t index = 0; index < count; ++index) {
                const auto length = Read<std::uint16_t>(cursor);
                const auto token = Read<std::uint16_t>(cursor);
                if (cursor + length > static_cast<std::size_t>(data.size)) {
                    std::abort();
                }
                group.fields.push_back({token, {bytes.data() + cursor, bytes.data() + cursor + length}});
                cursor += length;
            }
            groups.push_back(group);
        }
        return groups;
    }

    template<typename T> void Append(T value) {
        if (data.size + sizeof(value) > bytes.size()) {
            std::abort();
        }
        std::memcpy(bytes.data() + data.size, &value, sizeof(value));
        data.size += sizeof(value);
    }

    void Encode(const std::vector<Group> &groups) {
        data.size = 0;
        for (const auto &group : groups) {
            Append<std::uint16_t>(sizeof(std::int32_t));
            Append(group.token);
            Append(static_cast<std::uint32_t>(group.fields.size()));
            for (const auto &field : group.fields) {
                Append(static_cast<std::uint16_t>(field.bytes.size()));
                Append(field.token);
                if (data.size + field.bytes.size() > bytes.size()) {
                    std::abort();
                }
                std::memcpy(bytes.data() + data.size, field.bytes.data(), field.bytes.size());
                data.size += static_cast<int>(field.bytes.size());
            }
        }
    }

    void Rewind() {
        data.bufferSize = data.size;
        data.size = 0;
        data.pCurrentData = bytes.data();
    }
};
