#include <windows.h>
#include "tier0/memalloc.h"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

static void Require(bool value, const char* message) {
    if (!value) {
        std::fprintf(stderr, "tier0_allocator_failure=%s\n", message);
        std::exit(1);
    }
}

struct Allocation {
    void* pointer;
    std::size_t bytes;
    unsigned char pattern;
};

int main(int count, char* arguments[]) {
    Require(count == 3, "module and scenario arguments");
    std::puts("tier0_allocator_before_load=1");
    std::fflush(stdout);
    const HMODULE library = LoadLibraryA(arguments[1]);
    Require(library != nullptr, "actual complete module loads and finishes static initialization");
    auto** exported = reinterpret_cast<IMemAlloc**>(GetProcAddress(library, "g_pMemAlloc"));
    Require(exported != nullptr && *exported != nullptr, "real exported allocator");
    IMemAlloc* allocator = *exported;
    const std::string scenario = arguments[2];
#if defined(_WIN64)
    constexpr std::size_t RequiredAlignment = MEMORY_ALLOCATION_ALIGNMENT;
#else
    constexpr std::size_t RequiredAlignment = 4; // Retained Win32 small-block storage contract.
#endif
    const auto check = [&](void* pointer, std::size_t bytes) {
        Require(pointer != nullptr, "allocation succeeds");
        Require(reinterpret_cast<std::uintptr_t>(pointer) % RequiredAlignment == 0, "storage alignment");
        Require(allocator->GetSize(pointer) >= bytes, "reported capacity covers the requested bytes");
#if defined(_WIN64)
        Require(reinterpret_cast<std::uintptr_t>(pointer) > UINT32_MAX, "actual allocation above4GiB");
#endif
    };
    if (scenario == "sizes") {
        constexpr std::size_t MaximumSmallBlockBytes = 2048;
        constexpr std::size_t AllocationsPerSize = 3;
        std::vector<Allocation> allocations;
        for (std::size_t bytes = 0; bytes <= MaximumSmallBlockBytes + 1; ++bytes) {
            for (std::size_t index = 0; index < AllocationsPerSize; ++index) {
                void* pointer = allocator->Alloc(bytes);
                check(pointer, bytes);
                const auto pattern = static_cast<unsigned char>((bytes + index) % 251);
                std::memset(pointer, pattern, bytes);
                allocations.push_back({pointer, bytes, pattern});
            }
        }
        for (const auto& allocation : allocations) {
            const auto* bytes = static_cast<const unsigned char*>(allocation.pointer);
            for (std::size_t index = 0; index < allocation.bytes; ++index) {
                Require(bytes[index] == allocation.pattern, "active allocations do not overlap");
            }
            allocator->Free(allocation.pointer);
        }
    } else if (scenario == "reuse") {
        void* first = allocator->Alloc(1);
        void* second = allocator->Alloc(1);
        void* neighbor = allocator->Alloc(1);
        check(first, 1);
        check(second, 1);
        check(neighbor, 1);
        *static_cast<unsigned char*>(neighbor) = 0x5A;
        allocator->Free(first);
        allocator->Free(second);
        void* reusedSecond = allocator->Alloc(1);
        void* reusedFirst = allocator->Alloc(1);
        Require(reusedSecond == second && reusedFirst == first, "full-width free links retain LIFO identity");
        Require(*static_cast<unsigned char*>(neighbor) == 0x5A, "free links do not overwrite active neighbor");
        allocator->Free(reusedSecond);
        allocator->Free(reusedFirst);
        allocator->Free(neighbor);
    } else if (scenario == "realloc") {
        const std::size_t sizes[] = {1, 4, 8, 15, 16, 17, 63, 256, 2048, 2049, 13, 0, 1024};
        void* pointer = nullptr;
        std::size_t oldBytes = 0;
        for (const auto bytes : sizes) {
            pointer = allocator->Realloc(pointer, bytes);
            // A zero-size CRT reallocation may free an outside-pool block and return null.
            if (bytes == 0 && pointer == nullptr) {
                oldBytes = 0;
                continue;
            }
            check(pointer, bytes);
            const std::size_t preserved = oldBytes < bytes ? oldBytes : bytes;
            for (std::size_t index = 0; index < preserved; ++index) {
                Require(static_cast<unsigned char*>(pointer)[index] == 0x37, "reallocation preserves prefix bytes");
            }
            std::memset(pointer, 0x37, bytes);
            oldBytes = bytes;
        }
        allocator->Free(pointer);
    } else {
        Require(scenario == "startup", "known scenario");
    }
    Require(FreeLibrary(library) != FALSE, "complete module unloads");
    std::puts("tier0_allocator_passed=1");
    return 0;
}
