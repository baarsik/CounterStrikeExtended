#include "actual_time_type.h"

#include <cstdio>
#include <cstdlib>

namespace {

void Require(bool condition, const char* description) {
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", description);
        std::exit(1);
    }
}

}

int main() {
    constexpr double BeyondBinary32IntegerPrecision = 16777217.0;
    constexpr std::int32_t ExpectedPrecisionValue = 16777217;
    constexpr double MaximumSigned32Millis = 2147483647.0;
    constexpr std::int32_t ExpectedMaximumValue = 2147483647;
    Require(Vgui2FakeTimeProducer(BeyondBinary32IntegerPrecision) == ExpectedPrecisionValue,
        "valid milliseconds must not take a binary32 detour");
    Require(Vgui2FakeTimeProducer(MaximumSigned32Millis) == ExpectedMaximumValue,
        "the actual return type retains valid INT32_MAX milliseconds");
    Require(Vgui2FakeTimeProducer(-1.75) == -1, "valid fractional milliseconds truncate toward zero");
    std::printf("signed time bytes=%zu; valid precision and INT32_MAX preserved\n", sizeof(ActualTimeMillis));
    return 0;
}
