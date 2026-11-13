#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cwchar>
#include <string>

#include "../../../src/common/legacy_text_encoding.h"

namespace {
void Check(bool condition, const char* reason) {
    if (!condition) {
        std::fprintf(stderr, "%s\n", reason);
        std::exit(EXIT_FAILURE);
    }
}
}

namespace vgui2 {
struct Localizer {
    wchar_t translated[2]{L'X', 0};
    int conversions = 0;
    int capacityBytes = 0;

    wchar_t* Find(const char* text) {
        return std::strcmp(text, "#found") == 0 ? translated : nullptr;
    }
    int ConvertANSIToUnicode(const char* text, wchar_t* output, int bytes) {
        ++conversions;
        capacityBytes = bytes;
        return cse::legacy_text::Utf8ToWideCells(text, output, bytes);
    }
};
Localizer localizer;
Localizer* localize() {
    return &localizer;
}
}

#include "actual_find.inc"

int main() {
    constexpr std::size_t kRingEntries = 4;
    constexpr std::size_t kFallbackCells = 1024;
    constexpr std::size_t kBeyondOldNativeLimit = 700;
    wchar_t* slots[kRingEntries]{};
    std::string text(kBeyondOldNativeLimit, 'a');
    for (std::size_t index = 0; index < kRingEntries; ++index) {
        text[0] = static_cast<char>('a' + index);
        slots[index] = SharedFindString(&text[0]);
        Check(std::wcslen(slots[index]) == text.size(), "Fallback loses text after 511 native cells");
        Check(slots[index][0] == static_cast<wchar_t>(text[0]), "Fallback contents differ");
        for (std::size_t previous = 0; previous < index; ++previous) {
            Check(slots[previous] != slots[index], "Fallback ring aliases early");
        }
    }
    char found[] = "#found";
    const int previousConversions = vgui2::localizer.conversions;
    Check(SharedFindString(found) == vgui2::localizer.translated, "Localized identity differs");
    Check(vgui2::localizer.conversions == previousConversions, "Localized lookup converts fallback");
    text.assign(kFallbackCells - 1, 'z');
    Check(SharedFindString(&text[0]) == slots[0], "Localized lookup advanced the fallback ring");
    Check(std::wcslen(slots[0]) == kFallbackCells - 1, "Full fallback capacity differs");
    text.push_back('z');
    Check(SharedFindString(&text[0]) == slots[1], "Fallback ring order differs");
    Check(std::wcslen(slots[1]) == kFallbackCells - 1 && slots[1][kFallbackCells - 1] == 0,
        "Oversize fallback terminator differs");
    text.assign(kBeyondOldNativeLimit, 'p');
    text += "\xf0\x9f\x98\x80";
    Check(SharedFindString(&text[0]) == slots[2], "Supplementary fallback ring order differs");
    Check(std::wcslen(slots[2]) == kBeyondOldNativeLimit + 2, "UTF16 fallback cell count differs");
    Check(static_cast<std::uint32_t>(slots[2][kBeyondOldNativeLimit]) == 0xd83d
        && static_cast<std::uint32_t>(slots[2][kBeyondOldNativeLimit + 1]) == 0xde00,
        "Supplementary fallback units differ");
    Check(vgui2::localizer.capacityBytes == static_cast<int>(kFallbackCells * sizeof(wchar_t)),
        "Fallback passes a logical length in place of physical bytes");
    slots[2][0] = L'Q';
    Check(slots[2][0] == L'Q', "Fallback mutability differs");
    std::puts("PASS actual localization fallback capacity, four-slot identity and UTF16 cells");
}
