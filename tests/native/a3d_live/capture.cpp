#include "capture.h"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {
A3dCapture capture;
const A3dOpen realOpen = SDL_OpenAudioDeviceStream;
const A3dPut realPut = SDL_PutAudioStreamData;
#define CHECK(condition) do { \
    if (!(condition)) { \
        std::fprintf(stderr, "capture line %d: %s\n", __LINE__, #condition); \
        std::exit(1); \
    } \
} while (false)
SDL_AudioStream* SDLCALL ObserveOpen(SDL_AudioDeviceID id, const SDL_AudioSpec* format,
    SDL_AudioStreamCallback callback, void* context) {
    static thread_local bool forwarding = false;
    if (forwarding) {
        capture.recursion = true;
        return nullptr;
    }
    CHECK(SDL_IsMainThread());
    if (id == SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK) {
        ++capture.defaultOpens;
    } else {
        ++capture.explicitOpens;
    }
    forwarding = true;
    SDL_AudioStream* stream = realOpen(id, format, callback, context);
    forwarding = false;
    CHECK(!(capture.admitting.load() && capture.admittingMusic.load()));
    A3dStreamCapture* target = capture.admitting.load() ? &capture :
        (capture.admittingMusic.load() ? &capture.music : nullptr);
    if (stream && target) {
        CHECK(SDL_GetAudioStreamFormat(stream, &target->input, &target->conversion));
        CHECK(target->input.freq == format->freq && target->input.channels == format->channels &&
            target->input.format == format->format);
        target->requestedDevice = id;
        target->logicalDevice = SDL_GetAudioStreamDevice(stream);
        CHECK(SDL_GetAudioDeviceFormat(target->logicalDevice, &target->device, nullptr));
        const char* name = SDL_GetAudioDeviceName(target->logicalDevice);
        CHECK(name);
        target->name = name;
        target->stream.store(stream);
    }
    return stream;
}

bool SDLCALL ObservePut(SDL_AudioStream* stream, const void* data, int bytes) {
    // No locks, allocation, file I/O or logging on the audio callback. Each
    // reserved bounded byte range has one writer; main drains only after Close.
    static thread_local bool forwarding = false;
    if (forwarding) {
        capture.recursion = true;
        return false;
    }
    A3dStreamCapture* target = stream == capture.stream.load() ? &capture :
        (stream == capture.music.stream.load() ? &capture.music : nullptr);
    if (target == &capture && bytes > 0 && capture.injectFailure.exchange(false)) {
        capture.failures.fetch_add(1);
        return false; // Explicit injected transport scenario only; never music.
    }
    forwarding = true;
    const bool result = realPut(stream, data, bytes);
    forwarding = false;
    if (result && target && bytes > 0) {
        const std::size_t count = static_cast<std::size_t>(bytes);
        const std::size_t begin = target->bytes.fetch_add(count);
        if (begin < target->pcm.size()) {
            const std::size_t bounded = std::min(count, target->pcm.size() - begin);
            std::memcpy(target->pcm.data() + begin, data, bounded);
            const auto* values = static_cast<const unsigned char*>(data);
            std::size_t nonzero = 0;
            for (std::size_t index = 0; index < bounded; ++index) {
                nonzero += values[index] != 0;
            }
            target->nonzero.fetch_add(nonzero);
        }
    }
    return result;
}

__attribute__((used, section("__DATA,__interpose")))
const struct Interpose {
    const void* replacement;
    const void* original;
} interpose[] = {
    {reinterpret_cast<const void*>(ObserveOpen), reinterpret_cast<const void*>(SDL_OpenAudioDeviceStream)},
    {reinterpret_cast<const void*>(ObservePut), reinterpret_cast<const void*>(SDL_PutAudioStreamData)}
};

}
A3dCapture& A3dLiveCapture() {
    return capture;
}
A3dOpen A3dLiveOriginalOpen() {
    return realOpen;
}
A3dOpen A3dLiveObservedOpen() {
    return ObserveOpen;
}
A3dPut A3dLiveOriginalPut() {
    return realPut;
}
A3dPut A3dLiveObservedPut() {
    return ObservePut;
}
