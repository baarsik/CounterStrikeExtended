#include "binary_io.h"
#include "corpus.h"
#include "retained_boundary_results.h"

#if !defined(CSE_DOUBLE_NATIVE_CAST_BASELINE)
#include "../../../src/common/legacy_double_to_int.h"
#endif

#include <cstdint>
#include <cstdio>

#if defined(CSE_DOUBLE_NATIVE_CAST_BASELINE)
#if defined(_MSC_VER)
__declspec(noinline)
#else
__attribute__((noinline))
#endif
std::int32_t BaselineNativeCast(double value) {
    return static_cast<std::int32_t>(value);
}
#endif

int main() {
    using namespace double_conversion_fixture;
    int checked = 0;
    for (const RetainedDoubleResult& entry : RetainedDoubleResults) {
        const double value = FromBits(entry.inputBits);
#if defined(CSE_DOUBLE_NATIVE_CAST_BASELINE)
        const std::int32_t actual = BaselineNativeCast(value);
#else
        const std::int32_t actual = CseLegacyTruncateDoubleToInt(value);
        Require(static_cast<std::uint32_t>(CseScalarTruncateDoubleToInt(value)) == entry.expectedBits,
            "scalar matches the actual retained boundary result");
#endif
        if (static_cast<std::uint32_t>(actual) != entry.expectedBits) {
            std::fprintf(stderr, "bits=%016llx actual=%08x expected=%08x\n",
                static_cast<unsigned long long>(entry.inputBits), static_cast<std::uint32_t>(actual),
                entry.expectedBits);
            Require(false, "conversion matches actual retained Win32 result");
        }
        ++checked;
    }
    std::printf("retained boundary results matched=%d\n", checked);
    return 0;
}
