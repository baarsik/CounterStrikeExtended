#include "binary_io.h"
#include "corpus.h"
#include "../../../GoldSrc-SDK/common/legacy_double_to_int.h"

#include <cstdint>
#include <cstdio>
#include <string>

int main(int argc, char** argv) {
    using namespace double_conversion_fixture;
    Require(argc == 3, "provide frozen Win32 golden and new output prefixes");
    const std::string golden = argv[1];
    const std::string output = argv[2];
    std::FILE* direct = std::fopen((golden + "-direct.bin").c_str(), "rb");
    std::FILE* methods = std::fopen((golden + "-method.bin").c_str(), "rb");
    std::FILE* scaled = std::fopen((golden + "-scaled.bin").c_str(), "rb");
    std::FILE* directOutput = std::fopen((output + "-direct.bin").c_str(), "wb");
    std::FILE* methodOutput = std::fopen((output + "-method.bin").c_str(), "wb");
    Require(direct != nullptr && methods != nullptr && scaled != nullptr
        && directOutput != nullptr && methodOutput != nullptr, "open frozen goldens and result outputs");
    int checked = 0;
    ForEachBits([&](std::uint64_t bits) {
        const double value = FromBits(bits);
        const std::uint32_t expected = ReadBits<std::uint32_t>(direct);
        const std::uint32_t scalar = static_cast<std::uint32_t>(CseScalarTruncateDoubleToInt(value));
        const std::uint32_t native = static_cast<std::uint32_t>(CseLegacyTruncateDoubleToInt(value));
        if (expected != scalar || expected != native) {
            std::fprintf(stderr, "direct index=%d bits=%016llx expected=%08x scalar=%08x native=%08x\n",
                checked, static_cast<unsigned long long>(bits), expected, scalar, native);
            Require(false, "every encoded direct result matches the measured Win32 golden");
        }
        const std::uint64_t scaledBits = ReadBits<std::uint64_t>(scaled);
        const double scaledValue = FromBits(scaledBits);
        const std::uint32_t methodExpected = ReadBits<std::uint32_t>(methods);
        const std::uint32_t methodScalar = static_cast<std::uint32_t>(CseScalarTruncateDoubleToInt(scaledValue));
        const std::uint32_t methodNative = static_cast<std::uint32_t>(CseLegacyTruncateDoubleToInt(scaledValue));
        if (methodExpected != methodScalar || methodExpected != methodNative) {
            std::fprintf(stderr, "method index=%d scaled=%016llx expected=%08x scalar=%08x native=%08x\n",
                checked, static_cast<unsigned long long>(scaledBits), methodExpected, methodScalar, methodNative);
            Require(false, "every conversion of captured scaled bits matches the actual Win32 method");
        }
        WriteBits(directOutput, native);
        WriteBits(methodOutput, methodNative);
        ++checked;
    });
    Require(std::fgetc(direct) == EOF && std::fgetc(methods) == EOF && std::fgetc(scaled) == EOF,
        "consume every golden record without trailing data");
    Require(std::fclose(direct) == 0 && std::fclose(methods) == 0 && std::fclose(scaled) == 0,
        "close frozen golden inputs");
    Require(std::fclose(directOutput) == 0 && std::fclose(methodOutput) == 0, "close encoded result outputs");
    std::printf("direct_and_actual_method_samples_each=%d scalar_and_native_encoded_results_match=1\n", checked);
    return 0;
}
