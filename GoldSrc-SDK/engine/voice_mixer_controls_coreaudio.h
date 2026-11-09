#ifndef CSE_VOICE_MIXER_CONTROLS_COREAUDIO_H
#define CSE_VOICE_MIXER_CONTROLS_COREAUDIO_H

#include <CoreAudio/CoreAudio.h>
#include <CoreFoundation/CoreFoundation.h>
#include <cstddef>

class IMixerControls;

namespace cse {

// Private adapter: preserve each SDK signature and prepend the per-instance context.
template<typename Function>
struct CoreAudioMixerContextFunction;

template<typename Result, typename... Arguments>
struct CoreAudioMixerContextFunction<Result (*)(Arguments...)> {
    using Type = Result (*)(void*, Arguments...);
};

struct CoreAudioMixerServices {
    void* context = nullptr;
    CoreAudioMixerContextFunction<decltype(&AudioObjectHasProperty)>::Type hasProperty = nullptr;
    CoreAudioMixerContextFunction<decltype(&AudioObjectIsPropertySettable)>::Type isSettable = nullptr;
    CoreAudioMixerContextFunction<decltype(&AudioObjectGetPropertyDataSize)>::Type getSize = nullptr;
    CoreAudioMixerContextFunction<decltype(&AudioObjectGetPropertyData)>::Type getData = nullptr;
    CoreAudioMixerContextFunction<decltype(&AudioObjectSetPropertyData)>::Type setData = nullptr;
    void* (*allocate)(void*, std::size_t) = nullptr;
    void (*free)(void*, void*) = nullptr;
    void (*releaseCF)(void*, CFTypeRef) = nullptr;
};

// Table is copied; its context must outlive the returned object and all calls.
// Owner-thread operations are serialized. Release consumes the object once.
IMixerControls* CreateCoreAudioMixerControls(const CoreAudioMixerServices& services);

}

#endif
