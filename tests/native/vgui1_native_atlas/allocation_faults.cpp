#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <new>

namespace {
std::atomic<bool> failNext{false};
void* Allocate(std::size_t bytes) {
    if (failNext.exchange(false)) {
        std::fputs("CONTROLLED standard allocation throws std::bad_alloc\n", stderr);
        throw std::bad_alloc();
    }
    void* memory = std::malloc(bytes ? bytes : 1);
    if (!memory) {
        throw std::bad_alloc();
    }
    return memory;
}
}
void* operator new(std::size_t bytes) { return Allocate(bytes); }
void* operator new[](std::size_t bytes) { return Allocate(bytes); }
void operator delete(void* memory) noexcept { std::free(memory); }
void operator delete[](void* memory) noexcept { std::free(memory); }
void operator delete(void* memory, std::size_t) noexcept { std::free(memory); }
void operator delete[](void* memory, std::size_t) noexcept { std::free(memory); }
extern "C" void FixtureFailNextAllocation() { failNext = true; }
