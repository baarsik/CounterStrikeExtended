#include "voice_record_sdl.h"
#include "fake_sdl.h"

#if defined(_WIN32)
#include <windows.h>
#define CSE_CAPTURE_MODULE_API
#else
#define CSE_CAPTURE_MODULE_API __attribute__((visibility("default")))
#endif

extern "C" CSE_CAPTURE_MODULE_API IVoiceRecord* FixtureCreateVoiceRecord(int sampleRate, int policy) {
    return cse::CreateVoiceRecordSDL(sampleRate, static_cast<cse::VoiceCapturePolicy>(policy));
}

#if defined(_WIN32)
BOOL WINAPI DllMain(HINSTANCE, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        fake_sdl::NoteModuleLoaded(true);
    } else if (reason == DLL_PROCESS_DETACH) {
        fake_sdl::NoteModuleLoaded(false);
    }
    return TRUE;
}
#else
__attribute__((constructor)) static void Loaded() {
    fake_sdl::NoteModuleLoaded(true);
}

__attribute__((destructor)) static void Unloaded() {
    fake_sdl::NoteModuleLoaded(false);
}
#endif
