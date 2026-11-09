#include "characterset.h"

#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {
constexpr unsigned char kGuardByte = 0x5a;
constexpr std::size_t kByteValues = 256;

void Require(bool condition) {
    if (!condition) {
        std::fputs("Character-set byte-domain or guard check failed\n", stderr);
        std::exit(EXIT_FAILURE);
    }
}
}

int main() {
    struct GuardedTable {
        std::array<unsigned char, kByteValues> before;
        characterset_t table;
        std::array<unsigned char, kByteValues> after;
    } guarded{};
    guarded.before.fill(kGuardByte);
    guarded.after.fill(kGuardByte);
    std::array<unsigned char, kByteValues> input{};
    for (std::size_t index = 1; index < input.size(); ++index) {
        input[index - 1] = static_cast<unsigned char>(index);
    }
    CharacterSetBuild(&guarded.table, reinterpret_cast<const char*>(input.data()));
    Require(guarded.table.set[0] == 0);
    for (std::size_t index = 1; index < kByteValues; ++index) {
        Require(guarded.table.set[index] == 1);
    }
    for (std::size_t index = 0; index < kByteValues; ++index) {
        Require(guarded.before[index] == kGuardByte);
        Require(guarded.after[index] == kGuardByte);
    }
    const characterset_t beforeNull = guarded.table;
    CharacterSetBuild(nullptr, "test");
    CharacterSetBuild(&guarded.table, nullptr);
    Require(std::memcmp(&beforeNull, &guarded.table, sizeof(beforeNull)) == 0);
    CharacterSetBuild(&guarded.table, "");
    for (unsigned char value : guarded.table.set) {
        Require(value == 0);
    }
    CharacterSetBuild(&guarded.table, " ;\t; ");
    for (std::size_t index = 0; index < kByteValues; ++index) {
        const unsigned char expected = index == ' ' || index == ';' || index == '\t' ? 1 : 0;
        Require(guarded.table.set[index] == expected);
    }
    return EXIT_SUCCESS;
}
