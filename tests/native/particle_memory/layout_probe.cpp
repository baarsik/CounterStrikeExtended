#include "pman_main.h"
#include "../../../GoldSrc-SDK/cstrike/cl_dll/tri.h"

#include <cstdio>

int main() {
    std::printf("pointer=%zu MemoryBlock=%zu Core=%zu Base=%zu Smoke=%zu Rain=%zu Snow=%zu Grass=%zu Wind=%zu\n",
        sizeof(void*), sizeof(MemoryBlock), sizeof(CCoreTriangleEffect), sizeof(CBaseParticle),
        sizeof(CPartSmokeGrenade), sizeof(CPartRainDrop), sizeof(CPartSnowFlake), sizeof(CPartGrassPiece),
        sizeof(CPartWind));
    constexpr int originalPoolBudgetBytes = 30000;
    std::printf("Original unscaled native formula capacity=%zu\n", originalPoolBudgetBytes / sizeof(MemoryBlock));
    return 0;
}
