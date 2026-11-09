#include "pman_main.h"
#include "pman_misc.h"
#include "native_particle_phase.h"
#include "../../../GoldSrc-SDK/cstrike/cl_dll/tri.h"

#include <climits>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <set>

namespace {

constexpr int expectedParticleSlots = 1875;
float clientTime = 1;
int overflowWarnings = 0;
int constructedParticles = 0;
int particleThinks = 0;
int particleDeaths = 0;

void Require(bool condition, const char* description) {
    if (!condition) {
        std::fprintf(stderr, "Particle pool failure: %s\n", description);
        std::exit(EXIT_FAILURE);
    }
}

float ClientTime() {
    return clientTime;
}

void OutOfMemoryWarning(char*, ...) {
    ++overflowWarnings;
}

void Prepare(IParticleMan_Active& manager) {
    g_pParticleMan = &manager;
    gEngfuncs.GetClientTime = ClientTime;
    gEngfuncs.Con_DPrintf = OutOfMemoryWarning;
    g_lMaxParticleClassSize = 0;
    MaxParticleClassSize(0);
    manager.AddCustomParticleClassSize(static_cast<unsigned long>(sizeof(CPartSmokeGrenade)));
    manager.SetRender(1);
    Require(CMiniMem::Instance()->MaxBlockSize() >= static_cast<long>(sizeof(CPartSmokeGrenade)),
        "native largest registered particle class fits the actual pool block");
}

void Finish() {
    CMiniMem::Instance()->Reset();
    CMiniMem::Instance()->Shutdown();
}

void TestCapacity(IParticleMan_Active& manager) {
    std::set<char*> addresses;
    for (int index = 0; index < expectedParticleSlots; ++index) {
        char* memory = manager.RequestNewMemBlock(static_cast<int>(sizeof(CBaseParticle)));
        if (!memory) {
            std::fprintf(stderr, "Observed pool exhaustion after %d slots\n", index);
        }
        Require(memory != nullptr && addresses.insert(memory).second,
            "all 1875 logical slots own distinct native storage");
    }
    Require(CMiniMem::Instance()->PercentUsed() == 0, "existing statistic reports zero percent free when full");
    Require(manager.RequestNewMemBlock(static_cast<int>(sizeof(CBaseParticle))) == nullptr
        && overflowWarnings == 1, "pool exhaustion retains null and one diagnostic");
    manager.ResetParticles();
    Require(CMiniMem::Instance()->PercentUsed() == 100, "reset returns every active block to the free list");
    char* reused = manager.RequestNewMemBlock(static_cast<int>(sizeof(CBaseParticle)));
    Require(addresses.count(reused) == 1, "reset reuses the same pool-owned storage");
    std::printf("Actual particle slots: %zu\n", addresses.size());
}

#if !defined(_WIN32)
void TestCoreAllocation(IParticleMan_Active& manager) {
    const long freeBefore = CMiniMem::Instance()->PercentUsed();
    Require(CCoreTriangleEffect::operator new(sizeof(CCoreTriangleEffect) + 1) == nullptr,
        "native core rejects a mismatched class size before taking pool storage");
    Require(CCoreTriangleEffect::operator new(std::numeric_limits<std::size_t>::max()) == nullptr
        && CMiniMem::Instance()->PercentUsed() == freeBefore,
        "native core cannot truncate a maximum-size allocation into a valid block");
    std::set<void*> addresses;
    for (int index = 0; index < expectedParticleSlots; ++index) {
        void* memory = CCoreTriangleEffect::operator new(sizeof(CCoreTriangleEffect));
        Require(memory != nullptr && addresses.insert(memory).second,
            "core allocation returns each actual native pool block exactly once");
    }
    Require(CCoreTriangleEffect::operator new(sizeof(CCoreTriangleEffect)) == nullptr && overflowWarnings == 1,
        "native core pool exhaustion preserves its null result and one diagnostic");
    manager.ResetParticles();
    Require(CMiniMem::Instance()->PercentUsed() == 100, "core allocations reset to the same free pool");
}
#endif

class CountingParticle final : public CBaseParticle {
public:
    CountingParticle() {
        ++constructedParticles;
        m_flDieTime = 2;
    }

    bool CheckVisibility() override {
        return false;
    }

    void Think(float) override {
        ++particleThinks;
    }

    void Die() override {
        ++particleDeaths;
    }
};

void TestConstruction(IParticleMan_Active& manager) {
    manager.SetRender(0);
    CountingParticle* failed = new CountingParticle;
    Require(failed == nullptr && constructedParticles == 0, "render-disabled null allocation skips construction");
    manager.SetRender(1);
    CountingParticle* particle = new CountingParticle;
    Require(particle != nullptr && constructedParticles == 1,
        "actual manager constructs a native particle in its block");
    CMiniMem::Instance()->ProcessAll();
    Require(CMiniMem::Instance()->GetTotalParticles() == 1 && CMiniMem::Instance()->GetDrawnParticles() == 0
        && particleThinks == 1 && particleDeaths == 0, "one invisible active particle thinks without drawing");
    clientTime = 2;
    CMiniMem::Instance()->ProcessAll();
    Require(particleDeaths == 1 && CMiniMem::Instance()->PercentUsed() == 100,
        "ordinary death returns its actual block to the pool");
    CMiniMem::Instance()->ProcessAll();
    Require(CMiniMem::Instance()->GetTotalParticles() == 0, "next processing cycle reports no remaining particles");
    particle = new CountingParticle;
    Require(particle != nullptr, "allocate another live particle before reset");
    manager.ResetParticles();
    Require(particleDeaths == 1 && CMiniMem::Instance()->PercentUsed() == 100,
        "reset preserves its existing behavior of recycling storage without a Die callback");
}

void TestPhase() {
    using cse::particle_memory::SpiralAddressPhase;
    Require(SpiralAddressPhase(0) == 0 && SpiralAddressPhase(INT32_MAX) == INT32_MAX,
        "nonnegative Win32 phase bits retain their signed values");
    constexpr std::uint32_t signBit = std::uint32_t{1} << 31;
    Require(SpiralAddressPhase(signBit) == std::numeric_limits<std::int32_t>::min()
        && SpiralAddressPhase(UINT32_MAX) == -1, "high-bit Win32 phases retain the signed bit pattern");
#if UINTPTR_MAX > UINT32_MAX
    constexpr std::uintptr_t highAddressBits = std::uintptr_t{0x1234} << 32;
    for (const std::uint32_t low : {std::uint32_t{0}, std::uint32_t{17}, signBit, UINT32_MAX}) {
        Require(SpiralAddressPhase(highAddressBits | low) == SpiralAddressPhase(low),
            "native addresses with equal low 32 bits retain equal spiral phases");
    }
#endif
}

}

int main(int argc, char** argv) {
    Require(argc == 2, "select an actual pool scenario");
    if (std::strcmp(argv[1], "phase") == 0) {
        TestPhase();
    } else {
        IParticleMan_Active manager;
#if !defined(_WIN32)
        if (std::strcmp(argv[1], "core-allocation") == 0) {
            Prepare(manager);
            TestCoreAllocation(manager);
            Finish();
            return EXIT_SUCCESS;
        }
#endif
        if (std::strcmp(argv[1], "restart") == 0) {
            constexpr int cycles = 64;
            for (int cycle = 0; cycle < cycles; ++cycle) {
                Prepare(manager);
                Require(manager.RequestNewMemBlock(static_cast<int>(sizeof(CBaseParticle))) != nullptr,
                    "fresh pool instance can allocate after the previous instance detached");
                Finish();
            }
        } else {
            Prepare(manager);
            if (std::strcmp(argv[1], "capacity") == 0) {
                TestCapacity(manager);
            } else if (std::strcmp(argv[1], "exit-active") == 0) {
                Require(manager.RequestNewMemBlock(static_cast<int>(sizeof(CBaseParticle))) != nullptr,
                    "module lifetime owns an active block at process exit");
                std::puts("Actual particle pool exit-active passed");
                return EXIT_SUCCESS;
            } else {
                Require(std::strcmp(argv[1], "construction") == 0, "known pool scenario");
                TestConstruction(manager);
            }
            Finish();
        }
    }
    std::printf("Actual particle pool %s passed\n", argv[1]);
    return EXIT_SUCCESS;
}
