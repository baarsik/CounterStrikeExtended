#include "fake_sdl.h"
#include "ivoicerecord.h"

#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <thread>

#if defined(_WIN32)
#include <windows.h>
#include <crtdbg.h>
#else
#include <dlfcn.h>
#endif

namespace {

void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "capture module lifetime: %s\n", message);
        std::exit(1);
    }
}

using Factory = IVoiceRecord* (*)(int, int);

#if defined(_WIN32)
using Module = HMODULE;

Module Load(const char* path) {
    return LoadLibraryA(path);
}

Factory GetFactory(Module module) {
    return reinterpret_cast<Factory>(GetProcAddress(module, "FixtureCreateVoiceRecord"));
}

bool OwnsCallback(Module module, SDL_AudioStreamCallback callback, const char*) {
    HMODULE actual = nullptr;
    return GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        reinterpret_cast<const char*>(callback), &actual) != 0 && actual == module;
}

bool Unload(Module module) {
    return FreeLibrary(module) != 0;
}
#else
using Module = void*;

Module Load(const char* path) {
    return dlopen(path, RTLD_NOW | RTLD_LOCAL);
}

Factory GetFactory(Module module) {
    return reinterpret_cast<Factory>(dlsym(module, "FixtureCreateVoiceRecord"));
}

bool OwnsCallback(Module, SDL_AudioStreamCallback callback, const char* path) {
    Dl_info info = {};
    return dladdr(reinterpret_cast<const void*>(callback), &info) != 0
        && info.dli_fname != nullptr && std::strcmp(info.dli_fname, path) == 0;
}

bool Unload(Module module) {
    return dlclose(module) == 0;
}
#endif

}

int main(int argc, char** argv) {
    if (argc != 2) {
        return 2;
    }
#if defined(_WIN32)
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
    _set_error_mode(_OUT_TO_STDERR);
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
#if defined(_DEBUG)
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
#endif
#endif
    constexpr int sampleRate = 11025;
    constexpr unsigned int cyclesPerPolicy = 8;
    unsigned int cycles = 0;
    for (int policy : {0, 1}) {
        for (unsigned int index = 0; index < cyclesPerPolicy; ++index) {
            fake_sdl::Reset();
            Require(fake_sdl::ModulesAlive() == 0, "no fixture module survives the previous cycle");
            Module module = Load(argv[1]);
            Require(module != nullptr && fake_sdl::ModulesAlive() == 1, "load actual owned fixture module");
            Factory factory = GetFactory(module);
            Require(factory != nullptr, "resolve actual fixture factory");
            IVoiceRecord* record = factory(sampleRate, policy);
            Require(record != nullptr && record->RecordStart(), "real module recorder starts");
            SDL_AudioStream* stream = fake_sdl::LastStream();
            const auto callback = fake_sdl::CallbackFunction(stream);
            Require(callback != nullptr && OwnsCallback(module, callback, argv[1]),
                "the active callback code actually belongs to the module about to be unloaded");
            const short input[] = {-32768, -1, 0, 32767};
            fake_sdl::BlockNextRead();
            fake_sdl::PushFromWorker(stream, input, sizeof(input));
            Require(fake_sdl::WaitForBlockedRead(1000), "module callback is live inside a controlled SDK read");
            std::atomic<bool> released{false};
            IVoiceRecord* owned = record;
            record = nullptr;
            std::thread owner([owned, &released] {
                owned->Release();
                released = true;
            });
            Require(fake_sdl::WaitForPauseAttempt(1000) && !released && fake_sdl::CallbackActive(),
                "explicit release waits for module callback code before unload");
            fake_sdl::UnblockRead();
            owner.join();
            Require(released, "explicit module recorder release completes");
            const auto before = fake_sdl::Observe();
            Require(before.activeStreams == 0 && before.activeCallbacks == 0 && before.references == 0
                    && before.allocations == before.frees && before.callbackViolations == 0 && before.invalidCalls == 0,
                "module owns no callback, stream, queue or audio reference before unloading");
            factory = nullptr;
            Require(Unload(module) && fake_sdl::ModulesAlive() == 0, "actual module detach completed");
            Require(fake_sdl::Observe().callbacks == before.callbacks && !fake_sdl::CallbackActive(),
                "no callback survives the module's actual unload");
            ++cycles;
        }
    }
    std::printf("actual module unload passed: cycles%u, policies2, in-flight release, callbacks quiesced\n", cycles);
    return 0;
}
