#include <cstdint>
#include <cstdio>
#include "../../../src/common/native_int_count.h"

int main() {
    int result = -1;
    if (!cse::counts::TryIntCount(0, result) || result != 0
        || !cse::counts::TryIntCount(INT_MAX, result) || result != INT_MAX
        || cse::counts::TryIntCount(static_cast<std::size_t>(INT_MAX) + 1, result) || result != INT_MAX
        || cse::counts::TryIntCount(SIZE_MAX, result) || result != INT_MAX
        || cse::counts::IntCount(17) != 17 || cse::counts::AddIntCount(17, 4) != 21
        || !cse::counts::TryAddIntCount(0, INT_MAX, result) || result != INT_MAX
        || cse::counts::TryAddIntCount(INT_MAX, 1, result) || result != INT_MAX
        || cse::counts::TryAddIntCount(-1, 0, result) || result != INT_MAX) {
        return 1;
    }
    std::uint32_t unsignedResult = 0;
    if (!cse::counts::TryUint32Count(UINT32_MAX, unsignedResult) || unsignedResult != UINT32_MAX
        || cse::counts::Uint32Count(17) != 17) {
        return 1;
    }
#if SIZE_MAX > UINT32_MAX
    if (cse::counts::TryUint32Count(static_cast<std::size_t>(UINT32_MAX) + 1, unsignedResult)
        || unsignedResult != UINT32_MAX) {
        return 1;
    }
#endif
    std::puts("Legacy index bounds reject native count truncation");
    return 0;
}
