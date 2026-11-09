#pragma once

namespace cse {
// Engine-private heap load. The ordinary file hook runs once with its original
// null length argument. A rewritten non-heap mode is rejected before allocation.
// The independent output is not exposed to that hook and counts fully initialized
// file bytes, excluding the trailing NUL. Failure leaves it zero. Successful data
// belongs to the caller and must be released with Mem_Free.
#if !defined(_WIN32) && (defined(__GNUC__) || defined(__clang__))
__attribute__((visibility("hidden")))
#endif
unsigned char* LoadCustomizationFile(char* path, int* initializedBytes);
} // namespace cse
