// Explicit failing fixture baseline only; never selected by production targets.
#include "voice_mixer_controls_coreaudio.h"
namespace cse {
IMixerControls* CreateCoreAudioMixerControls(const CoreAudioMixerServices&) {
    return nullptr;
}
}
