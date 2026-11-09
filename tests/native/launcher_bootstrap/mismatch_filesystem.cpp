#include "public/filesystem.h"
#include "../public/cse_filesystem_abi.h"
#include <cstdio>
#include <cstdlib>

extern "C" __attribute__((visibility("default"))) const cse::abi::Descriptor* CseGetModuleAbi() {
    static cse::abi::Descriptor descriptor = cse::filesystem_abi::Descriptor;
#if !defined(CSE_BOOTSTRAP_NULL_FACTORY)
    descriptor.runtimeVersion = 0;
#endif
    return &descriptor;
}

extern "C" __attribute__((visibility("default"))) void* CreateInterface(const char*, int*) {
    if (const auto* path = std::getenv("CSE_BOOTSTRAP_LOG")) {
        if (auto* file = std::fopen(path, "ab")) {
#if defined(CSE_BOOTSTRAP_NULL_FACTORY)
            std::fputs("null_filesystem_factory\n", file);
#else
            std::fputs("forbidden_filesystem_factory\n", file);
#endif
            std::fclose(file);
        }
    }
    return nullptr;
}
