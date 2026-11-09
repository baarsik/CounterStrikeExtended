#include "ivoicerecord.h"
#include "voice_record_sdl.h"

IVoiceRecord* CreateVoiceRecord_DSound(int sampleRate) {
    return cse::CreateVoiceRecordSDL(sampleRate, cse::VoiceCapturePolicy::DirectSound);
}

IVoiceRecord* CreateVoiceRecord_WaveIn(int sampleRate) {
    return cse::CreateVoiceRecordSDL(sampleRate, cse::VoiceCapturePolicy::WaveIn);
}
