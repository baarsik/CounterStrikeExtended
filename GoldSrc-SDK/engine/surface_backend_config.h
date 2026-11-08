#ifndef CSE_SURFACE_BACKEND_CONFIG_H
#define CSE_SURFACE_BACKEND_CONFIG_H

#ifndef CSE_USE_SCALAR_SURFACE_LIGHTING
#if defined(_WIN32) && defined(_M_IX86)
#define CSE_USE_SCALAR_SURFACE_LIGHTING 0
#else
#define CSE_USE_SCALAR_SURFACE_LIGHTING 1
#endif
#endif

#if !CSE_USE_SCALAR_SURFACE_LIGHTING
#include <windows.h>
#include <intrin.h>
#include <mmintrin.h>
#endif

// The legacy renderer flag describes availability of the packed lighting path.
// Native scalar lighting needs no MMX hardware; CPUInformation still reports actual instruction features.
inline bool CsePackedSurfaceLightingAvailable() {
#if CSE_USE_SCALAR_SURFACE_LIGHTING
    return true;
#else
    constexpr int MmxFeatureBit = 1 << 23;
    int registers[4] = {};
    __try {
        __cpuid(registers, 1);
        if ((registers[3] & MmxFeatureBit) != 0) {
            _mm_empty();
            return true;
        }
    }
#pragma warning(suppress: 6320)
    __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
    return false;
#endif
}
#endif
