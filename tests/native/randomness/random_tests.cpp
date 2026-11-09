#include <climits>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <limits>

namespace {
std::time_t fixtureClock = 0;
}

static std::time_t CseFixtureTime(std::time_t*) noexcept {
    return fixtureClock;
}

#define time CseFixtureTime
#include CSE_RANDOM_IMPLEMENTATION
#undef time

namespace {
constexpr int kValuesPerSequence = 512;
constexpr std::int64_t kInputs[] = {
    0, 1, -1, 999, -999, 1000, -1000, 1001, -1001, 22261048, -22261048,
    INT_MAX, INT_MIN, 0x80000000LL, 0xffffffffLL, 0x100000000LL,
    0x100000001LL, -0x100000001LL, 0x1234567880000000LL
};
constexpr std::int64_t kTimes[] = {0, 1, -1, 1900000000, INT_MAX, 0x80000000LL, 0xffffffffLL, 0x100000000LL};
struct Bounds {
    int low;
    int high;
};
constexpr Bounds kBounds[] = {{0, 1}, {-10, 10}, {INT_MIN, -1}, {0, INT_MAX},
    {INT_MIN, INT_MAX}, {INT_MAX, INT_MIN}, {INT_MIN, INT_MIN}, {INT_MAX, INT_MAX}, {100, -100}};

void PrintWord(char kind, int sequence, int index, std::uint32_t word) {
    std::printf("%c %d %d %08x\n", kind, sequence, index, static_cast<unsigned int>(word));
}

void Sequence(long seed, int sequence) {
    SeedRandomNumberGenerator(seed);
    for (int index = 0; index < kValuesPerSequence; ++index) {
        PrintWord('I', sequence, index, static_cast<std::uint32_t>(ran1()));
    }
    SeedRandomNumberGenerator(seed);
    for (int index = 0; index < kValuesPerSequence; ++index) {
        const float value = RandomFloat(-999.25f, 3000.75f);
        std::uint32_t word = 0;
        std::memcpy(&word, &value, sizeof(word));
        PrintWord('F', sequence, index, word);
    }
    for (std::size_t bound = 0; bound < sizeof(kBounds) / sizeof(kBounds[0]); ++bound) {
        SeedRandomNumberGenerator(seed);
        for (int index = 0; index < kValuesPerSequence; ++index) {
            const int value = RandomLong(kBounds[bound].low, kBounds[bound].high);
            PrintWord('R', sequence, static_cast<int>(bound) * kValuesPerSequence + index,
                static_cast<std::uint32_t>(value));
        }
    }
}
}

int main() {
    static_assert(sizeof(int) == sizeof(std::uint32_t), "Random API integers are 32 bits");
    static_assert(sizeof(float) == sizeof(std::uint32_t), "Random float outputs use binary32 storage");
    int sequence = 0;
    fixtureClock = static_cast<std::time_t>(1900000000);
    for (std::int64_t seed : kInputs) {
        Sequence(static_cast<long>(seed), sequence++);
    }
    for (std::int64_t clock : kTimes) {
        fixtureClock = static_cast<std::time_t>(clock);
        Sequence(0, sequence++);
    }
    return std::ferror(stdout) ? EXIT_FAILURE : EXIT_SUCCESS;
}
