#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include "LocalizedStringTable.h"

#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cwchar>
#include <string>

namespace {

constexpr unsigned char guardByte = 0xa5;
constexpr std::size_t storageCharacters = 64;
constexpr std::size_t leadingGuardCharacters = 2;
constexpr int maximumBufferBytes = 40;
constexpr int exactFitCharacters = 4;
int failures = 0;

void Check(bool condition, const char* label, int bytes) {
    if (!condition) {
        std::fprintf(stderr, "%s failed for %d output bytes\n", label, bytes);
        ++failures;
    }
}

template<typename Format>
void CheckBuffer(const char* label, const wchar_t* expected, int bytes, Format format) {
    std::array<wchar_t, storageCharacters> storage;
    std::memset(storage.data(), guardByte, sizeof(storage));
    wchar_t* output = storage.data() + leadingGuardCharacters;
    format(output, bytes);

    const int capacity = bytes > 0 ? bytes / static_cast<int>(sizeof(wchar_t)) : 0;
    if (capacity > 0) {
        if (expected) {
            const std::wstring value = std::wstring(expected).substr(0, capacity - 1);
            Check(std::memcmp(output, value.data(), value.size() * sizeof(wchar_t)) == 0, label, bytes);
            Check(output[value.size()] == L'\0', label, bytes);
        } else {
            Check(std::wmemchr(output, L'\0', capacity) != nullptr, label, bytes);
        }
    }

    const auto* raw = reinterpret_cast<const unsigned char*>(storage.data());
    constexpr std::size_t outputOffset = leadingGuardCharacters * sizeof(wchar_t);
    for (std::size_t index = 0; index < outputOffset; ++index) {
        Check(raw[index] == guardByte, label, bytes);
    }
    const std::size_t end = outputOffset + static_cast<std::size_t>(capacity) * sizeof(wchar_t);
    for (std::size_t index = end; index < sizeof(storage); ++index) {
        Check(raw[index] == guardByte, label, bytes);
    }
}

template<typename Format>
void CheckSizes(const char* label, const wchar_t* expected, Format format) {
    for (int bytes = -2; bytes <= maximumBufferBytes; ++bytes) {
        CheckBuffer(label, expected, bytes, format);
    }
}

template<typename Localize>
void CheckVariadic(Localize* localize) {
    const struct {
        const wchar_t* format;
        const wchar_t* expected;
    } cases[] = {
        {L"literal", L"literal"},
        {L"%s1", L"abcd"},
        {L"X%s1Y", L"XabcdY"},
        {L"%s2-%s1", L"two-abcd"},
        {L"%s3", L"%s3"},
        {L"", L""},
        {nullptr, L""}
    };
    for (const auto& test : cases) {
        CheckSizes("variadic", test.expected, [&](wchar_t* output, int bytes) {
            localize->ConstructString(output, bytes, const_cast<wchar_t*>(test.format), 2, L"abcd", L"two");
        });
    }
}

// Formatting consumes only KeyValues::GetWString. Supply that external virtual
// boundary without initializing VGUI's unrelated filesystem/surface providers.
// Its slot is defined by the production src/public/KeyValues.h declaration.
struct Variables {
    static constexpr std::size_t getWStringSlot = 13;
    void** table;
    std::array<void*, getWStringSlot + 1> slots{};

    Variables() : table(slots.data()) {
        slots[getWStringSlot] = reinterpret_cast<void*>(&GetWString);
    }

    static const wchar_t* __fastcall GetWString(Variables*, void*, const char* name, const wchar_t* fallback) {
        return std::strcmp(name, "name") == 0 ? L"abcd" : fallback;
    }
};

void CheckKeyValues(vgui2::ILocalize* localize) {
    Variables variables;
    const struct {
        const wchar_t* format;
        const wchar_t* expected;
    } cases[] = {
        {L"literal", L"literal"},
        {L"%name%", L"abcd"},
        {L"X%name%Y", L"XabcdY"},
        {L"%name%-%name%", L"abcd-abcd"},
        {L"%% %name%", L"% abcd"},
        {L"%missing%", L"[unknown]"},
        {L"", L""}
    };
    for (const auto& test : cases) {
        localize->AddString("Format", const_cast<wchar_t*>(test.format), "test.txt");
        const auto index = localize->FindIndex("Format");
        auto* provider = reinterpret_cast<KeyValues*>(&variables);
        CheckSizes("KeyValues index", test.expected, [&](wchar_t* output, int bytes) {
            localize->ConstructString(output, bytes, index, provider);
        });
        CheckSizes("KeyValues token", test.expected, [&](wchar_t* output, int bytes) {
            localize->ConstructString(output, bytes, "Format", provider);
        });
    }
    localize->RemoveAll();
}

void CheckFallback(vgui2::ILocalize* localize) {
    CheckSizes("unknown index", L"[unknown string]", [&](wchar_t* output, int bytes) {
        localize->ConstructString(output, bytes, INVALID_LOCALIZE_STRING_INDEX, nullptr);
    });
    // UTF-8 conversion can fail when the token does not fit. Require bounded,
    // terminated output without changing that conversion's existing policy.
    CheckSizes("missing token", nullptr, [&](wchar_t* output, int bytes) {
        localize->ConstructString(output, bytes, "MissingToken", static_cast<KeyValues*>(nullptr));
    });
    CheckBuffer("missing token with sufficient capacity", L"MissingToken", maximumBufferBytes,
        [&](wchar_t* output, int bytes) {
            localize->ConstructString(output, bytes, "MissingToken", static_cast<KeyValues*>(nullptr));
    });
}

template<typename Localize>
void CheckNullVariadic(Localize* localize) {
    for (int bytes = -2; bytes <= maximumBufferBytes; ++bytes) {
        localize->ConstructString(nullptr, bytes, const_cast<wchar_t*>(L"%s1"), 1, L"abcd");
        localize->ConstructString(nullptr, bytes, static_cast<wchar_t*>(nullptr), 0);
    }
}

} // namespace

int wmain(int argc, wchar_t** argv) {
    static_assert(sizeof(wchar_t) == 2, "The production Windows localization ABI uses UTF-16 wchar_t.");
    if (argc != 3) {
        return EXIT_FAILURE;
    }
    HMODULE library = LoadLibraryExW(argv[1], nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
    if (!library) {
        std::fprintf(stderr, "Cannot load candidate VGUI2: %lu\n", GetLastError());
        return EXIT_FAILURE;
    }
    using Factory = void* (__cdecl*)(const char*, int*);
    const auto factory = reinterpret_cast<Factory>(GetProcAddress(library, "CreateInterface"));
    auto* localize = factory ? static_cast<vgui2::ILocalize*>(factory("VGUI_Localize003", nullptr)) : nullptr;
    auto* legacy = factory ? static_cast<vgui2::CLocalizedStringTable_VGUI_Localize002*>(
        factory("VGUI_Localize002", nullptr)) : nullptr;
    if (!localize || !legacy) {
        FreeLibrary(library);
        return EXIT_FAILURE;
    }

    if (std::wcscmp(argv[2], L"exact-fit") == 0) {
        constexpr int bytes = exactFitCharacters * sizeof(wchar_t);
        CheckBuffer("current exact-fit substitution", L"abc", bytes, [&](wchar_t* output, int size) {
            localize->ConstructString(output, size, const_cast<wchar_t*>(L"%s1"), 1, L"abcd");
        });
        CheckBuffer("legacy exact-fit substitution", L"abc", bytes, [&](wchar_t* output, int size) {
            legacy->ConstructString(output, size, const_cast<wchar_t*>(L"%s1"), 1, L"abcd");
        });
    } else if (std::wcscmp(argv[2], L"variadic") == 0) {
        CheckVariadic(localize);
        CheckVariadic(legacy);
    } else if (std::wcscmp(argv[2], L"keyvalues") == 0) {
        CheckKeyValues(localize);
    } else if (std::wcscmp(argv[2], L"fallback") == 0) {
        CheckFallback(localize);
    } else if (std::wcscmp(argv[2], L"invalid") == 0) {
        CheckNullVariadic(localize);
        CheckNullVariadic(legacy);
        for (int bytes = -2; bytes <= maximumBufferBytes; ++bytes) {
            localize->ConstructString(nullptr, bytes, "missing", static_cast<KeyValues*>(nullptr));
            localize->ConstructString(nullptr, bytes, INVALID_LOCALIZE_STRING_INDEX, nullptr);
        }
    } else {
        ++failures;
    }

    FreeLibrary(library);
    if (failures == 0) {
        std::puts("Localization output and buffer guards passed.");
    }
    return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
