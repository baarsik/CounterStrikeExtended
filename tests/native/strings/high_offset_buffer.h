#pragma once

#include <cstddef>
#include <cstdint>
#if defined(_WIN32)
#if !defined(WIN32_LEAN_AND_MEAN)
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <sys/mman.h>
#endif

// A real writable mapping far from the string base prevents low allocations from hiding truncation.
class HighOffsetBuffer {
public:
    explicit HighOffsetBuffer(const char *base) {
#if UINTPTR_MAX > UINT32_MAX
        constexpr std::uint64_t firstHint = UINT64_C(64) << 30;
        constexpr unsigned int maxAttempts = 8;
        const auto baseAddress = reinterpret_cast<std::uintptr_t>(base);
        for (unsigned int attempt = 0; attempt < maxAttempts; ++attempt) {
            auto *hint = reinterpret_cast<void *>(static_cast<std::uintptr_t>(firstHint << attempt));
            char *candidate = Map(hint);
            if (!candidate) {
                continue;
            }
            const auto address = reinterpret_cast<std::uintptr_t>(candidate);
            const auto distance = address > baseAddress ? address - baseAddress : baseAddress - address;
            if (distance > UINT32_MAX) {
                data_ = candidate;
                break;
            }
            Unmap(candidate);
        }
#else
        (void)base;
        data_ = Map(nullptr);
#endif
    }

    ~HighOffsetBuffer() {
        if (data_) {
            Unmap(data_);
        }
    }

    char *get() const {
        return data_;
    }

    char &operator[](std::size_t index) const {
        return data_[index];
    }

    HighOffsetBuffer(const HighOffsetBuffer &) = delete;
    HighOffsetBuffer &operator=(const HighOffsetBuffer &) = delete;

private:
    static constexpr std::size_t MappingBytes = 65536;
    char *data_ = nullptr;

    static char *Map(void *hint) {
#if defined(_WIN32)
        return static_cast<char *>(VirtualAlloc(hint, MappingBytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
#else
        void *mapping = mmap(hint, MappingBytes, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
        return mapping == MAP_FAILED ? nullptr : static_cast<char *>(mapping);
#endif
    }

    static void Unmap(char *mapping) {
#if defined(_WIN32)
        VirtualFree(mapping, 0, MEM_RELEASE);
#else
        munmap(mapping, MappingBytes);
#endif
    }
};
