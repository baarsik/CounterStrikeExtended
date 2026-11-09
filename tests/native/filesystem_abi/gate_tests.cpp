#include "contract_spike.h"
#include "public/native_filesystem_module.h"
#include <cstdio>

int main(int argc, char** argv) {
    if (argc != 10) {
        return 2;
    }
    // Keep independently instrumented fixture images mapped until all descriptors
    // are checked. Apple ASan retains image-global metadata across dlclose/reload;
    // repeated real unload behavior is qualified separately without that limitation.
    constexpr int ModuleCount = 9;
    void* modules[ModuleCount]{};
    for (int index = 1; index < argc; ++index) {
        void* module = cse_platform::LoadModule(argv[index]);
        if (!module) {
            return 3;
        }
        modules[index - 1] = module;
        const auto describe = reinterpret_cast<cse::abi::GetModuleAbi>(
            cse_platform::GetSymbol(module, "CseGetModuleAbi"));
        const auto factory = reinterpret_cast<CreateInterfaceFn>(cse_platform::GetSymbol(module, "CreateInterface"));
        const auto effects = reinterpret_cast<int (*)()>(cse_platform::GetSymbol(module, "CseFixtureFactoryEffects"));
        if (!factory || !effects || effects() != 0) {
            return 4;
        }
        const auto status = cse::abi::CheckRequired(filesystem_spike::Descriptor, describe ? describe() : nullptr);
        constexpr std::size_t TimestampBytes = 4;
        const bool providerCompatible =
            sizeof(decltype(static_cast<IFileSystem*>(nullptr)->GetFileTime(nullptr))) == TimestampBytes;
        const bool shouldAccept = index == 1 || index == 6 || (index == 8 && providerCompatible);
        if ((status == cse::abi::Compatibility::Compatible) != shouldAccept) {
            std::fprintf(stderr, "unexpected gate status module=%d status=%d\n", index, static_cast<int>(status));
            return 5;
        }
        if (status == cse::abi::Compatibility::Compatible) {
            int code = IFACE_FAILED;
            const auto checkedFactory = cse::filesystem_abi::CheckedFactory(module);
            if (!checkedFactory || !cse::filesystem_abi::CompatibleFactory(factory)
                || !checkedFactory("VFileSystem009", &code) || code != IFACE_OK) {
                return 6;
            }
        }
        if (!shouldAccept && (cse::filesystem_abi::CheckedFactory(module)
            || cse::filesystem_abi::CompatibleFactory(factory))) {
            return 9;
        }
        if (effects() != (shouldAccept ? 1 : 0)) {
            return 7;
        }
        // Generic third-party loading remains explicitly available, without owned-family enforcement.
        if (index == 9 && (!factory("third-party", nullptr) || effects() != 1)) {
            return 8;
        }
    }
    for (void* module : modules) {
        cse_platform::UnloadModule(module);
    }
    std::puts("filesystem_abi_gate_spike_passed=1");
}
