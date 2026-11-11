// Compile the actual launcher/session, replacing only its engine Run call:
// real cohort, filesystem, engine/root and provider load
// and close execute, but game/Steam session initialization must remain controller-owned.
#undef main
#undef _NSGetExecutablePath
#undef realpath
#undef chdir
#include "public/engine_launcher_api.h"
#include "public/filesystem.h"
#include "engine/registry.h"
#include "engine/utlvector.h"
#include <cassert>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dlfcn.h>
#include <mach-o/dyld.h>
#include <string>
#include <unistd.h>

int CseActualLauncherMain(int, const char**, const char**);

namespace {
const char* actualEnginePath = nullptr;
bool reachedRun = false;

// The exact engine registry owner, also used by the existing menu fixture.
namespace engine_registry {
#define Q_strncpy std::strncpy
#include "menu_registry.inc"
#undef Q_strncpy
}

bool PathError(const char* name) {
    const char* value = std::getenv("CSE_BOOTSTRAP_PATH_ERROR");
    return value && std::strcmp(value, name) == 0;
}

class StopBeforeGame final : public IEngineAPI {
public:
    int Run(void*, char* base, const char* command, char*, CreateInterfaceFn, CreateInterfaceFn factory) override {
        reachedRun = true;
        char* cwd = getcwd(nullptr, 0);
        assert(cwd);
        std::printf("cwd=%s\nbase=%s\nengine=%s\ncommand=%s\n", cwd, base, actualEnginePath, command);
        std::free(cwd);
        auto* filesystem = static_cast<IFileSystem*>(factory(kFileSystemVersion, nullptr));
        assert(filesystem);
        auto file = filesystem->Open("root-marker.txt", "rb", "ROOT");
        char bytes[sizeof("owned-root-marker")]{};
        assert(file);
        filesystem->Read(bytes, sizeof(bytes) - 1, file);
        filesystem->Close(file);
        assert(std::strcmp(bytes, "owned-root-marker") == 0);
        std::puts("actual_filesystem_root=read");

        const std::string providerName = std::string(base) + "/libsteam_api.dylib";
        void* provider = dlopen(providerName.c_str(), RTLD_NOW | RTLD_NOLOAD | RTLD_LOCAL);
        assert(provider);
        Dl_info info{};
        assert(dladdr(dlsym(provider, "SteamAPI_Init"), &info));
        char* providerPath = realpath(info.dli_fname, nullptr);
        assert(providerPath);
        std::printf("provider=%s\n", providerPath);
        std::free(providerPath);
        dlclose(provider);

        engine_registry::registry->Init();
        std::printf("windowed=%d\nwidth=%d\nheight=%d\n",
            engine_registry::registry->ReadInt("ScreenWindowed", -1),
            engine_registry::registry->ReadInt("DisplayWidth", -1),
            engine_registry::registry->ReadInt("DisplayHeight", -1));
        engine_registry::registry->Shutdown();
        return kEngineRunNone;
    }
} stopBeforeGame;

}

int FixtureStopBeforeGame(IEngineAPI* actual, void* instance, char* base, const char* command, char* restart,
    CreateInterfaceFn launcher, CreateInterfaceFn filesystem) {
    Dl_info info{};
    assert(actual && dladdr(actual, &info));
    actualEnginePath = info.dli_fname;
    return stopBeforeGame.Run(instance, base, command, restart, launcher, filesystem);
}

extern "C" int CseFixtureExecutablePath(char* path, std::uint32_t* capacity) {
    if (PathError("executable-too-long")) {
        *capacity += 1;
        return -1;
    }
    return _NSGetExecutablePath(path, capacity);
}

// The Darwin SDK redirects realpath to its extended ABI symbol.
extern "C" char* CseFixtureRealpath(const char*, char*) __asm("_CseFixtureRealpath$DARWIN_EXTSN");
extern "C" char* CseFixtureRealpath(const char* path, char* output) {
    if (PathError("unresolvable-executable")) {
        errno = ENOENT;
        return nullptr;
    }
    return realpath(path, output);
}

extern "C" int CseFixtureChdir(const char* path) {
    if (PathError("missing-directory") || PathError("denied-directory")) {
        errno = PathError("missing-directory") ? ENOENT : EACCES;
        return -1;
    }
    return chdir(path);
}

int main(int argc, const char** argv) {
    const int result = CseActualLauncherMain(argc, argv, nullptr);
    std::printf("launcher_result=%d\nreached_run=%d\n", result, reachedRun);
    return result;
}
