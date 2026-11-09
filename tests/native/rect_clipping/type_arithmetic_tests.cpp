#include "cse_rect.h"
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <type_traits>

static_assert(std::is_same<RECT, tagRECT>::value, "Canonical native tag identity");
static_assert(std::is_same<decltype(RECT::left), std::int32_t>::value, "Native signed32 coordinate");
static_assert(std::is_same<decltype(RECT::top), std::int32_t>::value, "Native signed32 coordinate");
static_assert(std::is_same<decltype(RECT::right), std::int32_t>::value, "Native signed32 coordinate");
static_assert(std::is_same<decltype(RECT::bottom), std::int32_t>::value, "Native signed32 coordinate");
static_assert(sizeof(RECT) == 16 && alignof(RECT) == alignof(std::int32_t), "Canonical record storage");
static_assert(offsetof(RECT, left) == 0 && offsetof(RECT, top) == 4 && offsetof(RECT, right) == 8
    && offsetof(RECT, bottom) == 12, "Windows physical field order");
static_assert(cse_rect::Difference32((std::numeric_limits<std::int32_t>::min)(), 1)
    == (std::numeric_limits<std::int32_t>::max)(), "Subtraction wraps before signed32 mapping");
namespace {
void Check(std::int32_t position, std::int32_t origin) {
    constexpr std::int64_t Modulus = std::int64_t{1} << 32;
    constexpr auto Minimum = (std::numeric_limits<std::int32_t>::min)();
    constexpr auto Maximum = (std::numeric_limits<std::int32_t>::max)();
    std::int64_t expected = static_cast<std::int64_t>(position) - origin;
    if (expected < Minimum) {
        expected += Modulus;
    } else if (expected > Maximum) {
        expected -= Modulus;
    }
    if (cse_rect::Difference32(position, origin) != expected) {
        std::fprintf(stderr, "FAIL difference position=%d origin=%d expected=%lld\n", position, origin,
            static_cast<long long>(expected));
        std::exit(1);
    }
}
std::uint32_t Next(std::uint32_t& state) {
    state ^= state << 13;
    state ^= state >> 17;
    state ^= state << 5;
    return state;
}
std::int32_t Signed(std::uint32_t value) {
    if (value <= static_cast<std::uint32_t>((std::numeric_limits<std::int32_t>::max)())) {
        return static_cast<std::int32_t>(value);
    }
    return -1 - static_cast<std::int32_t>((std::numeric_limits<std::uint32_t>::max)() - value);
}
}
int main() {
    constexpr std::int32_t Minimum = (std::numeric_limits<std::int32_t>::min)();
    constexpr std::int32_t Maximum = (std::numeric_limits<std::int32_t>::max)();
    constexpr std::int32_t Positions[] = {Minimum, Minimum + 1, -16777217, -1, 0, 1, 16777217, Maximum - 1, Maximum};
    for (auto position : Positions) {
        for (auto origin : Positions) {
            Check(position, origin);
        }
    }
    std::uint32_t state = 0x0d9e2543U;
    constexpr int RandomCases = 100000;
    for (int index = 0; index < RandomCases; ++index) {
        const auto position = Signed(Next(state));
        const auto origin = Signed(Next(state));
        Check(position, origin);
    }
    std::puts("PASS 100081 independent int64-oracle rectangle differences + native record contract");
}
