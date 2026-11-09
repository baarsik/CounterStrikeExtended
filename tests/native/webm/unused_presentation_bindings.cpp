// Separate from the complete production TU so MSVC does not fold its player
// around these aborting bindings. Real engine declarations check the signatures.
#include "unused_presentation_bindings.h"
#include "webm_video.h"
#include "conprint.h"
#include "mathlib.h"
#include "sound.h"
#include "sys.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <crtdbg.h>

[[noreturn]] static void UnexpectedPresentationBinding(const char* name) {
    std::fprintf(stderr, "UNEXERCISED_PRESENTATION_ABORT:%s\n", name);
    std::fflush(stderr);
    std::abort();
}

void Con_Printf(const char*, ...) {
    UnexpectedPresentationBinding("Con_Printf");
}

void Con_DPrintf(const char*, ...) {
    UnexpectedPresentationBinding("Con_DPrintf");
}

void S_Update(vec_t*, vec_t*, vec_t*, vec_t*) {
    UnexpectedPresentationBinding("S_Update");
}

void S_StartDynamicSound(int, int, sfx_t*, vec_t*, float, float, int, int) {
    UnexpectedPresentationBinding("S_StartDynamicSound");
}

sfx_t* S_PrecacheSound(char*) {
    UnexpectedPresentationBinding("S_PrecacheSound");
}

double Sys_FloatTime() {
    UnexpectedPresentationBinding("Sys_FloatTime");
}

void Sys_Sleep(int) {
    UnexpectedPresentationBinding("Sys_Sleep");
}

vec3_t vec3_origin = {};

void PreparePresentationGuardProcess() {
    // Intentional abort checks must terminate without CRT or Windows crash UI.
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
#if defined(_DEBUG)
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
#endif
}

void InvokeUnusedPresentationBinding(const char* name) {
    if (std::strcmp(name, "PlayVideo") == 0) {
        WebMPlayer player;
        player.PlayVideo("unreachable-presentation-input.webm", nullptr);
    } else if (std::strcmp(name, "Con_Printf") == 0) {
        Con_Printf("unreachable");
    } else if (std::strcmp(name, "Con_DPrintf") == 0) {
        Con_DPrintf("unreachable");
    } else if (std::strcmp(name, "S_Update") == 0) {
        S_Update(nullptr, nullptr, nullptr, nullptr);
    } else if (std::strcmp(name, "S_StartDynamicSound") == 0) {
        S_StartDynamicSound(0, 0, nullptr, nullptr, 0, 0, 0, 0);
    } else if (std::strcmp(name, "S_PrecacheSound") == 0) {
        S_PrecacheSound(nullptr);
    } else if (std::strcmp(name, "Sys_FloatTime") == 0) {
        Sys_FloatTime();
    } else if (std::strcmp(name, "Sys_Sleep") == 0) {
        Sys_Sleep(0);
    }
}
