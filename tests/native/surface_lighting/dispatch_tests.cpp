#include "../../../src/engine/surface_backend_config.h"
#include <cstdio>

int main() {
#if !CSE_USE_SCALAR_SURFACE_LIGHTING
    int registers[4] = {};
    __cpuid(registers, 1);
    constexpr int MmxFeatureBit = 1 << 23;
    const bool expected = (registers[3] & MmxFeatureBit) != 0;
#else
    constexpr bool expected = true;
#endif
    if (CsePackedSurfaceLightingAvailable() != expected) {
        std::fputs("Packed surface implementation availability mismatch\n", stderr);
        return 1;
    }
    return 0;
}
