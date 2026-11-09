#include "allocation_fault.h"
#include "fake_sdl.h"

#include <atomic>
#include <cstdlib>
#include <new>

namespace {

std::atomic<bool> failNext{false};
std::atomic<unsigned int> callbackOperations{0};

void ObserveOperation() {
    if (fake_sdl::InsideCallback()) {
        ++callbackOperations;
    }
}

void* Allocate(std::size_t bytes) noexcept {
    ObserveOperation();
    if (failNext.exchange(false)) {
        return nullptr;
    }
    return std::malloc(bytes == 0 ? 1 : bytes);
}

void Free(void* memory) noexcept {
    ObserveOperation();
    std::free(memory);
}

}

namespace allocation_fault {

void FailNext() {
    failNext = true;
}

unsigned int CallbackOperations() {
    return callbackOperations;
}

}

void* operator new(std::size_t bytes) {
    if (void* memory = Allocate(bytes)) {
        return memory;
    }
    throw std::bad_alloc();
}

void* operator new[](std::size_t bytes) {
    return ::operator new(bytes);
}

void* operator new(std::size_t bytes, const std::nothrow_t&) noexcept {
    return Allocate(bytes);
}

void* operator new[](std::size_t bytes, const std::nothrow_t&) noexcept {
    return Allocate(bytes);
}

void operator delete(void* memory) noexcept {
    Free(memory);
}

void operator delete[](void* memory) noexcept {
    Free(memory);
}

void operator delete(void* memory, const std::nothrow_t&) noexcept {
    Free(memory);
}

void operator delete[](void* memory, const std::nothrow_t&) noexcept {
    Free(memory);
}

#if defined(__cpp_sized_deallocation)
void operator delete(void* memory, std::size_t) noexcept {
    Free(memory);
}

void operator delete[](void* memory, std::size_t) noexcept {
    Free(memory);
}
#endif
