#include "public/native_module.h"
#include <cstdio>

extern "C" void MountForProbe(void*, const char*);
extern "C" void UnmountForProbe(void*);
extern "C" long long ConsumerTime(void*);
extern "C" long long ProviderTime(void*);
extern "C" long long ConsumerModificationTime(void*);
extern "C" long long ProviderModificationTime(void*);

int main(int argc, char** argv) {
    if (argc != 3) {
        return 2;
    }
    void* module = cse_platform::LoadModule(argv[1]);
    if (!module) {
        return 3;
    }
    using Factory = void* (*)(const char*, int*);
    auto factory = reinterpret_cast<Factory>(cse_platform::GetSymbol(module, "CreateInterface"));
    if (!factory) {
        return 4;
    }
    void* object = factory("VFileSystem009", nullptr);
    if (!object) {
        return 5;
    }
    MountForProbe(object, argv[2]);
    const long long offered = ProviderTime(object);
    const long long consumed = ConsumerTime(object);
    const long long offeredModification = ProviderModificationTime(object);
    const long long consumedModification = ConsumerModificationTime(object);
    std::printf("actual_provider_time=%lld actual_consumer_time=%lld provider_modification=%lld consumer_modification=%lld\n",
        offered, consumed, offeredModification, consumedModification);
    UnmountForProbe(object);
    cse_platform::UnloadModule(module);
    constexpr long long ControlledTimestamp = -1;
    return offered == consumed && offeredModification == ControlledTimestamp &&
        consumedModification == ControlledTimestamp ? 0 : 1;
}
