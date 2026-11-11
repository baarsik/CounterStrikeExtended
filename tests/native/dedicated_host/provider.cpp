#include "public/native_dedicated_abi.h"
#include "public/native_dedicated_lifetime.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>

static void Event(const char* name) {
    if (const char* path = std::getenv("CSE_TEST_EVENTS")) {
        if (FILE* file = std::fopen(path, "a")) {
            std::fprintf(file, "%s\n", name);
            std::fclose(file);
        }
    }
}
class Engine final : public IDedicatedServerAPI {
public:
    bool Init(char*, char*, CreateInterfaceFn host, CreateInterfaceFn) override {
        Event("init");
        if (retired || !cse::dedicated_abi::HostFactoryCompatible(host)) {
            return false;
        }
        exports = static_cast<IDedicatedExports*>(host("VENGINE_DEDICATEDEXPORTS_API_VERSION001", nullptr));
        running = true;
        if (std::getenv("CSE_TEST_FAIL_INIT")) {
            return false;
        }
        return exports != nullptr;
    }
    int Shutdown() override {
        if (running) {
            Event("shutdown");
            exports->Sys_Printf((char*)"shutdown output\n");
            running = false;
        }
        complete = true;
        return 3;
    }
    bool RunFrame() override {
        Event("frame");
        return ++frames < 8;
    }
    void AddConsoleText(char* text) override { Event(text); }
    void UpdateStatus(float* fps, int* active, int* maximum, char* map) override {
        *fps = 100;
        *active = 0;
        *maximum = 8;
        std::strcpy(map, "fixture");
    }
    IDedicatedExports* exports = nullptr;
    bool running = false;
    bool complete = false;
    bool retired = false;
    int frames = 0;
};
class Admin final : public IGameServerData {
public:
    void WriteDataRequest(const void*, int) override { std::abort(); }
    int ReadDataResponse(void*, int) override { std::abort(); }
};
static Engine engine;
static Admin admin;
extern "C" __attribute__((visibility("default"))) void* CreateInterface(const char* name, int*) {
    Event("factory");
    if (std::strcmp(name, "VENGINE_HLDS_API_VERSION002") == 0) {
        return std::getenv("CSE_TEST_BAD_VIEW") ? static_cast<void*>(&admin) : static_cast<void*>(&engine);
    }
    if (std::strcmp(name, "GameServerData001") == 0) {
        return &admin;
    }
    return nullptr;
}
extern "C" __attribute__((visibility("default"))) const cse::abi::Descriptor* CseGetDedicatedAbiV1() {
    static auto descriptor = cse::dedicated_abi::Engine;
    if (std::getenv("CSE_TEST_BAD_ROLE")) {
        descriptor.tables[0].signature ^= 1;
    }
    return &descriptor;
}
extern "C" __attribute__((visibility("default"))) const cse::abi::Descriptor* CseGetGameServerDataAbiV1() {
    return &cse::dedicated_abi::Admin;
}
#ifndef CSE_TEST_MISSING_RETIRE
extern "C" __attribute__((visibility("default"))) uint32_t CseNativeDedicatedRetireV1(uint32_t version) {
    if (version != CSE_NATIVE_DEDICATED_LIFETIME_VERSION) { return CSE_DEDICATED_RETIRE_UNSUPPORTED_VERSION; }
    if (engine.running) { return CSE_DEDICATED_RETIRE_SESSION_ACTIVE; }
    if (!engine.complete) { return CSE_DEDICATED_RETIRE_SHUTDOWN_REQUIRED; }
    Event("retire");
    if (std::getenv("CSE_TEST_RETIRE_FAIL")) { return CSE_DEDICATED_RETIRE_TRANSPORT_FAILURE; }
    engine.retired = true;
    return CSE_DEDICATED_RETIRE_READY;
}
#endif
__attribute__((destructor)) static void Unload() { Event("unload"); }
