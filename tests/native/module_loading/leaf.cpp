#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <cstring>

#if defined(_WIN32)
#define FIXTURE_EXPORT __declspec(dllexport)
#else
#define FIXTURE_EXPORT __attribute__((visibility("default")))
#endif

namespace {
constexpr std::uint64_t LeafIdentity = UINT64_C(0x4353454c45414631);
std::uint64_t leafValue = LeafIdentity;

void Trace(char event) {
    const char* path = std::getenv("CSE_MODULE_FIXTURE_TRACE");
    if (!path) {
        std::abort();
    }
    std::FILE* file = std::fopen(path, "ab");
    if (!file || std::fputc(event, file) == EOF || std::fclose(file) != 0) {
        std::abort();
    }
}

struct ModuleLifetime {
    ModuleLifetime() {
        Trace('C');
    }
    ~ModuleLifetime() {
        Trace('D');
    }
};
ModuleLifetime lifetime;
} // namespace

extern "C" FIXTURE_EXPORT void* CreateInterface(const char* name, int* status) {
    const bool found = name && std::strcmp(name, "FixtureLeaf001") == 0;
    if (status) {
        *status = found ? 0 : 1;
    }
    return found ? &leafValue : nullptr;
}

extern "C" FIXTURE_EXPORT int CseLeafPing(int value) {
    constexpr int PingIncrement = 7;
    return value + PingIncrement;
}

extern "C" {
#if defined(_WIN32)
// The definition file owns the Windows data ordinal; avoid a duplicate automatic export.
std::uint64_t CseLeafData = UINT64_C(0x4353454c45414631);
#else
FIXTURE_EXPORT std::uint64_t CseLeafData = UINT64_C(0x4353454c45414631);
#endif
}
