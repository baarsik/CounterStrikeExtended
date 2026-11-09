#include "pman_main.h"

#include <climits>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <new>

IParticleMan* g_pParticleMan = nullptr;

namespace {

void Require(bool condition, const char* description) {
    if (!condition) {
        std::fprintf(stderr, "Particle boundary failure: %s\n", description);
        std::exit(EXIT_FAILURE);
    }
}

class RecordingManager : public IParticleMan {
public:
    void SetUp(cl_enginefunc_t*) override {}
    void Update() override {}
    void SetVariables(float, Vector) override {}
    void ResetParticles() override {}
    void ApplyForce(Vector, Vector, float, float, float) override {}
    void AddCustomParticleClassSize(unsigned long) override {}
    CBaseParticle* CreateParticle(Vector, Vector, model_s*, float, float, const char*) override {
        return nullptr;
    }
    char* RequestNewMemBlock(int size) override {
        ++requests;
        requestedBytes = size;
        if (fail) {
            return nullptr;
        }
        memory = static_cast<char*>(std::malloc(static_cast<std::size_t>(size)));
        Require(memory != nullptr, "allocate actual small fixture storage");
        return memory;
    }
    void CoreInitializeSprite(CCoreTriangleEffect*, Vector, Vector, model_s*, float, float) override {}
    void CoreThink(CCoreTriangleEffect*, float) override {}
    void CoreDraw(CCoreTriangleEffect*) override {}
    void CoreAnimate(CCoreTriangleEffect*, float) override {}
    void CoreAnimateAndDie(CCoreTriangleEffect*, float) override {}
    void CoreExpand(CCoreTriangleEffect*, float) override {}
    void CoreContract(CCoreTriangleEffect*, float) override {}
    void CoreFade(CCoreTriangleEffect*, float) override {}
    void CoreSpin(CCoreTriangleEffect*, float) override {}
    void CoreCalculateVelocity(CCoreTriangleEffect*, float) override {}
    void CoreCheckCollision(CCoreTriangleEffect*, float) override {}
    void CoreTouch(CCoreTriangleEffect*, Vector, Vector, int) override {}
    void CoreDie(CCoreTriangleEffect*) override {}
    void CoreForce(CCoreTriangleEffect*) override {}
    bool CoreCheckVisibility(CCoreTriangleEffect*) override {
        return false;
    }
    void SetRender(int) override {}

    void Release() {
        std::free(memory);
        memory = nullptr;
    }

    int requests = 0;
    int requestedBytes = 0;
    char* memory = nullptr;
    bool fail = true;
};

int constructedParticles = 0;

class CountingParticle final : public CBaseParticle {
public:
    CountingParticle() {
        ++constructedParticles;
    }
};

}

int main() {
    static_assert(noexcept(CBaseParticle::operator new(0)), "Pool failures require a nonthrowing allocation function");
#if !defined(_WIN32)
    static_assert(noexcept(CCoreTriangleEffect::operator new(0)),
        "The native core particle null-result allocator must also be nonthrowing");
#endif
    RecordingManager manager;
    g_pParticleMan = &manager;
    constexpr std::size_t maximumDispatchBytes = static_cast<std::size_t>(INT_MAX);
    Require(CBaseParticle::operator new(maximumDispatchBytes) == nullptr,
        "representable failed allocation retains the pool's null result");
    Require(manager.requests == 1 && manager.requestedBytes == INT_MAX,
        "largest int request reaches the virtual boundary exactly");
    Require(CBaseParticle::operator new(maximumDispatchBytes + 1) == nullptr && manager.requests == 1,
        "request above INT_MAX fails before dispatch");
    Require(CBaseParticle::operator new(std::numeric_limits<std::size_t>::max()) == nullptr && manager.requests == 1,
        "maximum native-size request cannot wrap through the virtual boundary");
#if SIZE_MAX > UINT32_MAX
    constexpr std::size_t highRequestBytes = (std::size_t{1} << 32) + sizeof(CountingParticle);
    Require(CBaseParticle::operator new(highRequestBytes) == nullptr && manager.requests == 1,
        "request above 4 GiB cannot truncate to a valid small class");
#endif
    CountingParticle* failed = new CountingParticle;
    Require(failed == nullptr && constructedParticles == 0,
        "null pool failure skips construction without throwing");
    manager.fail = false;
    CountingParticle* particle = new CountingParticle;
    Require(particle != nullptr && static_cast<void*>(particle) == manager.memory && constructedParticles == 1,
        "native particle construction retains actual allocated storage identity");
    Require(manager.requestedBytes == static_cast<int>(sizeof(CountingParticle)),
        "native class size reaches pool intact");
    particle->~CountingParticle();
    manager.Release();
#if defined(POSIX)
    manager.fail = true;
    const int priorRequests = manager.requests;
    Require(CBaseParticle::operator new(maximumDispatchBytes + 1, std::nothrow) == nullptr
        && manager.requests == priorRequests, "nothrow form rejects unsupported width before dispatch");
    failed = new (std::nothrow) CountingParticle;
    Require(failed == nullptr && constructedParticles == 1, "nothrow pool failure skips construction");
#endif
    std::puts("Particle native-size and null-allocation boundary passed");
    return EXIT_SUCCESS;
}
