#include "FileSystem.h"
#include "native_standalone_abi.h"
namespace {
int calls = 0;
}
extern "C" __attribute__((visibility("default"))) const cse::abi::Descriptor* CseGetStandaloneVguiAbiV1() {
    static auto descriptor = cse::standalone_abi::Provider;
#if defined(CSE_TEST_WRONG_CONTRACT)
    descriptor.tables[1].signature = 0;
#else
    descriptor.tables[0].signature = 0;
#endif
    return &descriptor;
}
extern "C" __attribute__((visibility("default"))) void* CreateInterface(const char*, int*) {
    ++calls;
    return nullptr;
}
extern "C" __attribute__((visibility("default"))) int FactoryCalls() {
    return calls;
}
