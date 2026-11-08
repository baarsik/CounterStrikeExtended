#include "interface.h"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#if !defined(_WIN32)
#include <dlfcn.h>
#endif

#if CSE_TEST_BORROWED_NAME_API
extern void* GetModuleHandle(const char* name);
#endif

namespace {
void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "Module loading fixture failed: %s\n", message);
        std::exit(1);
    }
}

std::string ReadTrace(const std::string& path) {
    std::ifstream stream(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
}
} // namespace

int main(int argc, char** argv) {
    Require(argc == 3, "leaf module and trace paths required");
    const std::string modulePath = argv[1];
    const std::string tracePath = argv[2];
    std::filesystem::remove(tracePath);
#if defined(_WIN32)
    Require(_putenv_s("CSE_MODULE_FIXTURE_TRACE", tracePath.c_str()) == 0, "set owned trace environment");
#else
    Require(setenv("CSE_MODULE_FIXTURE_TRACE", tracePath.c_str(), 1) == 0, "set owned trace environment");
#endif
    const CreateInterfaceFn unloadedFactory = CSE_TEST_MISSING_FACTORY_MAIN ? Sys_GetFactoryThis() : nullptr;
    Require(Sys_GetFactory(modulePath.c_str()) == unloadedFactory, "preserve unloaded-module factory lookup policy");
#if CSE_TEST_BORROWED_NAME_API
    Require(GetModuleHandle(modulePath.c_str()) == nullptr, "unloaded lookup must not create a dangling handle");
#endif
    Require(ReadTrace(tracePath).empty(), "unloaded lookup must not run module constructors");
    Require(Sys_GetFactory("cse-fixture-missing-module") == unloadedFactory, "preserve missing-module factory policy");
    Require(Sys_GetFactory(static_cast<CSysModule*>(nullptr)) == nullptr, "null owned module has no factory");
#if CSE_TEST_NULL_FACTORY_MAIN
    Require(reinterpret_cast<CreateInterfaceFn>(Sys_GetProcAddress(nullptr, "CreateInterface"))
        == Sys_GetFactoryThis(), "legacy raw null-handle symbol lookup uses the main executable");
    Require(Sys_GetFactory(static_cast<const char*>(nullptr)) == Sys_GetFactoryThis(), "main-executable null lookup");
#else
    Require(Sys_GetProcAddress(nullptr, "CreateInterface") == nullptr, "null handle does not resolve a global symbol");
    Require(Sys_GetFactory(static_cast<const char*>(nullptr)) == nullptr, "rejected null-name lookup");
#endif

    constexpr int LifecycleCycles = 16;
    for (int cycle = 0; cycle < LifecycleCycles; ++cycle) {
        std::filesystem::remove(tracePath);
        CSysModule* first = Sys_LoadModule(modulePath.c_str());
        Require(first != nullptr && ReadTrace(tracePath) == "C", "load module exactly once");
        CSysModule* second = Sys_LoadModule(modulePath.c_str());
        Require(second == first && ReadTrace(tracePath) == "C", "duplicate open retains the same module");
#if CSE_TEST_BORROWED_NAME_API
        Require(GetModuleHandle(modulePath.c_str()) == first, "borrow loaded handle without changing ownership");
#endif
        CreateInterfaceFn factory = Sys_GetFactory(first);
        Require(factory && Sys_GetFactory(modulePath.c_str()) == factory, "native factory identity");
        int status = -1;
        void* value = factory("FixtureLeaf001", &status);
        Require(value && status == 0, "call actual module factory");
        constexpr std::uint64_t LeafIdentity = UINT64_C(0x4353454c45414631);
#if UINTPTR_MAX > UINT32_MAX
        Require(reinterpret_cast<std::uintptr_t>(value) > UINT32_MAX, "native factory returns a high data address");
#endif
        Require(*static_cast<const std::uint64_t*>(value) == LeafIdentity, "native factory data value");
        const auto* exportedData = static_cast<const std::uint64_t*>(Sys_GetProcAddress(first, "CseLeafData"));
        Require(exportedData && *exportedData == LeafIdentity, "native data symbol");
        constexpr std::uintptr_t DataOrdinal = 395;
        const auto* ordinalName = reinterpret_cast<const char*>(DataOrdinal);
#if defined(_WIN32)
        Require(Sys_GetProcAddress(first, ordinalName) == exportedData, "Windows data ordinal preserved");
#else
        Require(Sys_GetProcAddress(first, ordinalName) == nullptr, "Windows ordinal rejected without dereferencing on POSIX");
#endif
        using Ping = int (*)(int);
        const auto ping = reinterpret_cast<Ping>(Sys_GetProcAddress(first, "CseLeafPing"));
        Require(ping && ping(5) == 12, "call a typed native symbol");
        Require(Sys_GetProcAddress(first, "CseNeverExported") == nullptr, "missing symbol");
        Sys_UnloadModule(first);
        Require(ReadTrace(tracePath) == "C" && ping(6) == 13, "borrowed lookup does not consume the retained owner");
        Sys_UnloadModule(second);
        Require(ReadTrace(tracePath) == "CD", "last owner unloads exactly once");
        Require(Sys_GetFactory(modulePath.c_str()) == unloadedFactory, "no borrowed factory leak after final unload");
#if CSE_TEST_BORROWED_NAME_API
        Require(GetModuleHandle(modulePath.c_str()) == nullptr, "no borrowed reference leak after final unload");
#endif
        Require(ReadTrace(tracePath) == "CD", "post-unload lookup has no lifecycle side effects");
    }
    std::filesystem::path bare = modulePath;
    bare.replace_extension();
    std::filesystem::remove(tracePath);
    CSysModule* module = Sys_LoadModule(bare.string().c_str());
    Require(module != nullptr, "native suffix fallback for an absolute basename");
    Sys_UnloadModule(module);
    Require(ReadTrace(tracePath) == "CD", "suffix fallback lifecycle");
    const std::string relative = std::filesystem::path(modulePath).filename().string();
    module = Sys_LoadModule(relative.c_str());
    Require(module != nullptr, "relative path load");
    Sys_UnloadModule(module);
#if !defined(_WIN32)
    std::filesystem::path legacy = modulePath;
    legacy.replace_extension(".dll");
    module = Sys_LoadModule(legacy.string().c_str());
    Require(module && Sys_GetFactory(legacy.string().c_str()) == Sys_GetFactory(module),
        "Windows module spelling maps to the native extension");
    Sys_UnloadModule(module);
#endif
#if !defined(_WIN32)
    std::filesystem::remove(tracePath);
    void* local = dlopen(modulePath.c_str(), RTLD_NOW | RTLD_LOCAL);
    Require(local && dlsym(RTLD_DEFAULT, "CseLeafPing") == nullptr, "local module starts hidden from global lookup");
    Require(Sys_GetFactory(modulePath.c_str()) != nullptr, "query a locally loaded module");
    Require(dlsym(RTLD_DEFAULT, "CseLeafPing") == nullptr, "borrowed lookup must not promote module visibility");
    Require(dlclose(local) == 0 && ReadTrace(tracePath) == "CD", "local owner lifecycle remains balanced");
    const auto directory = std::filesystem::current_path();
    const auto missingDirectory = directory / ("owned-missing-cwd-" + std::filesystem::path(tracePath).stem().string());
    Require(std::filesystem::create_directory(missingDirectory), "create owned missing-directory fixture");
    std::filesystem::current_path(missingDirectory);
    Require(std::filesystem::remove(missingDirectory), "remove only the owned empty working directory");
    Require(Sys_LoadModule(relative.c_str()) == nullptr, "unavailable working directory fails without an invalid path");
    std::filesystem::current_path(directory);
#endif
    Sys_UnloadModule(nullptr);
    std::puts("Production engine loader: borrowed handles, two owners, factories, high addresses and suffix lifecycle");
    return 0;
}
