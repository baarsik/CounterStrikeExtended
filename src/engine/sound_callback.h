#pragma once

#include <cstdint>

struct sfxcache_s;
int VoiceSE_GetSoundDataCallback(sfxcache_s* cache, char* output, int outputBytes, int samplePosition, int sampleCount);

namespace cse::sound {

using DataCallback = decltype(&VoiceSE_GetSoundDataCallback);

// Voice is the sole callback-cache producer. Its native key is process-local, never a sample-loop offset.
constexpr int VoiceDataCallbackKey = 1;

inline int VoiceDataCallbackStorage() {
#if UINTPTR_MAX == UINT32_MAX
    return reinterpret_cast<int>(&VoiceSE_GetSoundDataCallback);
#else
    return VoiceDataCallbackKey;
#endif
}

inline DataCallback ResolveDataCallback(int storage) {
#if UINTPTR_MAX == UINT32_MAX
    return reinterpret_cast<DataCallback>(storage);
#else
    return storage == VoiceDataCallbackKey ? &VoiceSE_GetSoundDataCallback : nullptr;
#endif
}

}
