#include "legacy_wide_format.h"
#include <array>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cwchar>
#include <string>

namespace {
void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "FAILED: %s\n", message);
        std::abort();
    }
}
}

namespace actual_wrappers {
#define _vsnwprintf CseLegacyVsnwprintf
#include "actual_wrappers.h"
#undef _vsnwprintf
}

int main() {
    wchar_t format[]{L'%', L's', 0};
    wchar_t* first = actual_wrappers::SharedWVarArgs(format, L"first");
    Require(std::wcscmp(first, L"first") == 0, "actual ring wrapper formats canonical Windows wide s");
    Require(actual_wrappers::SharedWVarArgs(format, L"second") != first, "second ring slot");
    Require(actual_wrappers::SharedWVarArgs(format, L"third") != first, "third ring slot");
    Require(actual_wrappers::SharedWVarArgs(format, L"fourth") != first, "fourth ring slot");
    Require(actual_wrappers::SharedWVarArgs(format, L"fifth") == first && std::wcscmp(first, L"fifth") == 0,
        "fifth call reuses the owned first slot in original order");
    constexpr std::size_t kRingCapacity = 1024;
    constexpr std::size_t kOversizeCharacters = kRingCapacity + 76;
    const std::wstring large(kOversizeCharacters, L'X');
    wchar_t* truncated = actual_wrappers::SharedWVarArgs(format, large.c_str());
    Require(std::wcslen(truncated) == kRingCapacity - 1 && truncated[kRingCapacity - 1] == 0,
        "actual wrapper preserves its extra terminator policy after raw legacy truncation");
    std::array<wchar_t, 8> destination;
    destination.fill(L'Z');
    int remaining = 4;
    wchar_t* end = actual_wrappers::BufWPrintf(destination.data(), remaining, L"ABCD");
    Require(end == destination.data() + 3 && remaining == 1 && destination[0] == L'A'
        && destination[1] == L'B' && destination[2] == L'C' && destination[3] == 0 && destination[4] == L'Z',
        "actual append wrapper advances/decrements by its guarded wcslen on exact fit");
    for (int length : {0, -1}) {
        destination.fill(L'Z');
        int size = length;
        Require(actual_wrappers::BufWPrintf(destination.data(), size, L"A") == nullptr && size == length
            && destination[0] == L'Z', "actual nonpositive append capacity returns before writing");
    }
    std::puts("PASS: actual shared wide wrapper ring, count, NUL and append ownership contracts");
    return 0;
}
