#ifndef CSE_VOICE_CAPTURE_FAKE_SDL_H
#define CSE_VOICE_CAPTURE_FAKE_SDL_H

#include <SDL3/SDL.h>
#include <cstddef>
#include <cstdint>

#if defined(_WIN32)
#if defined(CSE_BUILD_FAKE_SDL)
#define CSE_FAKE_API __declspec(dllexport)
#else
#define CSE_FAKE_API __declspec(dllimport)
#endif
#else
#define CSE_FAKE_API __attribute__((visibility("default")))
#endif

namespace fake_sdl {

enum class Failure {
    None,
    Allocation,
    Init,
    Open,
    FormatQuery,
    WrongFormat,
    WrongChannels,
    WrongRate,
    Resume,
    Pause,
    Available,
    Read,
    ReadOdd,
    ReadOversized,
    ReadZero
};

struct Stats {
    unsigned int allocations;
    unsigned int frees;
    unsigned int initCalls;
    unsigned int quitCalls;
    unsigned int references;
    unsigned int opens;
    unsigned int resumes;
    unsigned int pauses;
    unsigned int detaches;
    unsigned int destroys;
    unsigned int activeStreams;
    unsigned int callbacks;
    unsigned int activeCallbacks;
    unsigned int callbackViolations;
    unsigned int diagnostics;
    unsigned int readCalls;
    unsigned int invalidCalls;
    std::uint64_t allocatedBytes;
    SDL_AudioFormat requestedFormat;
    int requestedChannels;
    int requestedRate;
};

CSE_FAKE_API void Reset();
CSE_FAKE_API void Fail(Failure failure);
CSE_FAKE_API Stats Observe();
CSE_FAKE_API SDL_AudioStream* LastStream();
CSE_FAKE_API SDL_AudioStreamCallback CallbackFunction(SDL_AudioStream* stream);
CSE_FAKE_API void Push(SDL_AudioStream* stream, const void* data, std::size_t bytes,
    int additionalHint = 0, int totalHint = 0);
CSE_FAKE_API void BlockNextRead();
CSE_FAKE_API bool WaitForBlockedRead(unsigned int milliseconds);
CSE_FAKE_API void UnblockRead();
CSE_FAKE_API void PushFromWorker(SDL_AudioStream* stream, const void* data, std::size_t bytes);
CSE_FAKE_API void JoinWorker(SDL_AudioStream* stream);
CSE_FAKE_API void NextAvailable(int bytes);
CSE_FAKE_API std::size_t QueuedBytes(SDL_AudioStream* stream);
CSE_FAKE_API bool WaitForPauseAttempt(unsigned int milliseconds);
CSE_FAKE_API void HoldExternalAudioReference();
CSE_FAKE_API void ReleaseExternalAudioReference();
CSE_FAKE_API bool CallbackActive();
CSE_FAKE_API bool InsideCallback();
CSE_FAKE_API void NoteModuleLoaded(bool loaded);
CSE_FAKE_API int ModulesAlive();

}

CSE_FAKE_API void Con_DPrintf(const char* format, ...);

#endif
