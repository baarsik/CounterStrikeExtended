#define CSE_BRIDGE_FIXTURE_BUILD 1
#include "fixture_api.h"
#include "SDL_internal.h"
#include "events/SDL_mouse_c.h"
#include "video/SDL_sysvideo.h"

static SDL_Mouse SDL_mouse;
static SDL_VideoDevice fixtureVideo;
static SDL_VideoDevice *_this;
static SDL_ThreadID SDL_MainThreadID;
static SDL_ThreadID SDL_EventsThreadID;
static SDL_ThreadID SDL_VideoThreadID;
static Uint8 SDL_SubsystemRefCount[32];
static SDL_ThreadID fixtureCurrentThread;
static uint32_t fixtureCounts[FixtureCounterCount];
static bool fixtureForeign;
static bool fixturePoison;
static SDL_AssertionHandler assertion_handler;
static void *assertion_userdata;

static SDL_AssertState SDLCALL SDL_PromptAssertion(const SDL_AssertData *data, void *userdata) {
    (void)data;
    (void)userdata;
    ++fixtureCounts[FixtureHandlerCalled];
    return SDL_ASSERTION_ABORT;
}

static SDL_AssertState SDLCALL FixtureForeignHandler(const SDL_AssertData *data, void *userdata) {
    (void)data;
    (void)userdata;
    ++fixtureCounts[FixtureHandlerCalled];
    // Distinct observable body prevents intra-image ICF from folding this controlled identity.
    return SDL_ASSERTION_IGNORE;
}

SDL_ThreadID SDLCALL SDL_GetCurrentThreadID_REAL(void) {
    ++fixtureCounts[FixtureThreadCheck];
    return fixtureCurrentThread;
}

#include "sdk_bodies.inc"

#undef SDL_GetDefaultAssertionHandler
SDL_AssertionHandler SDLCALL SDL_GetDefaultAssertionHandler(void) {
    ++fixtureCounts[FixturePublicCheck];
    return fixtureForeign ? FixtureForeignHandler : SDL_GetDefaultAssertionHandler_REAL();
}

void FixtureReset(void) {
    size_t index;
    unsigned char *bytes = (unsigned char *)&SDL_mouse;
    for (index = 0; index < sizeof(SDL_mouse); ++index) {
        bytes[index] = 0;
    }
    bytes = (unsigned char *)&fixtureVideo;
    for (index = 0; index < sizeof(fixtureVideo); ++index) {
        bytes[index] = 0;
    }
    for (index = 0; index < SDL_arraysize(SDL_SubsystemRefCount); ++index) {
        SDL_SubsystemRefCount[index] = 0;
    }
    for (index = 0; index < FixtureCounterCount; ++index) {
        fixtureCounts[index] = 0;
    }
    SDL_MainThreadID = 1;
    SDL_EventsThreadID = 1;
    SDL_VideoThreadID = 1;
    fixtureCurrentThread = 1;
    fixtureVideo.thread = 1;
    _this = &fixtureVideo;
    SDL_SubsystemRefCount[SDL_MostSignificantBitIndex32(SDL_INIT_VIDEO)] = 1;
    fixtureForeign = false;
    fixturePoison = false;
    assertion_handler = SDL_PromptAssertion;
    assertion_userdata = NULL;
    SDL_mouse.x_accu = 17.25f;
    SDL_mouse.y_accu = -3.5f;
}

void FixtureSetForeign(uint32_t foreign) {
    fixtureForeign = foreign != 0;
}

void FixtureSetRelative(uint32_t relative) {
    SDL_mouse.relative_mode = relative != 0;
}

void FixtureSetReady(uint32_t ready) {
    SDL_SubsystemRefCount[SDL_MostSignificantBitIndex32(SDL_INIT_VIDEO)] = ready != 0;
}

void FixtureSetThread(uint32_t mainThread, uint32_t videoThread) {
    fixtureCurrentThread = mainThread ? 1 : 2;
    fixtureVideo.thread = videoThread ? fixtureCurrentThread : 3;
}

void FixtureSetDevice(uint32_t present, uint32_t quitting, uint32_t poison) {
    fixtureVideo.is_quitting = quitting != 0;
    fixturePoison = poison != 0;
    _this = fixturePoison ? (SDL_VideoDevice *)(uintptr_t)UINTPTR_MAX : (present ? &fixtureVideo : NULL);
}

void FixtureSetCustomHandler(uint32_t custom) {
    SDL_SetAssertionHandler_REAL(custom ? FixtureForeignHandler : NULL, NULL);
}

bool SDL_SetError_REAL(const char *format, ...) {
    (void)format;
    ++fixtureCounts[FixtureUnexpectedEffect];
    return false;
}

void *SDL_malloc_REAL(size_t bytes) {
    (void)bytes;
    ++fixtureCounts[FixtureUnexpectedEffect];
    return NULL;
}

void SDL_free_REAL(void *storage) {
    (void)storage;
    ++fixtureCounts[FixtureUnexpectedEffect];
}

void SDL_PumpEvents_REAL(void) {
    ++fixtureCounts[FixtureUnexpectedEffect];
}

void SDL_WarpMouseInWindow_REAL(SDL_Window *window, float x, float y) {
    (void)window;
    (void)x;
    (void)y;
    ++fixtureCounts[FixtureUnexpectedEffect];
}

bool SDL_SetWindowRelativeMouseMode_REAL(SDL_Window *window, bool enabled) {
    (void)window;
    (void)enabled;
    ++fixtureCounts[FixtureUnexpectedEffect];
    return false;
}

bool SDL_SetHint_REAL(const char *name, const char *value) {
    (void)name;
    (void)value;
    ++fixtureCounts[FixtureUnexpectedEffect];
    return false;
}

void SDL_Log_REAL(const char *format, ...) {
    (void)format;
    ++fixtureCounts[FixtureUnexpectedEffect];
}

SDL_MouseButtonFlags SDL_GetRelativeMouseState_REAL(float *x, float *y) {
    (void)x;
    (void)y;
    ++fixtureCounts[FixtureUnexpectedEffect];
    SDL_mouse.x_accu = 0.0f;
    SDL_mouse.y_accu = 0.0f;
    return 0;
}

uint32_t FixtureCount(uint32_t counter) {
    return counter < FixtureCounterCount ? fixtureCounts[counter] : UINT32_MAX;
}

static uint64_t FixtureHash(uint64_t value, const void *storage, size_t count) {
    const unsigned char *bytes = (const unsigned char *)storage;
    size_t index;
    const uint64_t multiplier = UINT64_C(1099511628211);
    for (index = 0; index < count; ++index) {
        value = (value ^ bytes[index]) * multiplier;
    }
    return value;
}

uint64_t FixtureSnapshot(void) {
    const uint64_t offset = UINT64_C(14695981039346656037);
    uint64_t value = FixtureHash(offset, &SDL_mouse, sizeof(SDL_mouse));
    value = FixtureHash(value, &fixtureVideo, sizeof(fixtureVideo));
    value = FixtureHash(value, SDL_SubsystemRefCount, sizeof(SDL_SubsystemRefCount));
    value = FixtureHash(value, &_this, sizeof(_this));
    value = FixtureHash(value, &assertion_handler, sizeof(assertion_handler));
    value = FixtureHash(value, &assertion_userdata, sizeof(assertion_userdata));
    value = FixtureHash(value, &SDL_MainThreadID, sizeof(SDL_MainThreadID));
    value = FixtureHash(value, &SDL_EventsThreadID, sizeof(SDL_EventsThreadID));
    value = FixtureHash(value, &SDL_VideoThreadID, sizeof(SDL_VideoThreadID));
    return value;
}
