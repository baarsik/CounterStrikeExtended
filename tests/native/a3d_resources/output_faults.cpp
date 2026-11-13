// Only native_output.cpp's SDL submission boundary is redirected here by this
// fixture's source-local compile definition. The real provider and owners run.
#include <SDL3/SDL.h>
#include <atomic>

std::atomic<bool> fixtureFailOutputPut{false};
std::atomic<unsigned> fixtureOutputPutFailures{0};
std::atomic<unsigned> fixtureOutputPutSuccesses{0};

extern "C" bool SDLCALL A3dFixturePutAudioStreamData(SDL_AudioStream* stream, const void* data, int length) {
    if (fixtureFailOutputPut) {
        ++fixtureOutputPutFailures;
        return false;
    }
    const bool result = SDL_PutAudioStreamData(stream, data, length);
    if (result) {
        ++fixtureOutputPutSuccesses;
    }
    return result;
}
