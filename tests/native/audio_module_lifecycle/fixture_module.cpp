// Actual owned worker, audio shutdown/DMA close/Run bodies; controlled game/controller dependencies.
#include "../../../GoldSrc-SDK/engine/mpg123_music.h"
#include "../../../GoldSrc-SDK/engine/sdl3_audio_backend.h"
#include "../../../GoldSrc-SDK/engine/thread.h"
#include "worker_fault_injection.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#if defined(_WIN32)
#include <eh.h>
#define FIXTURE_EXPORT __declspec(dllexport)
#else
#define FIXTURE_EXPORT __attribute__((visibility("default")))
#endif

cse::Mpg123MusicPlayer musicPlayer;
cse::SdlAudioBackend audioBackend;
namespace {
int controllerShutdowns = 0;
int captureCleanupCalls = 0;
int mixerCleanupCalls = 0;
std::atomic<bool> commandEntered{false};
std::atomic<bool> commandCompleted{false};
std::atomic<bool> lateCommandAccepted{false};
std::atomic<int> deliveredCommands{0};
const char* requestedMode = nullptr;
const char* requestedFailure = nullptr;
const char* requestedTrack = nullptr;
}

// Capture close and callback quiescence have separate component fixtures.
void Voice_ReleaseCaptureForModuleUnload() {
    ++captureCleanupCalls;
}

void Voice_ReleaseMixerForModuleUnload() {
    ++mixerCleanupCalls;
}

CCDAudio::CCDAudio() {}
CCDAudio::~CCDAudio() {}
int CCDAudio::Init() {
    musicPlayer.Init();
    if (std::strcmp(requestedFailure, "controller-init-cpp") == 0) {
        throw std::runtime_error("controlled controller initialization failure");
    }
#if defined(_WIN32)
    if (std::strcmp(requestedFailure, "controller-init-seh") == 0) {
        RaiseException(EXCEPTION_ACCESS_VIOLATION, 0, 0, nullptr);
    }
#endif
    return 0; // The actual controller's successful Init also returns zero.
}
void CCDAudio::Shutdown() {
    if (commandEntered && !commandCompleted) {
        std::abort();
    }
    ++controllerShutdowns;
#if defined(_WIN32)
    if (std::strcmp(requestedFailure, "controller-close-seh") == 0) {
        RaiseException(EXCEPTION_ACCESS_VIOLATION, 0, 0, nullptr);
    }
#endif
    musicPlayer.Shutdown();
}
void CCDAudio::Play(int, bool) {}
void CCDAudio::PlayFile(const char*, bool) {}
void CCDAudio::Pause() {}
void CCDAudio::Resume() {}
void CCDAudio::Frame() {}
void CCDAudio::MP3_Resume_Audio() {}
void CCDAudio::MP3_Suspend_Audio() {}
void CCDAudio::_CDUpdate(int, int) {}
void CCDAudio::_Stop(int, int) {
    musicPlayer.Stop();
}
void CCDAudio::_PlayFile(const char* path, int) {
    ++deliveredCommands;
    commandEntered = true;
    constexpr Uint32 InFlightCommandMilliseconds = 150;
    SDL_Delay(InFlightCommandMilliseconds);
    if (!musicPlayer.Open(path, true) || !musicPlayer.Start()) {
        std::terminate();
    }
    lateCommandAccepted = thread->AddThreadItem(&CCDAudio::_Stop, 0, 0);
    commandCompleted = true;
}
static CCDAudio controller;
ICDAudio* cdaudio = &controller;
CCDAudio* GetInteralCDAudio() {
    return &controller;
}

using CreateInterfaceFn = void* (*)(const char*, int*);
static int build_number() {
    return 0;
}
static char* va(char*, ...) {
    return nullptr;
}
static void SteamAPI_UseBreakpadCrashHandler(char*, const char*, const char*, int, void*, void*) {}
#if defined(_WIN32)
static void ExceptionHandler(unsigned int, EXCEPTION_POINTERS*) {}
#else
static void ExceptionHandler(unsigned int, void*) {}
static void _set_se_translator(void (*)(unsigned int, void*)) {}
#endif
class CEngineAPI {
public:
    int Run(void*, char*, char*, char*, CreateInterfaceFn, CreateInterfaceFn);
};

static WorkerFault RequestedFault() {
    if (std::strcmp(requestedFailure, "mutex-fail") == 0) {
        return WorkerFault::Mutex;
    }
    if (std::strcmp(requestedFailure, "event-fail") == 0) {
        return WorkerFault::Event;
    }
    if (std::strcmp(requestedFailure, "thread-fail") == 0) {
        return WorkerFault::Thread;
    }
    if (std::strcmp(requestedFailure, "attribute-fail") == 0) {
        return WorkerFault::Attribute;
    }
    if (std::strcmp(requestedFailure, "attribute-type-fail") == 0) {
        return WorkerFault::AttributeType;
    }
    if (std::strcmp(requestedFailure, "condition-fail") == 0) {
        return WorkerFault::Condition;
    }
    return WorkerFault::None;
}

static int RunListenServer(void*, char*, char*, char*, CreateInterfaceFn, CreateInterfaceFn) {
    const bool partial = std::strcmp(requestedFailure, "partial") == 0;
    const WorkerFault fault = RequestedFault();
    SetWorkerFault(fault);
    if (partial) {
        if (!musicPlayer.Init()) {
            return -1;
        }
    } else if (!thread->Init()) {
        return fault == WorkerFault::None ? -2 : 0;
    }
    const bool useEffects = std::strcmp(requestedMode, "music") != 0;
    const bool useMusic = std::strcmp(requestedMode, "effects") != 0;
    const bool inFlight = std::strcmp(requestedFailure, "inflight") == 0;
    const bool primed = std::strcmp(requestedFailure, "primed") == 0;
    constexpr int EffectsFrequency = 22050;
    constexpr int EffectsChannels = 2;
    if (!partial && useEffects && (!audioBackend.Open(EffectsFrequency, EffectsChannels) || !audioBackend.SetPaused(false))) {
        return -3;
    }
    if (!partial && useMusic && !inFlight
        && (!musicPlayer.Open(requestedTrack, true) || (!primed && !musicPlayer.Start()))) {
        return -4;
    }
    if (inFlight) {
        if (!thread->AddFileThreadItem(&CCDAudio::_PlayFile, requestedTrack, 1)) {
            return -5;
        }
        constexpr Uint64 CommandDeadlineMilliseconds = 5000;
        const Uint64 deadline = SDL_GetTicks() + CommandDeadlineMilliseconds;
        while (!commandEntered && SDL_GetTicks() < deadline) {
            SDL_Delay(1);
        }
        if (!commandEntered || !thread->AddFileThreadItem(&CCDAudio::_PlayFile, requestedTrack, 1)) {
            return -6;
        }
    }
    std::puts("fixture_audio_started");
    std::fflush(stdout);
    if (std::strcmp(requestedFailure, "controller-close-seh") == 0) {
        thread->Shutdown();
        return 0;
    }
    if (std::strcmp(requestedFailure, "join-fail") == 0) {
        SetWorkerFault(WorkerFault::Join);
        return 0;
    }
    if (std::strcmp(requestedFailure, "cpp") == 0 || partial || primed || inFlight) {
        throw std::runtime_error("controlled Run failure");
    }
#if defined(_WIN32)
    if (std::strcmp(requestedFailure, "seh") == 0) {
        constexpr ULONG_PTR InvalidReadAddress = 1;
        const ULONG_PTR arguments[] = {0, InvalidReadAddress};
        RaiseException(EXCEPTION_ACCESS_VIOLATION, 0, 2, arguments);
    }
#endif
    return 1;
}
#include "production_run.inc"

extern "C" FIXTURE_EXPORT int CseAudioFixtureRun(const char* mode, const char* failure, const char* track, int) {
    requestedMode = mode;
    requestedFailure = failure;
    requestedTrack = track;
    char commandLine[] = "-nobreakpad";
    CEngineAPI engine;
    return engine.Run(nullptr, nullptr, commandLine, nullptr, nullptr, nullptr);
}

extern "C" FIXTURE_EXPORT int CseAudioFixtureShutdown() {
    return CDAudio_ShutdownForModuleUnload() ? 1 : 0;
}

extern "C" FIXTURE_EXPORT int CseAudioFixtureState(int selector) {
    if (selector == 0) {
        return controllerShutdowns;
    }
    if (selector == 1) {
        return WorkerLiveResources();
    }
    if (selector == 3) {
        return deliveredCommands;
    }
    if (selector == 4) {
        return commandCompleted && !lateCommandAccepted ? 1 : 0;
    }
    if (selector == 5) {
        return captureCleanupCalls;
    }
    if (selector == 6) {
        return mixerCleanupCalls;
    }
    return audioBackend.IsOpen() || musicPlayer.IsPlaying()
        || static_cast<CThread*>(thread)->m_bThreadRunning ? 1 : 0;
}
