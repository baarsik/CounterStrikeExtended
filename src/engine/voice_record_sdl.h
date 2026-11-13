#ifndef CSE_VOICE_RECORD_SDL_H
#define CSE_VOICE_RECORD_SDL_H

class IVoiceRecord;

namespace cse {

enum class VoiceCapturePolicy {
    DirectSound,
    WaveIn
};

// Private engine factory. Callers retain ownership through IVoiceRecord::Release.
IVoiceRecord* CreateVoiceRecordSDL(int sampleRate, VoiceCapturePolicy policy);

}

#endif
