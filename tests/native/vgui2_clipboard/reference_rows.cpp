#include "NativeClipboard.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>

namespace {
using namespace cse_vgui2_clipboard;

void Require(Status status) {
    if (status != Status::Ok) {
        std::fprintf(stderr, "Unexpected pure status %d\n", static_cast<int>(status));
        std::exit(1);
    }
}

void Hex(const unsigned char* bytes, std::size_t size) {
    for (std::size_t index = 0; index < size; ++index) {
        std::printf("%02x", static_cast<unsigned int>(bytes[index]));
    }
}

void WideRow(const SnapshotView& snapshot, unsigned int referenceRow, std::int32_t offset,
    std::uint64_t capacityCells) {
    wchar_t output[12];
    for (auto& value : output) {
        value = 0xa5a5;
    }
    std::int32_t count = -1;
    Require(ReadUnicodeCells(snapshot, offset, {output, capacityCells * sizeof(wchar_t)}, count));
    std::printf("{\"kind\":\"wide\",\"reference_row\":%u,\"result\":%d,\"cells\":[", referenceRow, count);
    for (unsigned int index = 0; index < 12; ++index) {
        std::printf("%s%u", index ? "," : "", static_cast<unsigned int>(output[index]));
    }
    std::puts("]}");
}
}

int main() {
    const unsigned char input[] = {0x41, 0, 0xa9, 3, 0, 0, 0xb1, 0xb2, 0xb3};
    const SnapshotView snapshot{1, true, 17, true, {input, sizeof(input)}, 8};
    unsigned int referenceRow = 0;
    for (std::uint64_t capacity : {UINT64_C(1), UINT64_C(2), UINT64_C(3), UINT64_C(4), UINT64_C(8),
            UINT64_C(9), UINT64_C(12)}) {
        unsigned char output[24];
        std::memset(output, 0xa5, sizeof(output));
        std::int32_t count = -1;
        Require(ReadUnicodeBytes(snapshot, 0, {output, capacity}, count));
        std::printf("{\"kind\":\"raw\",\"reference_row\":%u,\"result\":%d,\"bytes\":\"", referenceRow++, count);
        Hex(output, sizeof(output));
        std::puts("\"}");
    }
    WideRow(snapshot, 29, 0, 1);
    WideRow(snapshot, 31, 0, 2);
    WideRow(snapshot, 32, 0, 4);
    WideRow(snapshot, 56, 1, 1);
    std::int32_t count = -1;
    Require(CountLegacyText(snapshot, count));
    std::printf("{\"kind\":\"count\",\"reference_row\":72,\"result\":%d}\n", count);
    unsigned char packed[13];
    std::memset(packed, 0xcc, sizeof(packed));
    const unsigned char raw[] = {0x41, 0, 0x80, 0x5a};
    std::uint64_t bytes = 0;
    Require(PackLegacyBytes({raw, sizeof(raw)}, 4, {packed, 8}, bytes));
    std::printf("{\"kind\":\"pack\",\"reference_row\":62,\"requested\":%llu,\"bytes\":\"",
        static_cast<unsigned long long>(bytes));
    Hex(packed, 8);
    std::puts("\"}");
    std::memset(packed, 0xcc, sizeof(packed));
    const wchar_t wide[] = {0x41, 0, 0xd83d, 0xde00};
    Require(PackWideCells({wide, sizeof(wide)}, 4, {packed, sizeof(packed)}, bytes));
    std::printf("{\"kind\":\"pack\",\"reference_row\":67,\"requested\":%llu,\"bytes\":\"",
        static_cast<unsigned long long>(bytes));
    Hex(packed, sizeof(packed));
    std::puts("\"}");
    if (sizeof(wchar_t) == 4) {
        std::uint32_t seed = 0x927add34;
        for (unsigned int row = 0; row < 520; ++row) {
            const std::uint32_t boundaries[] = {0x10000, 0x10001, 0x103ff, 0x10400,
                0x1ffff, 0x20000, 0x10fffe, 0x10ffff};
            seed ^= seed << 13;
            seed ^= seed >> 17;
            seed ^= seed << 5;
            const auto value = row < 8 ? boundaries[row] : 0x10000 + (seed % 0x100000);
            const wchar_t scalar[] = {static_cast<wchar_t>(value)};
            unsigned char encoded[6];
            std::memset(encoded, 0xa5, sizeof(encoded));
            Require(PackWideCells({scalar, sizeof(scalar)}, 1, {encoded, sizeof(encoded)}, bytes));
            if (bytes != sizeof(encoded)) {
                return 1;
            }
            std::printf("{\"kind\":\"scalar\",\"value\":%u,\"bytes\":\"", value);
            Hex(encoded, sizeof(encoded));
            std::puts("\"}");
        }
    }
    return 0;
}
