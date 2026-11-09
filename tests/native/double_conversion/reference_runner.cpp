#include "binary_io.h"
#include "corpus.h"
#include "reference_clock.h"

#include <cstdint>
#include <cstdio>
#include <string>

int main(int argc, char** argv) {
    using namespace double_conversion_fixture;
    Require(argc == 2, "provide a new reference output prefix");
    const std::string prefix = argv[1];
    std::FILE* direct = std::fopen((prefix + "-direct.bin").c_str(), "wb");
    std::FILE* methods = std::fopen((prefix + "-method.bin").c_str(), "wb");
    std::FILE* scaled = std::fopen((prefix + "-scaled.bin").c_str(), "wb");
    Require(direct != nullptr && methods != nullptr && scaled != nullptr, "open reference corpus files");
    CSystem clock;
    int checked = 0;
    int scaledDisagreements = 0;
    ForEachBits([&](std::uint64_t bits) {
        const double value = FromBits(bits);
        const std::int32_t rawResult = ReferenceRawDoubleCast(value);
        const double seconds = value / 1000.0;
        SetReferenceSeconds(seconds);
        const std::int32_t methodResult = static_cast<std::int32_t>(clock.GetTimeMillis());
        const double scaledValue = ReferenceScaleSeconds(seconds);
        const std::int32_t roundedScaledResult = ReferenceRawDoubleCast(scaledValue);
        scaledDisagreements += methodResult != roundedScaledResult ? 1 : 0;
        WriteBits(direct, static_cast<std::uint32_t>(rawResult));
        WriteBits(methods, static_cast<std::uint32_t>(methodResult));
        WriteBits(scaled, ToBits(scaledValue));
        ++checked;
    });
    Require(std::fclose(direct) == 0 && std::fclose(methods) == 0 && std::fclose(scaled) == 0,
        "close reference corpora");
    std::printf("reference_samples=%d method_vs_rounded_scaled_differences=%d\n", checked, scaledDisagreements);
    return 0;
}
