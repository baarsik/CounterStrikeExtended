#include <SDL3/SDL.h>
#include "../../../GoldSrc-SDK/public/native_dedicated_lifetime.h"
#include <dlfcn.h>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <unistd.h>
#include "../../../GoldSrc-SDK/public/native_dedicated_abi.h"
using Factory = CreateInterfaceFn;
using Dedicated = IDedicatedServerAPI;
class Exports : public IDedicatedExports {
public:
    void Sys_Printf(char* text) override { std::fputs(text, stdout); }
};
extern "C" __attribute__((visibility("default"))) const cse::abi::Descriptor* CseGetDedicatedAbiV1() {
    return &cse::dedicated_abi::Host;
}
static Exports exports;
static CseNativeDedicatedRetireFn retire;
extern "C" __attribute__((visibility("default"))) void *CreateInterface(const char *name, int *) {
    assert(retire(CSE_NATIVE_DEDICATED_LIFETIME_VERSION) == CSE_DEDICATED_RETIRE_SESSION_ACTIVE);
    return std::strcmp(name, "VENGINE_DEDICATEDEXPORTS_API_VERSION001") == 0 ? &exports : nullptr;
}
static void *MissingFactory(const char *, int *) {
    return nullptr;
}
int main(int argc, char **argv) {
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    std::setvbuf(stderr, nullptr, _IONBF, 0);
    assert(argc == 2 || argc == 6 || argc == 7);
    assert(SDL_WasInit(SDL_INIT_VIDEO) == 0);
    void *engine = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
    if (!engine) {
        std::fprintf(stderr, "%s\n", dlerror());
        return 1;
    }
    assert(SDL_WasInit(SDL_INIT_VIDEO) == 0);
    auto factory = reinterpret_cast<Factory>(dlsym(engine, "CreateInterface"));
    assert(factory);
    retire = reinterpret_cast<CseNativeDedicatedRetireFn>(dlsym(engine, "CseNativeDedicatedRetireV1"));
    assert(retire);
    assert(retire(0) == CSE_DEDICATED_RETIRE_UNSUPPORTED_VERSION);
    assert(retire(CSE_NATIVE_DEDICATED_LIFETIME_VERSION) == CSE_DEDICATED_RETIRE_SHUTDOWN_REQUIRED);
    assert(factory("VENGINE_LAUNCHER_API_VERSION002", nullptr));
    auto *api = static_cast<Dedicated *>(factory("VENGINE_HLDS_API_VERSION002", nullptr));
    assert(api);
    char basedir[] = ".";
    char failedCommand[] = "-dedicated -nobreakpad -game valve";
    assert(!api->Init(basedir, failedCommand, MissingFactory, MissingFactory));
    assert(!api->Init(basedir, failedCommand, CreateInterface, MissingFactory));
    api->Shutdown();
    api->Shutdown();
    assert(SDL_WasInit(SDL_INIT_VIDEO) == 0);
    if (argc >= 6) {
        void *filesystem = dlopen(argv[2], RTLD_NOW | RTLD_LOCAL);
        if (!filesystem) {
            std::fprintf(stderr, "%s\n", dlerror());
            return 1;
        }
        auto filesystemFactory = reinterpret_cast<Factory>(dlsym(filesystem, "CreateInterface"));
        assert(filesystemFactory);
        char command[512];
        std::snprintf(command, sizeof(command),
                      "-dedicated -nobreakpad -nomaster +ip 127.0.0.1 -port %s -game %s +map %s",
                      argc == 7 ? argv[6] : "27461", argv[4], argv[5]);
        std::puts("BEGIN dedicated Init");
        assert(api->Init(argv[3], command, CreateInterface, filesystemFactory));
        std::puts("END dedicated Init");
        assert(retire(CSE_NATIVE_DEDICATED_LIFETIME_VERSION) == CSE_DEDICATED_RETIRE_SESSION_ACTIVE);
        for (int frame = 0; frame < 500; ++frame) {
            assert(api->RunFrame());
            usleep(10000);
        }
        float fps = 0;
        int players = 0;
        int maxPlayers = 0;
        char map[256]{};
        api->UpdateStatus(&fps, &players, &maxPlayers, map);
        std::printf("STATUS fps=%f players=%d max=%d map=%s\n", fps, players, maxPlayers, map);
        assert(std::strcmp(map, argv[5]) == 0);
        char change[512];
        std::snprintf(change, sizeof(change), "changelevel %s\n", argv[5]);
        api->AddConsoleText(change);
        for (int frame = 0; frame < 300; ++frame) {
            assert(api->RunFrame());
            usleep(10000);
        }
        api->UpdateStatus(&fps, &players, &maxPlayers, map);
        std::printf("CHANGELEVEL map=%s\n", map);
        assert(std::strcmp(map, argv[5]) == 0);
        std::puts("BEGIN dedicated Shutdown");
        api->Shutdown();
        std::puts("END dedicated Shutdown");
        std::snprintf(command, sizeof(command),
                      "-dedicated -nobreakpad -nomaster +ip 127.0.0.1 -port %s -game %s +map %s",
                      argc == 7 ? argv[6] : "27461", argv[4], argv[5]);
        std::puts("BEGIN dedicated Init");
        assert(api->Init(argv[3], command, CreateInterface, filesystemFactory));
        std::puts("END dedicated Init");
        assert(retire(CSE_NATIVE_DEDICATED_LIFETIME_VERSION) == CSE_DEDICATED_RETIRE_SESSION_ACTIVE);
        for (int frame = 0; frame < 300; ++frame) {
            assert(api->RunFrame());
            usleep(10000);
        }
        api->UpdateStatus(&fps, &players, &maxPlayers, map);
        std::printf("REINIT map=%s\n", map);
        assert(std::strcmp(map, argv[5]) == 0);
        std::puts("BEGIN dedicated Shutdown");
        api->Shutdown();
        std::puts("END dedicated Shutdown");
        dlclose(filesystem);
    }
    assert(SDL_WasInit(SDL_INIT_VIDEO) == 0);
    assert(retire(0) == CSE_DEDICATED_RETIRE_UNSUPPORTED_VERSION);
    assert(retire(CSE_NATIVE_DEDICATED_LIFETIME_VERSION) == CSE_DEDICATED_RETIRE_READY);
    assert(retire(CSE_NATIVE_DEDICATED_LIFETIME_VERSION) == CSE_DEDICATED_RETIRE_ALREADY_RETIRED);
    assert(!api->Init(basedir, failedCommand, CreateInterface, MissingFactory));
    dlclose(engine);
    std::puts("PASS dedicated_load_without_video");
}
