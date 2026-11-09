#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {
constexpr std::size_t StorageBytes = 64;
unsigned char storage[StorageBytes];
std::size_t requestedBytes = 0;
bool failAllocation = false;

void* ControlledMalloc(std::size_t bytes) {
    requestedBytes = bytes;
    if (failAllocation) {
        return nullptr;
    }
    if (bytes > sizeof(storage)) {
        std::abort();
    }
    std::memset(storage, 0x5a, sizeof(storage));
    return storage;
}
}

// Only allocation is controlled. The entire actual production function and its
// memset, error diagnostic and termination path are compiled unchanged.
#define malloc ControlledMalloc
#include "mem.c"
#undef malloc

int main(int argc, char**) {
    failAllocation = argc > 1;
    if (failAllocation) {
        Mem_ZeroMalloc(StorageBytes);
        std::fputs("FAIL: allocation failure returned\n", stderr);
        return 1;
    }
    for (std::size_t bytes = 0; bytes <= sizeof(storage); ++bytes) {
        void* result = Mem_ZeroMalloc(bytes);
        if (result != storage || requestedBytes != bytes) {
            return 2;
        }
        for (std::size_t index = 0; index < sizeof(storage); ++index) {
            if (storage[index] != (index < bytes ? 0 : 0x5a)) {
                return 3;
            }
        }
    }
    std::puts("PASS: exact zeroed region, allocation size, identity and retained tail");
}
