#include "../../../src/common/legacy_text_encoding.h"

#include <array>
#include <climits>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <stdexcept>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace {
void Require(bool value, const char* message) {
    if (!value) {
        throw std::runtime_error(message);
    }
}

struct GuardedPages {
    unsigned char* base = nullptr;
    unsigned char* middle = nullptr;
    std::size_t page = 0;

    GuardedPages() {
#if defined(_WIN32)
        SYSTEM_INFO system{};
        GetSystemInfo(&system);
        page = system.dwPageSize;
        if (sizeof(void*) > sizeof(std::uint32_t)) {
            const std::uint64_t hints[] = {0x1000000000ULL, 0x2000000000ULL, 0x3000000000ULL};
            for (const auto hint : hints) {
                base = static_cast<unsigned char*>(VirtualAlloc(reinterpret_cast<void*>(
                    static_cast<std::uintptr_t>(hint)), page * 3, MEM_RESERVE | MEM_COMMIT, PAGE_NOACCESS));
                if (base) {
                    break;
                }
            }
        } else {
            base = static_cast<unsigned char*>(VirtualAlloc(nullptr, page * 3,
                MEM_RESERVE | MEM_COMMIT, PAGE_NOACCESS));
        }
        Require(base != nullptr, "owned guard allocation failed");
        middle = base + page;
        DWORD previous = 0;
        Require(VirtualProtect(middle, page, PAGE_READWRITE, &previous) != 0, "owned guard protection failed");
#else
        const long size = sysconf(_SC_PAGESIZE);
        Require(size > 0, "page size unavailable");
        page = static_cast<std::size_t>(size);
        void* allocation = mmap(nullptr, page * 3, PROT_NONE, MAP_PRIVATE | MAP_ANON, -1, 0);
        Require(allocation != MAP_FAILED, "owned guard allocation failed");
        base = static_cast<unsigned char*>(allocation);
        middle = base + page;
        Require(mprotect(middle, page, PROT_READ | PROT_WRITE) == 0, "owned guard protection failed");
#endif
        if (sizeof(void*) > sizeof(std::uint32_t)) {
            Require(reinterpret_cast<std::uintptr_t>(middle) > UINT32_MAX, "buffer is not above 4GiB");
        }
    }

    ~GuardedPages() {
#if defined(_WIN32)
        if (base) {
            VirtualFree(base, 0, MEM_RELEASE);
        }
#else
        if (base) {
            munmap(base, page * 3);
        }
#endif
    }
};

template<typename Cell>
void UnitsAndBoundaries() {
    using namespace cse::legacy_text;
    const std::array<Cell, 3> pair{{static_cast<Cell>(0xd83d), static_cast<Cell>(0xde42), 0}};
    std::array<char, 8> output;
    output.fill('Z');
    Require(WideCellsToUtf8(pair.data(), output.data(), static_cast<int>(output.size())) == 5 &&
        std::memcmp(output.data(), "\xf0\x9f\x99\x82\0", 5) == 0 && output[5] == 'Z' && output[7] == 0,
        "supplementary pair changed");
    std::array<Cell, 8> wide;
    wide.fill(static_cast<Cell>(0x5a5a));
    Require(Utf8ToWideCells("\xf0\x9f\x99\x82", wide.data(), static_cast<int>(sizeof(wide))) == 3 &&
        static_cast<std::uint32_t>(wide[0]) == 0xd83d && static_cast<std::uint32_t>(wide[1]) == 0xde42 &&
        wide[2] == 0 && wide[3] == 0x5a5a && wide[7] == 0, "UTF8 output does not store UTF16 units");
    for (int bytes : {-1, INT_MIN, 0}) {
        output.fill('Z');
        wide.fill(static_cast<Cell>(0x5a5a));
        Require(WideCellsToUtf8(pair.data(), output.data(), bytes) == 0 && output[0] == 'Z' && output[7] == 'Z',
            "invalid byte capacity modified caller");
        Require(Utf8ToWideCells("A", wide.data(), bytes) == 0 && wide[0] == 0x5a5a && wide[7] == 0x5a5a,
            "invalid wide capacity modified caller");
    }
    GuardedPages source;
    GuardedPages destination;
    const char* truncated[] = {"\xe2", "\xe2\x82", "\xf0\x9f\x99", "\xed\xa0\x80", "\xf0\x9f\x99\x82"};
    for (const char* value : truncated) {
        const std::size_t count = std::strlen(value) + 1;
        char* input = reinterpret_cast<char*>(source.middle + source.page - count);
        std::memcpy(input, value, count);
        for (std::size_t cells = 0; cells <= 8; ++cells) {
            for (std::size_t remainder = 0; remainder < sizeof(Cell); ++remainder) {
                std::memset(destination.middle, 0x5a, destination.page);
                Cell* target = reinterpret_cast<Cell*>(destination.middle + destination.page - cells * sizeof(Cell));
                const int bytes = static_cast<int>(cells * sizeof(Cell) + remainder);
                Utf8ToWideCells(input, target, bytes);
                Require(reinterpret_cast<unsigned char*>(target)[-1] == 0x5a, "wide lower guard changed");
            }
        }
    }
    Cell* input = reinterpret_cast<Cell*>(source.middle + source.page - sizeof(pair));
    std::memcpy(input, pair.data(), sizeof(pair));
    for (int bytes = 0; bytes <= 8; ++bytes) {
        std::memset(destination.middle, 0x5a, destination.page);
        char* target = reinterpret_cast<char*>(destination.middle + destination.page - static_cast<std::size_t>(bytes));
        WideCellsToUtf8(input, target, bytes);
        Require(target[-1] == 'Z', "UTF8 lower guard changed");
    }
    input[0] = static_cast<Cell>(0xd800);
    input[1] = 0;
    output.fill('Z');
    Require(WideCellsToUtf8(input, output.data(), static_cast<int>(output.size())) == 4 &&
        std::memcmp(output.data(), "\xef\xbf\xbd\0", 4) == 0, "isolated surrogate repair changed");
}

void Native32Convenience() {
    using namespace cse::legacy_text;
    const std::int32_t scalar[] = {0x1f642, 0};
    char output[8] = {};
    Require(WideCellsToUtf8(scalar, output, sizeof(output)) == 5 &&
        std::memcmp(output, "\xf0\x9f\x99\x82\0", 5) == 0, "native32 scalar convenience failed");
    for (std::int32_t value : {-1, INT32_MIN, 0x110000, 0xd800, 0xdc00}) {
        const std::int32_t invalid[] = {value, 0};
        Require(WideCellsToUtf8(invalid, output, sizeof(output)) == 4 &&
            std::memcmp(output, "\xef\xbf\xbd\0", 4) == 0, "invalid native32 repair differs");
    }
}
} // namespace

int main() {
    try {
        UnitsAndBoundaries<std::uint16_t>();
        UnitsAndBoundaries<std::uint32_t>();
        Native32Convenience();
        std::printf("PASS UTF16 units, native32 repair, negative/odd capacities, guarded sources/destinations pointer_bytes=%zu\n",
            sizeof(void*));
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "FAIL %s\n", error.what());
        return 1;
    }
}
