#include "fixture.h"

static int exchanges = 0;

static std::uintptr_t CSE_ABI_CALL EchoCommand(std::uintptr_t handle) {
    return handle;
}

static const char *CSE_ABI_CALL StringAt(std::uintptr_t base, std::ptrdiff_t offset) {
    return reinterpret_cast<const char *>(base) + offset;
}

#if CSE_ABI_FIXTURE_FAULT != 7
static constexpr auto ModuleAbi() {
    auto descriptor = FixtureAbi;
#if CSE_ABI_FIXTURE_FAULT == 1
    ++descriptor.runtimeVersion;
#elif CSE_ABI_FIXTURE_FAULT == 2
    descriptor.architecture = 0;
#elif CSE_ABI_FIXTURE_FAULT == 3
    descriptor.pointerBits = 0;
#elif CSE_ABI_FIXTURE_FAULT == 4
    descriptor.compilerAbi = 0;
#elif CSE_ABI_FIXTURE_FAULT == 5
    descriptor.tables[0].signature ^= 1;
#elif CSE_ABI_FIXTURE_FAULT == 6
    --descriptor.descriptorBytes;
#elif CSE_ABI_FIXTURE_FAULT == 8
    --descriptor.descriptorVersion;
#endif
    return descriptor;
}
#endif

#if defined(_WIN32)
#define FIXTURE_EXPORT
#else
#define FIXTURE_EXPORT __attribute__((visibility("default")))
#endif

#if CSE_ABI_FIXTURE_FAULT != 7
extern "C" FIXTURE_EXPORT const cse::abi::Descriptor *CSE_ABI_CALL CseGetModuleAbi() {
    static constexpr auto descriptor = ModuleAbi();
    return &descriptor;
}
#endif

extern "C" FIXTURE_EXPORT const FixtureTable *CSE_ABI_CALL CseFixtureGetTable() {
    ++exchanges;
    static constexpr FixtureTable table{EchoCommand, StringAt};
    return &table;
}

extern "C" FIXTURE_EXPORT int CSE_ABI_CALL CseFixtureGetExchangeCount() {
    return exchanges;
}
