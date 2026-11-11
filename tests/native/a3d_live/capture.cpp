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
    forwarding = true;
    SDL_AudioStream* stream = realOpen(id, format, callback, context);
    forwarding = false;
    if (stream && capture.admitting.load()) {
        CHECK(SDL_GetAudioStreamFormat(stream, &capture.input, &capture.conversion));
        CHECK(capture.input.freq == format->freq && capture.input.channels == format->channels &&
            capture.input.format == format->format);
        capture.logicalDevice = SDL_GetAudioStreamDevice(stream);
        CHECK(SDL_GetAudioDeviceFormat(capture.logicalDevice, &capture.device, nullptr));
        const char* name = SDL_GetAudioDeviceName(capture.logicalDevice);
        std::snprintf(capture.name.data(), capture.name.size(), "%s", name ? name : "unknown");
        capture.stream.store(stream);
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
    if (stream == capture.stream.load() && bytes > 0) {
        if (capture.injectFailure.exchange(false)) {
            capture.failures.fetch_add(1);
            return false; // Explicit injected transport scenario only.
        }
        const std::size_t count = static_cast<std::size_t>(bytes);
        const std::size_t begin = capture.bytes.fetch_add(count);
        if (begin < capture.pcm.size()) {
            std::memcpy(capture.pcm.data() + begin, data, std::min(count, capture.pcm.size() - begin));
        }
        const auto* samples = static_cast<const std::int16_t*>(data);
        std::size_t nonzero = 0;
        for (std::size_t index = 0; index < count / sizeof(*samples); ++index) {
            nonzero += samples[index] != 0;
        }
        capture.nonzero.fetch_add(nonzero);
    }
    forwarding = true;
    const bool result = realPut(stream, data, bytes);
    forwarding = false;
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
A3dPut A3dLiveOriginalPut() {
    return realPut;
}
A3dPut A3dLiveObservedPut() {
    return ObservePut;
}
