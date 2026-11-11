#include "public/native_system_module_abi.h"
#if defined(_WIN32)
#define FIXTURE_EXPORT __declspec(dllexport)
#else
#define FIXTURE_EXPORT __attribute__((visibility("default")))
#endif
static int mode;
static int creates;
static ISystemModule* CSE_ABI_CALL Create(const char*) {
    ++creates;
    return nullptr;
}
static void CSE_ABI_CALL Release(ISystemModule*) {}
extern "C" FIXTURE_EXPORT void SetMode(int value) {
    mode = value;
    creates = 0;
}
extern "C" FIXTURE_EXPORT int CreateCalls() { return creates; }
extern "C" FIXTURE_EXPORT void* CreateInterface(const char* name, int*) {
    static cse::modules::Factory table;
    table = {cse::modules::Contract, sizeof(table), cse::modules::ContractRevision, Create, Release};
    switch (mode) {
        case 1: return nullptr;
        case 2: table.bytes = 0; break;
        case 3: ++table.revision; break;
        case 4: ++table.descriptor.architecture; break;
        case 5: table.descriptor.pointerBits = 32; break;
        case 6: ++table.descriptor.compilerAbi; break;
        case 7: ++table.descriptor.tables[0].signature; break;
        case 8: table.Create = nullptr; break;
        case 9: table.Release = nullptr; break;
        case 10: table.descriptor.tableCount = 0; break;
    }
    if (cse::modules::Is(name, cse::modules::FactoryName)) { return &table; }
    if (cse::modules::Is(name, cse::modules::ConsumerName)) { return &table.descriptor; }
    return nullptr;
}
