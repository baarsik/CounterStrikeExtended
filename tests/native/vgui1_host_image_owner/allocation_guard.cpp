#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <new>

namespace {
std::atomic<bool> reject{false};
}

void RejectComponentNew(bool value) {
    reject.store(value);
}

void* operator new(std::size_t bytes) {
    if (reject.load()) {
        std::fputs("FAIL unexpected component C++ allocation\n", stderr);
        std::abort();
    }
    void* result = std::malloc(bytes ? bytes : 1);
    if (!result) {
        std::abort();
    }
    return result;
}

void* operator new[](std::size_t bytes) {
    return ::operator new(bytes);
}

void operator delete(void* pointer) noexcept {
    std::free(pointer);
}

void operator delete[](void* pointer) noexcept {
    std::free(pointer);
}

void operator delete(void* pointer, std::size_t) noexcept {
    std::free(pointer);
}

void operator delete[](void* pointer, std::size_t) noexcept {
    std::free(pointer);
}
