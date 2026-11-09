#include "public/engine_launcher_api.h"
#include "public/filesystem.h"
#include "../public/native_filesystem_module.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dlfcn.h>
#include <unistd.h>

namespace {
void Event(const char* name, const char* value = "") {
    const auto* path = std::getenv("CSE_BOOTSTRAP_LOG");
    if (path) {
        auto* file = std::fopen(path, "ab");
        if (file) {
            std::fprintf(file, "%s\t%s\n", name, value);
            std::fclose(file);
        }
    }
}

__attribute__((constructor)) void Loaded() {
    Event("load");
}

__attribute__((destructor)) void Unloaded() {
    Event("unload");
}

class Engine final : public IEngineAPI {
public:
    int Run(void* instance, char* base, const char* command, char* restart,
        CreateInterfaceFn launcherFactory, CreateInterfaceFn fileSystemFactory) override {
        Event("run");
        Event("base", base);
        Event("command", command);
        auto* cwd = getcwd(nullptr, 0);
        Event("cwd", cwd ? cwd : "[failed]");
        std::free(cwd);
        Dl_info info{};
        dladdr(reinterpret_cast<void*>(&Loaded), &info);
        Event("engine", info.dli_fname ? info.dli_fname : "[failed]");
        Event("instance", instance == nullptr ? "null" : "unexpected");
        Event("launcher_factory", launcherFactory ? "present" : "missing");
        const bool compatible = cse::filesystem_abi::CompatibleFactory(fileSystemFactory);
        Event("checked_borrowed_filesystem", compatible ? "yes" : "no");
        if (!compatible) {
            return kEngineRunNone;
        }
        auto* filesystem = static_cast<IFileSystem*>(fileSystemFactory(kFileSystemVersion, nullptr));
        auto file = filesystem->Open("root-marker.txt", "rb", "ROOT");
        char bytes[32]{};
        const auto count = file ? filesystem->Read(bytes, static_cast<int>(sizeof(bytes) - 1), file) : 0;
        if (file) {
            filesystem->Close(file);
        }
        Event("root_file", count > 0 && std::strcmp(bytes, "owned-root-marker") == 0 ? "yes" : "no");
        const auto* plan = std::getenv("CSE_BOOTSTRAP_PLAN");
        const auto* counterPath = std::getenv("CSE_BOOTSTRAP_COUNTER");
        int countRun = 0;
        if (auto* counter = std::fopen(counterPath, "rb")) {
            std::fscanf(counter, "%d", &countRun);
            std::fclose(counter);
        }
        if (auto* counter = std::fopen(counterPath, "wb")) {
            std::fprintf(counter, "%d", countRun + 1);
            std::fclose(counter);
        }
        if (plan && std::strcmp(plan, "restart") == 0 && countRun == 0) {
            std::strcpy(restart, "-game valve");
            return kEngineRunRestart;
        }
        return kEngineRunNone;
    }
};
Engine engine;
}

extern "C" __attribute__((visibility("default"))) void* CreateInterface(const char* name, int*) {
    Event("engine_factory", name);
    return std::strcmp(name, kEngineLauncherVersion) == 0 ? &engine : nullptr;
}

extern "C" __attribute__((visibility("default"))) int FixtureEngineAlive() {
    return 1;
}
