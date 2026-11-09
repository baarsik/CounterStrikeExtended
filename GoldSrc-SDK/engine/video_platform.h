#pragma once

#include "igame.h"
#include <SDL3/SDL_video.h>

#if defined(_WIN32)
using CseVideoDC = HDC;
using CseVideoContext = HGLRC;
using CseGLModule = HINSTANCE;
#else
#include "../common/cse_rect.h"
// SDL owns the native window/context. No platform device context is borrowed.
using CseVideoDC = void*;
using CseVideoContext = SDL_GLContext;
using CseGLModule = void*;
#endif
