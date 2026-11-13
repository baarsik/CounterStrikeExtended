#include "../../../src/common/legacy_double_to_int.h"

#include <cstdint>

#if defined(_MSC_VER)
#define CSE_DOUBLE_NOINLINE __declspec(noinline)
#else
#define CSE_DOUBLE_NOINLINE __attribute__((noinline))
#endif

CSE_DOUBLE_NOINLINE std::int32_t ScalarDoubleCall(double value) {
    return CseScalarTruncateDoubleToInt(value);
}

CSE_DOUBLE_NOINLINE std::int32_t NativeDoubleCall(double value) {
    return CseLegacyTruncateDoubleToInt(value);
}
