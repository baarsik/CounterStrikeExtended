#include "../../GoldSrc-SDK/common/sdl3_policy.h"
#include <cassert>
#include <cstring>

int main() {
    cse::FractionalMotion motion;
    assert(motion.TakeX(0.25f) == 0);
    assert(motion.TakeX(0.25f) == 0);
    assert(motion.TakeX(0.5f) == 1);
    assert(motion.TakeY(-0.75f) == 0);
    assert(motion.TakeY(-0.75f) == -1);
    assert(motion.TakeY(0.5f) == 0);
    motion.Reset();
    assert(motion.TakeX(0.75f) == 0);
    motion.Reset();
    assert(motion.TakeX(0.25f) == 0);
    constexpr unsigned long long start = 9000000000000ULL;
    assert(cse::WithinDoubleClickTime(start, start + 400000000ULL, 400.0f));
    assert(cse::WithinDoubleClickTime(start, start + 400000001ULL, 400.0f));
    assert(cse::WithinDoubleClickTime(start, start + 400999999ULL, 400.0f));
    assert(!cse::WithinDoubleClickTime(start, start + 401000000ULL, 400.0f));
    assert(!cse::WithinDoubleClickTime(start + 999999ULL, start + 401000000ULL, 400.0f));
    assert(!cse::WithinDoubleClickTime(start, start - 1, 400.0f));
    assert(!cse::WithinDoubleClickTime(start, start, -1.0f));
    const char *utf8 = "ab\xc4\x85\xf0\x9f\x98\x80";
    assert(cse::Utf8PrefixBytes(utf8, 3) == 2);
    assert(cse::Utf8PrefixBytes(utf8, 4) == 4);
    assert(cse::Utf8PrefixBytes(utf8, 7) == 4);
    assert(cse::Utf8PrefixBytes(utf8, 8) == 8);
    const char *longText = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
    assert(cse::Utf8PrefixBytes(longText, 100) == std::strlen(longText));
    assert(cse::ScaleCoordinate(150.5f, 300, 600) == 301);
    assert(cse::ScaleCoordinate(10.0f, 0, 600) == 0);
    assert(cse::RefreshRateOrDefault(59.94f) == 60);
    assert(cse::RefreshRateOrDefault(0.0f) == 60);
    static_assert(cse::LegacyGamepadButtonCount == 21, "engine bindings are fixed");
    return 0;
}
