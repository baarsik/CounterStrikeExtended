#include "VGui1HostImageOwner.h"
#include "native_module.h"
#include <dlfcn.h>
#include <cstdint>

#define FIXTURE_EXPORT extern "C" __attribute__((visibility("default")))

namespace {
void* retained = nullptr;
}

FIXTURE_EXPORT void* CreateInterface(const char*, int*) {
    return nullptr;
}

FIXTURE_EXPORT int RootHold(const char* path) {
    if (retained) {
        return 0;
    }
    retained = cse_platform::LoadModule(path, cse_platform::ModuleFallback::None);
    return retained != nullptr;
}

FIXTURE_EXPORT void* RootRetained() {
    return retained;
}

FIXTURE_EXPORT int RootTransferHeld(const cse_vgui1_host::HostReleasesV2* host) {
    if (!retained || !host || host->structBytes != sizeof(*host)
        || host->version != cse_vgui1_host::HostReleasesVersion || !host->acceptRoot) {
        return 0;
    }
    if (!host->acceptRoot(host->context, retained)) {
        return 0;
    }
    retained = nullptr;
    return 1;
}

FIXTURE_EXPORT int RootBurst(const cse_vgui1_host::HostReleasesV2* host, const char* path, int count) {
    if (!host || host->structBytes != sizeof(*host) || host->version != cse_vgui1_host::HostReleasesVersion
        || !host->acceptRoot || count <= 0) {
        return -1;
    }
    for (int index = 0; index < count; ++index) {
        void* reference = cse_platform::LoadModule(path, cse_platform::ModuleFallback::None);
        if (!reference) {
            return -2;
        }
        if (!host->acceptRoot(host->context, reference)) {
            return dlclose(reference) == 0 ? -3 : -4;
        }
    }
    return count;
}

FIXTURE_EXPORT int RootProbe(int (*probe)(void*), void* context) {
    return probe(context);
}

FIXTURE_EXPORT int RootPing() {
    constexpr int LiveRootValue = 73;
    return LiveRootValue;
}
