#include <climits>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include "../../../src/common/legacy_text_to_integer.h"
#include "actual_native_track.inc"

namespace {
struct TrackCase {
    const char* text;
    int expected;
};
constexpr TrackCase kCases[] = {
    {"", 1}, {"0", 1}, {"-1", 0}, {"1", 2}, {"99", 100}, {"100", 101},
    {"2147483646", INT_MAX}, {"2147483647", INT_MIN}, {"2147483648", INT_MIN},
    {"-2147483648", INT_MIN + 1}, {"-2147483649", INT_MIN + 1},
    {"99999999999999999999999999", INT_MIN}, {"+", 1}, {"\v -10tail", -9}, {"0x12", 1}
};
}

int main() {
    for (const TrackCase& entry : kCases) {
        const int actual = ParseNativeMp3Track(entry.text);
        if (actual != entry.expected) {
            std::fprintf(stderr, "Track parse differs for '%s': %d expected %d\n", entry.text, actual, entry.expected);
            return EXIT_FAILURE;
        }
#if defined(_WIN32)
        if (ActualWindowsTrack(entry.text) != actual) {
            std::fprintf(stderr, "Track parse differs from retained Windows expression for '%s'\n", entry.text);
            return EXIT_FAILURE;
        }
#endif
    }
    std::puts("PASS actual CD track parser and 15 Win32 boundary values");
    return EXIT_SUCCESS;
}
