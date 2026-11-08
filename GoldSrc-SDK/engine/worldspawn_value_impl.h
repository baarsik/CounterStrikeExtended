#pragma once

#include <cstddef>
#include <cstring>

// Uses the engine's existing lexer; parser state belongs to the interrupted engine operation.
bool COM_FindWorldspawnValue(const char *entityData, const char *classname, const char *key,
                            char *output, size_t outputBytes) {
    if (!entityData || !key || !output || outputBytes == 0) {
        return false;
    }
    output[0] = '\0';
    struct ParserState {
        char token[COM_TOKEN_SIZE];
        decltype(s_com_token_unget) unget = s_com_token_unget;
        decltype(com_ignorecolons) ignoreColons = com_ignorecolons;

        ParserState() {
            std::memcpy(token, com_token, sizeof(token));
            s_com_token_unget = 0;
            com_ignorecolons = 0;
        }
        ~ParserState() {
            std::memcpy(com_token, token, sizeof(token));
            s_com_token_unget = unget;
            com_ignorecolons = ignoreColons;
        }
    } state;
    const char *cursor = entityData;
    const char *end = entityData + std::strlen(entityData);
    const auto next = [&]() {
        if (!cursor || cursor >= end) {
            return false;
        }
        cursor = COM_Parse(cursor);
        return cursor && cursor <= end;
    };
    if (!next() || std::strcmp(com_token, "{") != 0) {
        return false;
    }
    bool found = false;
    while (next()) {
        if (std::strcmp(com_token, "}") == 0) {
            return found;
        }
        // Match ED_ParseEdict's key-name bound and trailing-space handling.
        constexpr std::size_t EntityKeyBytes = 256;
        char name[EntityKeyBytes]{};
        auto bytes = std::strlen(com_token);
        if (bytes >= sizeof(name)) {
            bytes = sizeof(name) - 1;
        }
        std::memcpy(name, com_token, bytes);
        while (bytes && name[bytes - 1] == ' ') {
            name[--bytes] = '\0';
        }
        if (!next() || std::strcmp(com_token, "}") == 0) {
            return false;
        }
        // ED_ParseEdict skips values equal to the established class name.
        if (std::strcmp(name, key) == 0 && (!classname || std::strcmp(com_token, classname) != 0)) {
            const auto valueBytes = std::strlen(com_token) + 1;
            if (valueBytes > outputBytes) {
                return false;
            }
            std::memcpy(output, com_token, valueBytes);
            found = true; // Repeated map keys take their last applied value.
        }
    }
    return false;
}
