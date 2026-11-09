#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include "../../../GoldSrc-SDK/common/legacy_text_encoding.h"
#include <array>
#include <cstdint>
#include <cstdio>

namespace {
constexpr std::size_t Cells = 32;
constexpr std::uint32_t BmpLimit = 0x10000;

template<typename Cell>
bool Utf8Case(const char* input) {
    std::array<wchar_t, Cells> reference;
    reference.fill(0x5a5a);
    const int expected = MultiByteToWideChar(CP_UTF8, 0, input, -1, reference.data(),
        static_cast<int>(reference.size()));
    reference.back() = 0;
    std::array<Cell, Cells> actual;
    actual.fill(static_cast<Cell>(0x5a5a));
    const int result = cse::legacy_text::Utf8ToWideCells(input, actual.data(), static_cast<int>(sizeof(actual)));
    if (result != expected) {
        return false;
    }
    for (std::size_t index = 0; index < Cells; ++index) {
        if (static_cast<std::uint32_t>(actual[index]) != static_cast<std::uint32_t>(reference[index])) {
            return false;
        }
    }
    return true;
}

template<typename Cell>
bool WideCase(std::uint32_t value) {
    const wchar_t input[] = {static_cast<wchar_t>(value), 0};
    const Cell cells[] = {static_cast<Cell>(value), 0};
    std::array<char, Cells> reference;
    reference.fill('Z');
    const int expected = WideCharToMultiByte(CP_UTF8, 0, input, -1, reference.data(),
        static_cast<int>(reference.size()), nullptr, nullptr);
    reference.back() = 0;
    std::array<char, Cells> actual;
    actual.fill('Z');
    const int result = cse::legacy_text::WideCellsToUtf8(cells, actual.data(), static_cast<int>(actual.size()));
    return result == expected && actual == reference;
}
} // namespace

int main() {
    constexpr unsigned char prefixes[][4] = {{0xc2}, {0xe0}, {0xe2}, {0xed}, {0xe2, 0x82},
        {0xf0}, {0xf0, 0x9f}, {0xf0, 0x9f, 0x99}, {0xf4}, {0xf5}};
    std::size_t utf8Cases = 0;
    for (const auto& prefix : prefixes) {
        for (unsigned int byte = 0; byte <= 0xff; ++byte) {
            char input[8] = {};
            std::size_t position = 0;
            while (prefix[position] != 0) {
                input[position] = static_cast<char>(prefix[position]);
                ++position;
            }
            input[position++] = static_cast<char>(byte);
            input[position++] = static_cast<char>(0x82);
            input[position] = static_cast<char>(0xac);
            if (!Utf8Case<std::uint16_t>(input) || !Utf8Case<std::uint32_t>(input)) {
                std::printf("FAIL UTF8 prefix=%zu byte=%u\n", utf8Cases / 256, byte);
                return 1;
            }
            ++utf8Cases;
        }
    }
    for (std::uint32_t value = 0; value < BmpLimit; ++value) {
        if (!WideCase<std::uint16_t>(value) || !WideCase<std::uint32_t>(value)) {
            std::printf("FAIL UTF16 value=%u\n", value);
            return 1;
        }
    }
    std::printf("PASS exact_WinAPI UTF8_transition_cases=%zu all_single_UTF16_units=%u cell_widths=16,32\n",
        utf8Cases, BmpLimit);
    return 0;
}
