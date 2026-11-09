#ifndef CSE_VOICE_CAPTURE_COMPONENT_API_H
#define CSE_VOICE_CAPTURE_COMPONENT_API_H

#if defined(CSE_CAPTURE_BASELINE)
class IVoiceRecord;
namespace cse {
enum class VoiceCapturePolicy {
    DirectSound,
    WaveIn
};
IVoiceRecord* CreateVoiceRecordSDL(int sampleRate, VoiceCapturePolicy policy);
}
#else
#include "voice_record_sdl.h"
#endif

#endif
