#include "../../../GoldSrc-SDK/common/legacy_save_strings.h"
#include <cstdio>
#include <memory>

#define CHECK(condition) do { if (!(condition)) { \
    std::fprintf(stderr, "Check failed at line %d: %s\n", __LINE__, #condition); \
    return 1; \
} } while (false)

int main() {
    using cse::save::ValidLegacyStringPayload;
    const char two[] = {'a', '\0', 'b', '\0'};
    CHECK(ValidLegacyStringPayload(two, sizeof(two), 2));
    CHECK(!ValidLegacyStringPayload(two, sizeof(two), 1));
    CHECK(!ValidLegacyStringPayload(two, sizeof(two) - 1, 2));
    CHECK(!ValidLegacyStringPayload(two, sizeof(two), 3));
    CHECK(!ValidLegacyStringPayload(two, sizeof(two), -1));
    const char empty[] = {'\0', '\0'};
    CHECK(ValidLegacyStringPayload(empty, sizeof(empty), 2));
    const unsigned char utf8[] = {0xc5, 0xbc, 0xe2, 0x98, 0x83, 0};
    CHECK(ValidLegacyStringPayload(utf8, sizeof(utf8), 1));
    CHECK(!ValidLegacyStringPayload(utf8, sizeof(utf8) - 1, 1));
    CHECK(ValidLegacyStringPayload(nullptr, 0, 0));
    CHECK(!ValidLegacyStringPayload(nullptr, 0, 1));
    CHECK(!ValidLegacyStringPayload(nullptr, 1, 1));
    constexpr size_t truncatedBytes = 3;
    auto truncated = std::make_unique<unsigned char[]>(truncatedBytes);
    truncated[0] = 'a';
    truncated[1] = 'b';
    truncated[2] = 'c';
    CHECK(!ValidLegacyStringPayload(truncated.get(), truncatedBytes, 1));
    return 0;
}
