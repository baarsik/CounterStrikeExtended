#include "contract_spike.h"

#if defined(_WIN32)
#define FIXTURE_EXPORT __declspec(dllexport)
#define FACTORY_EXPORT
#else
#define FIXTURE_EXPORT __attribute__((visibility("default")))
#define FACTORY_EXPORT FIXTURE_EXPORT
#endif

namespace {
int effects = 0;
int factoryObject = 0;
}

#ifndef CSE_FS_MISSING
extern "C" FIXTURE_EXPORT const cse::abi::Descriptor* CSE_ABI_CALL CseGetModuleAbi() {
    static const cse::abi::Descriptor descriptor = []() {
        auto value = filesystem_spike::Descriptor;
#if CSE_FS_FAULT == 1
        value.tables[0].identifier ^= 1;
#elif CSE_FS_FAULT == 2
        value.tables[1].signature ^= 1;
#elif CSE_FS_FAULT == 3
        value.tables[2].signature ^= 1;
#elif CSE_FS_FAULT == 4
        --value.runtimeVersion;
#elif CSE_FS_FAULT == 5
        value.tables[value.tableCount++] = {0x554e524c, 0, 42}; // unrelated compatible family
#endif
        return value;
    }();
    return &descriptor;
}
#endif

extern "C" FACTORY_EXPORT void* CreateInterface(const char*, int* returnCode) {
    ++effects;
    if (returnCode) {
        *returnCode = IFACE_OK;
    }
    return &factoryObject;
}
extern "C" FIXTURE_EXPORT int CseFixtureFactoryEffects() { return effects; }
