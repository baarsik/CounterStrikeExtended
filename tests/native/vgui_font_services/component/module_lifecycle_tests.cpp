#include "VGui1FontServices.h"
#include "fake_provider.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dlfcn.h>
#include <string>

namespace {
using Result = CseVgui1FontResultV1;
void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "FAILED module lifecycle: %s\n", message);
        std::abort();
    }
}

template<class Function>
Function Symbol(void* library, const char* name) {
    void* symbol = dlsym(library, name);
    Require(symbol != nullptr, name);
    return reinterpret_cast<Function>(symbol);
}

void* Load(const char* path) {
    void* library = dlopen(path, RTLD_NOW | RTLD_LOCAL);
    if (!library) {
        std::fprintf(stderr, "%s\n", dlerror());
    }
    Require(library != nullptr, "actual owned dylib load");
    return library;
}

std::string Read(const char* path) {
    FILE* input = std::fopen(path, "rb");
    Require(input != nullptr, "teardown receipt exists");
    std::string value;
    char buffer[1024];
    std::size_t count = 0;
    while ((count = std::fread(buffer, 1, sizeof(buffer), input)) != 0) {
        value.append(buffer, count);
    }
    Require(std::fclose(input) == 0, "teardown receipt close");
    return value;
}
}

int main(int argc, char** argv) {
    Require(argc == 7, "module, two clients, model, receipt and mode arguments");
    Require(setenv("CSE_VGUI1_FONT_TEARDOWN_RECEIPT", argv[5], 1) == 0, "owned teardown receipt environment");
    const bool processExit = std::strcmp(argv[6], "exit") == 0;
    const bool repeat = std::strcmp(argv[6], "repeat") == 0;
    constexpr int kRepeatedLoads = 8;
    const int cycles = repeat ? kRepeatedLoads : 1;
    std::remove(argv[5]);
    for (int cycle = 0; cycle < cycles; ++cycle) {
        void* module = Load(argv[1]);
        void* first = Load(argv[2]);
        void* second = Load(argv[3]);
        const auto bind = Symbol<decltype(&CseVgui1FontsBindFilesV1)>(module, "CseVgui1FontsBindFilesV1");
        const auto preflight = Symbol<decltype(&CseVgui1FontsPreflightV1)>(module, "CseVgui1FontsPreflightV1");
        const auto release = Symbol<decltype(&CseVgui1FontsReleaseFilesV1)>(module, "CseVgui1FontsReleaseFilesV1");
        const auto stats = Symbol<decltype(&FakeFontSnapshotC)>(module, "FakeFontSnapshotC");
        const auto failRelease = Symbol<decltype(&FakeFontFailReleaseC)>(module, "FakeFontFailReleaseC");
        const auto firstTall = Symbol<int (*)()>(first, "CseFixtureFontTall");
        const auto secondTall = Symbol<int (*)()>(second, "CseFixtureFontTall");
        const auto firstPlatform = Symbol<const void* (*)()>(first, "CseFixtureFontPlatform");
        const auto secondPlatform = Symbol<const void* (*)()>(second, "CseFixtureFontPlatform");
        const auto reset = Symbol<void (*)()>(first, "CseFixtureFontReset");
        const auto firstPrepare = Symbol<Result (*)()>(first, "CseFixtureFontPrepare");
        const auto secondPrepare = Symbol<Result (*)()>(second, "CseFixtureFontPrepare");
        Require(preflight(nullptr, 0) == Result::PendingResources && stats().contextsCreated == 0,
            "actual consumer initializers park requests before binding");
        const void* identity = firstPlatform();
        reset();
        const char* files[]{argv[4]};
        Require(bind(files, 1, nullptr, 0) == Result::FilesBound && preflight(nullptr, 0) == Result::Ready,
            "global preflight skips the retained consumer requests");
        Require(stats().realizations == 0 && firstPrepare() == Result::Ready && secondPrepare() == Result::Ready
            && stats().realizations == 1 && firstPlatform() == identity,
            "two real consumers prepare one retained initial request without identity changes");
        reset();
        Require(release(nullptr, 0) == Result::FilesReleased && bind(files, 1, nullptr, 0) == Result::FilesBound
            && preflight(nullptr, 0) == Result::Ready && stats().realizations == 1,
            "release/rebind leaves formerly Ready consumer requests outside global preflight");
        Require(firstPrepare() == Result::Ready && secondPrepare() == Result::Ready
            && stats().realizations == 2 && firstPlatform() == identity,
            "scoped preparation realizes retained consumers before unload or exit");
        Require(firstPlatform() == secondPlatform() && firstTall() == 5 && secondTall() == 5,
            "two separately loaded clients share module-owned platform identity");
        if (processExit) {
            // Keep real loader references. Client static destructors must run
            // while the service is usable, before its owned context is freed.
            std::puts("PASS prepared process-exit dependency ownership");
            return 0;
        }
        Require(dlclose(first) == 0 && secondTall() == 5 && stats().liveFaces == 1,
            "unloading one borrower does not invalidate the peer");
        Require(dlclose(second) == 0, "second borrower unload");
        const auto before = stats();
        failRelease(1);
        Require(release(nullptr, 0) == Result::ReleaseFailed && stats().liveFaces == 0
            && stats().contextsDestroyed == before.contextsDestroyed
            && bind(files, 1, nullptr, 0) == Result::ReleaseFailed,
            "root-equivalent owner keeps module loaded and refuses rebind after failed release");
        Require(release(nullptr, 0) == Result::FilesReleased
            && stats().contextsDestroyed == before.contextsDestroyed + 1,
            "explicit bounded retry releases retained owner/context before unload");
        Require(dlclose(module) == 0, "service unload only after successful release");
        const std::string receipt = Read(argv[5]);
        const std::size_t destroyed = receipt.rfind("context-destroyed faces=0");
        Require(destroyed != std::string::npos && receipt.rfind("live-metric=5") < destroyed,
            "consumer metric/destruction precedes context teardown");
    }
    std::puts("PASS actual independent client/service unload and release retry");
}
