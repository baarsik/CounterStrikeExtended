#include <array>
#include <cstdio>
#include <cstring>
#include <initializer_list>
#include <vector>

constexpr std::size_t COM_TOKEN_SIZE = 2048;
char com_token[COM_TOKEN_SIZE]{};
int s_com_token_unget = 0;
int com_ignorecolons = 0;
std::vector<const char *> tokens;
std::size_t tokenIndex = 0;

// Script the unchanged lexer boundary; exercise production selection, bounds and parser-state restoration.
const char *COM_Parse(const char *cursor) {
    if (s_com_token_unget || com_ignorecolons || tokenIndex == tokens.size()) {
        return nullptr;
    }
    const char *token = tokens[tokenIndex++];
    std::memcpy(com_token, token, std::strlen(token) + 1);
    return cursor + 1;
}

#include "../../../GoldSrc-SDK/engine/worldspawn_value_impl.h"

bool Check(std::initializer_list<const char *> input, bool expected, const char *value, std::size_t outputBytes = 64) {
    tokens = input;
    tokenIndex = 0;
    const char prior[] = "prior-parser-token";
    std::memcpy(com_token, prior, sizeof(prior));
    s_com_token_unget = 1;
    com_ignorecolons = 1;
    std::array<char, 128> entityBytes{};
    entityBytes.fill('x');
    entityBytes.back() = '\0';
    std::array<char, 64> output{};
    const bool actual = COM_FindWorldspawnValue(entityBytes.data(), "worldspawn", "mapteams",
                                                output.data(), outputBytes);
    return actual == expected && (!expected || std::strcmp(output.data(), value) == 0)
        && std::strcmp(com_token, prior) == 0 && s_com_token_unget == 1 && com_ignorecolons == 1;
}

int main() {
    if (!Check({"{", "classname", "worldspawn", "mapteams", "red;blue", "}"}, true, "red;blue")
        || !Check({"{", "mapteams ", "red", "mapteams", "blue", "}"}, true, "blue")
        || !Check({"{", "mapteams", "red", "mapteams", "worldspawn", "}"}, true, "red")
        || !Check({"{", "mapteams", "", "}"}, true, "")
        || !Check({"{", "classname", "worldspawn", "}"}, false, "")
        || !Check({"{", "mapteams", "red", "}", "{", "mapteams", "other", "}"}, true, "red")
        || !Check({"{", "mapteams", "red", "}"}, false, "", 3)
        || !Check({"{", "mapteams", "}"}, false, "")
        || !Check({"{", "mapteams", "red"}, false, "")
        || !Check({"worldspawn"}, false, "")) {
        std::fprintf(stderr, "Worldspawn value selection or parser restoration failed\n");
        return 1;
    }
    std::printf("Worldspawn value bounds, duplicates, absence, empty and parser state passed\n");
    return 0;
}
