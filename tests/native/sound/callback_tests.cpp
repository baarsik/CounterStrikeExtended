#include <array>
#include <cstdint>
#include <cstddef>
#include <cstdio>
#include "../../../src/engine/sound.h"
#include "../../../src/engine/sound_callback.h"

static int calls = 0;
static_assert(offsetof(sfxcache_t, data) == 5 * sizeof(int), "PCM cache layout must stay unchanged");

int VoiceSE_GetSoundDataCallback(sfxcache_s* cache, char* output, int outputBytes, int samplePosition, int sampleCount) {
    ++calls;
    cache->length = calls;
    if (outputBytes != 4 || samplePosition != 17 || sampleCount != 2) {
        return 0;
    }
    output[0] = 'v';
    output[1] = 'o';
    output[2] = 'x';
    output[3] = '!';
    return sampleCount;
}

int main() {
    const auto stored = cse::sound::VoiceDataCallbackStorage();
    const auto callback = cse::sound::ResolveDataCallback(stored);
    if (callback != &VoiceSE_GetSoundDataCallback) {
        return 1;
    }
    if (sizeof(void*) > sizeof(std::uint32_t)) {
        if (reinterpret_cast<std::uintptr_t>(callback) <= UINT32_MAX) {
            std::fputs("The native fixture must exercise a callback above 4 GiB\n", stderr);
            return 1;
        }
        constexpr std::array<int, 4> invalidKeys = {0, -1, INT32_MIN, INT32_MAX};
        for (const int key : invalidKeys) {
            if (cse::sound::ResolveDataCallback(key)) {
                return 1;
            }
        }
    }
    sfxcache_t cache{};
    std::array<char, 4> output{};
    if (callback(&cache, output.data(), static_cast<int>(output.size()), 17, 2) != 2 || cache.length != 1 || calls != 1 ||
        output != std::array<char, 4>{'v', 'o', 'x', '!'}) {
        return 1;
    }
    return 0;
}
