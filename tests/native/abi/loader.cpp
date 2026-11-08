#include "fixture.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <iterator>
#include <memory>
#if defined(_WIN32)
#include <windows.h>
#else
#include <dlfcn.h>
#endif

class Module {
public:
    explicit Module(const char *path) {
#if defined(_WIN32)
        handle = LoadLibraryA(path);
#else
        handle = dlopen(path, RTLD_NOW | RTLD_LOCAL);
#endif
    }
    ~Module() {
        if (handle) {
#if defined(_WIN32)
            FreeLibrary(handle);
#else
            dlclose(handle);
#endif
        }
    }
    explicit operator bool() const { return handle != nullptr; }
    Module(const Module &) = delete;
    Module &operator=(const Module &) = delete;
    template<typename Function> Function Find(const char *name) const {
#if defined(_WIN32)
        return reinterpret_cast<Function>(GetProcAddress(handle, name));
#else
        return reinterpret_cast<Function>(dlsym(handle, name));
#endif
    }
private:
#if defined(_WIN32)
    HMODULE handle = nullptr;
#else
    void *handle = nullptr;
#endif
};

static bool CheckModule(const char *path, cse::abi::Compatibility expected) {
    Module module(path);
    if (!module) {
        std::fprintf(stderr, "Failed to load owned ABI fixture: %s\n", path);
        return false;
    }
    const auto describe = module.Find<cse::abi::GetModuleAbi>("CseGetModuleAbi");
    const auto observed = cse::abi::Check(FixtureAbi, describe ? describe() : nullptr);
    using Exchange = const FixtureTable *(CSE_ABI_CALL *)();
    using Count = int (CSE_ABI_CALL *)();
    const FixtureTable *table = nullptr;
    if (observed == cse::abi::Compatibility::Compatible) {
        const auto exchange = module.Find<Exchange>("CseFixtureGetTable");
        if (!exchange) {
            return false;
        }
        table = exchange();
    }
    const auto count = module.Find<Count>("CseFixtureGetExchangeCount");
    if (observed != expected || !count || count() != (table ? 1 : 0)) {
        std::fprintf(stderr, "Module mismatch reached callback exchange or returned the wrong reason.\n");
        return false;
    }
    if (!table) {
        return true;
    }
    constexpr std::size_t bufferBytes = 128;
    std::unique_ptr<char, decltype(&std::free)> storage(static_cast<char *>(std::malloc(bufferBytes)), std::free);
    if (!storage) {
        return false;
    }
    const auto address = reinterpret_cast<std::uintptr_t>(storage.get());
    if constexpr (sizeof(void *) > sizeof(std::uint32_t)) {
        if (address <= (std::numeric_limits<std::uint32_t>::max)()) {
            std::fputs("Fixture allocation must be above 4 GiB.\n", stderr);
            return false;
        }
    }
    constexpr std::ptrdiff_t stringOffset = 37;
    constexpr char value[] = "native string identity";
    std::memcpy(storage.get() + stringOffset, value, sizeof(value));
    const char *text = table->stringAt(address, stringOffset);
    return table->command(address) == address && text == storage.get() + stringOffset
        && std::strcmp(text, value) == 0;
}

int main(int count, char **arguments) {
    using cse::abi::Compatibility;
    if (count == 3 && std::strcmp(arguments[1], "--reject-load") == 0) {
        Module foreign(arguments[2]);
        return foreign ? 3 : 0;
    }
    constexpr Compatibility expected[]{Compatibility::Compatible, Compatibility::RuntimeVersion,
        Compatibility::Architecture, Compatibility::PointerWidth, Compatibility::CompilerAbi,
        Compatibility::TableLayout, Compatibility::DescriptorSize, Compatibility::MissingDescriptor,
        Compatibility::DescriptorVersion};
    if (count != static_cast<int>(std::size(expected)) + 1
        || cse::abi::Check(FixtureAbi, nullptr) != Compatibility::MissingDescriptor) {
        return 1;
    }
    auto oversized = FixtureAbi;
    oversized.tableCount = cse::abi::MaxTables + 1;
    const auto duplicate = cse::abi::Describe({FixtureLayout, FixtureLayout});
    if (cse::abi::Check(FixtureAbi, &oversized) != Compatibility::TableCount
        || cse::abi::Check(duplicate, &duplicate) != Compatibility::InvalidDescriptor) {
        return 1;
    }
    for (std::size_t index = 0; index < std::size(expected); ++index) {
        if (!CheckModule(arguments[index + 1], expected[index])) {
            return 2;
        }
    }
    std::printf("Owned ABI loading, mismatch/missing-export rejections, native strings/handles passed (%zu bits).\n",
                sizeof(void *) * 8);
    return 0;
}
