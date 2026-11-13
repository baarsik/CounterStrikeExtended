#pragma once

// Preserve the maintained Win32 device as a reference while native targets use SDL3.
#ifndef CSE_SOUND_USE_SDL
#if !defined(_WIN32) || defined(_WIN64)
#define CSE_SOUND_USE_SDL 1
#else
#define CSE_SOUND_USE_SDL 0
#endif
#endif
