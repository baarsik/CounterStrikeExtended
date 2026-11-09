#include "clock.h"

#include <array>
#include <cfloat>
#include <cfenv>
#include <climits>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>

#if defined(_M_IX86) || defined(_M_X64)
#include <xmmintrin.h>
#endif

#include "clock_cases.inc"

namespace {

static_assert(CHAR_BIT == 8, "Timestamp wire bytes are eight bits");
static_assert(sizeof(float) == sizeof(std::uint32_t), "Writer stores binary32");
static_assert(sizeof(double) == sizeof(std::uint64_t), "Clock returns binary64");
static_assert(std::numeric_limits<float>::is_iec559, "Fixture requires IEEE binary32");
static_assert(std::numeric_limits<double>::is_iec559, "Fixture requires IEEE binary64");
constexpr unsigned int MxcsrStatusBits = 0x3f;
constexpr std::size_t SameTickWrites = 4;

class ControlledClock final : public Clock {
public:
    void SetBits(std::uint64_t bits) {
        double decoded = 0;
        std::memcpy(&decoded, &bits, sizeof(decoded));
        value = decoded;
    }

    double GetTime() override {
        return value;
    }

private:
    volatile double value = 0;
};

std::uint32_t Bits(float value) {
    std::uint32_t result = 0;
    std::memcpy(&result, &value, sizeof(result));
    return result;
}

unsigned int Mxcsr() {
#if defined(_M_IX86) || defined(_M_X64)
    return _mm_getcsr();
#else
    return 0;
#endif
}

} // namespace

int main() {
    unsigned int controlBefore = 0;
    if (_controlfp_s(&controlBefore, 0, 0) != 0) {
        return 2;
    }
    const int roundingBefore = std::fegetround();
    const unsigned int mxcsrBefore = Mxcsr();
    if (roundingBefore != FE_TONEAREST || (controlBefore & _MCW_RC) != _RC_NEAR) {
        return 3;
    }
    std::printf("profile msc=%d ptr=%zu round=%d control=%08x mxcsr=%08x\n", _MSC_VER,
                sizeof(void *), roundingBefore, controlBefore, mxcsrBefore);
    for (const ClockCase &sample : ClockCases) {
        ControlledClock clock;
        clock.SetBits(sample.clockBits);
        WriterExpressions writer(&clock);
        writer.Start();
        std::array<std::uint32_t, SameTickWrites> encoded{};
        for (std::uint32_t &value : encoded) {
            value = Bits(writer.EncodeTime());
        }
        clock.SetBits(sample.nextBits);
        const std::uint32_t advanced = Bits(writer.EncodeTime());
        std::printf("case %s %016llx %08x %08x %08x %08x %08x %016llx %08x\n", sample.name,
                    static_cast<unsigned long long>(sample.clockBits), Bits(writer.StartTime()),
                    encoded[0], encoded[1], encoded[2], encoded[3],
                    static_cast<unsigned long long>(sample.nextBits), advanced);
    }
    unsigned int controlAfter = 0;
    if (_controlfp_s(&controlAfter, 0, 0) != 0) {
        return 4;
    }
    const int roundingAfter = std::fegetround();
    const unsigned int mxcsrAfter = Mxcsr();
    std::printf("final round=%d control=%08x mxcsr=%08x\n", roundingAfter, controlAfter, mxcsrAfter);
    if (controlBefore != controlAfter || roundingBefore != roundingAfter ||
        ((mxcsrBefore ^ mxcsrAfter) & ~MxcsrStatusBits) != 0) {
        return 5;
    }
    return 0;
}
