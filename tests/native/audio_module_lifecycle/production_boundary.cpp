#include "../../../src/engine/cd.h"
#include "../../../src/engine/ithread.h"
#include "../../../src/engine/mpg123_music.h"
#include <cstdio>
#include <cstdlib>
#if defined(_WIN32)
#include <Windows.h>
#endif
extern cse::Mpg123MusicPlayer musicPlayer;
extern "C" void SNDDMA_Shutdown();
void Voice_ReleaseCaptureForModuleUnload();
void Voice_ReleaseMixerForModuleUnload();
#include "production_audio_boundary.inc"
