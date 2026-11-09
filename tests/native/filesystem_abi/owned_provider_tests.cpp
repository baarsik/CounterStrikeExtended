#include "contract_spike.h"
#include "public/native_filesystem_module.h"

#include <cstdio>

int main(int argc, char** argv) {
    if (argc != 2) {
        return 2;
    }
    void* module = cse_platform::LoadModule(argv[1]);
    if (!module) {
        return 3;
    }
    cse::abi::Compatibility reason{};
    const auto factory = cse::filesystem_abi::CheckedFactory(module, &reason);
    if (!factory) {
        std::fprintf(stderr, "actual owned provider rejected before factory call: reason=%d\n",
            static_cast<int>(reason));
        cse_platform::UnloadModule(module);
        return 4;
    }
    if (!cse::filesystem_abi::CompatibleFactory(factory)) {
        return 5;
    }
    int code = IFACE_FAILED;
    const auto filesystem = static_cast<IFileSystem*>(factory(FILESYSTEM_INTERFACE_VERSION, &code));
    if (!filesystem || code != IFACE_OK) {
        return 6;
    }
    // The provider retains this singleton; consumers never delete its C++ object.
    cse_platform::UnloadModule(module);
    std::puts("filesystem_actual_owned_provider_contract=passed");
}
