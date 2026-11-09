#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif
#include "fixture_sdl.h"
#include "SDL_hints_c.h"
#include "SDL_hashtable.h"

static FixtureCounts counts;
static int unexpectedCalls;
static SDL_Window windows[2];
static bool alive[2];
static SDL_Window *keyboardFocus;
static SDL_VideoDevice fakeVideo;
static SDL_VideoDevice *_this = &fakeVideo;
static bool failBackend;
static Uint64 clockTicks;
bool SDL_object_validation = true;
#if defined(SDL_PLATFORM_WINDOWS)
static UINT WINAPI FixtureDoubleClickTime(void) {
    const UINT fixtureDoubleClickMilliseconds = 500;
    return fixtureDoubleClickMilliseconds;
}
#define GetDoubleClickTime FixtureDoubleClickTime
#endif

#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable: 4100)
#endif
#include "sdk_bodies.inc"
#if defined(_MSC_VER)
#pragma warning(pop)
#endif

static bool DriverRelative(bool enabled) {
    if (enabled) {
        ++counts.backendEnable;
    } else {
        ++counts.backendDisable;
    }
    return !failBackend;
}

static bool DriverWarp(SDL_Window *window, float x, float y) {
    ++counts.backendWarps;
    SDL_SendMouseMotion(0, window, SDL_GLOBAL_MOUSE_ID, false, x, y);
    return true;
}

void FixtureReset(void) {
    memset(&windows, 0, sizeof(windows));
    memset(&counts, 0, sizeof(counts));
    if (!SDL_PreInitMouse()) { exit(30); }
    alive[0] = true;
    alive[1] = true;
    for (int index = 0; index < 2; ++index) {
        windows[index].id = (SDL_WindowID)(index + FixtureFirstWindowId);
        windows[index].w = FixtureWindowWidth;
        windows[index].h = FixtureWindowHeight;
    }
    keyboardFocus = &windows[0];
    windows[0].flags = SDL_WINDOW_INPUT_FOCUS;
    SDL_mouse.focus = &windows[0];
    SDL_mouse.x = (float)FixtureCenterX;
    SDL_mouse.y = (float)FixtureCenterY;
    SDL_mouse.last_x = (float)FixtureCenterX;
    SDL_mouse.last_y = (float)FixtureCenterY;
    SDL_mouse.has_position = true;
    SDL_mouse.SetRelativeMouseMode = DriverRelative;
    SDL_mouse.WarpMouse = DriverWarp;
    failBackend = false;
    clockTicks = SDL_MS_TO_NS(100);
    track_mouse_down = false;
}

SDL_Window *FixtureWindow(int index) {
    return index >= 0 && index < 2 ? &windows[index] : NULL;
}

void FixtureWindowAlive(int index, bool valid, SDL_WindowID id) {
    if (index < 0 || index >= 2) {
        exit(23);
    }
    alive[index] = valid;
    windows[index].id = id;
    windows[index].is_destroying = !valid;
}

void FixtureSetFocus(SDL_Window *window) {
    keyboardFocus = window;
}

void FixtureFailBackend(bool fail) {
    failBackend = fail;
}

int FixtureUnexpectedCount(void) {
    return unexpectedCalls;
}

FixtureCounts FixtureGetCounts(void) {
    return counts;
}

void FixtureClearCounts(void) {
    memset(&counts, 0, sizeof(counts));
}

void FixtureMotion(bool relative, float x, float y) {
    SDL_SendMouseMotion(clockTicks, &windows[0], SDL_DEFAULT_MOUSE_ID, relative, x, y);
}

void FixtureHint(const char *value) {
    SDL_MouseWarpEmulationChanged(&SDL_mouse, SDL_HINT_MOUSE_EMULATE_WARP_WITH_RELATIVE, NULL, value);
}

void FixtureClock(Uint64 ticks) {
    clockTicks = ticks;
}

void FixtureInitialCursor(bool visible) {
    SDL_mouse.cursor_visible = visible;
}

void FixtureGetLayout(size_t *mouseBytes, size_t *windowBytes, size_t offsets[8]) {
    *mouseBytes = sizeof(SDL_Mouse);
    *windowBytes = sizeof(SDL_Window);
    offsets[0] = offsetof(SDL_Mouse, relative_mode);
    offsets[1] = offsetof(SDL_Mouse, warp_emulation_active);
    offsets[2] = offsetof(SDL_Mouse, warp_emulation_hint);
    offsets[3] = offsetof(SDL_Mouse, warp_emulation_prohibited);
    offsets[4] = offsetof(SDL_Mouse, x);
    offsets[5] = offsetof(SDL_Mouse, last_x);
    offsets[6] = offsetof(SDL_Window, flags);
    offsets[7] = offsetof(SDL_Window, id);
}

bool SDL_FindObject(void *object, SDL_ObjectType type) {
    if (type != SDL_OBJECT_TYPE_WINDOW) {
        exit(24);
    }
    for (int index = 0; index < 2; ++index) {
        if (object == &windows[index]) {
            return alive[index];
        }
    }
    return false;
}

SDL_Window *SDL_GetKeyboardFocus(void) {
    return keyboardFocus;
}

SDL_VideoDevice *SDL_GetVideoDevice(void) {
    return _this;
}

bool SDL_UninitializedVideo(void) {
    return SDL_SetError("Controlled video unavailable");
}

bool SDLCALL SDL_SetError(const char *format, ...) {
    (void)format;
    ++counts.errors;
    return false;
}

void SDL_RedrawCursor(void) {
    ++counts.redraws;
}

void SDL_UpdateWindowGrab(SDL_Window *window) {
    (void)window;
    ++counts.grabUpdates;
}

bool SDL_UpdateMouseCapture(bool forceRelease) {
    (void)forceRelease;
    ++counts.captureUpdates;
    return true;
}

void SDLCALL SDL_FlushEvent(Uint32 type) {
    if (type != SDL_EVENT_MOUSE_MOTION) {
        exit(25);
    }
    ++counts.motionFlushes;
}

bool SDLCALL SDL_EventEnabled(Uint32 type) {
    if (type != SDL_EVENT_MOUSE_MOTION) {
        exit(26);
    }
    return true;
}

bool SDLCALL SDL_PushEvent(SDL_Event *event) {
    if (event->type != SDL_EVENT_MOUSE_MOTION) {
        exit(27);
    }
    ++counts.queuedMotion;
    return true;
}

void SDL_SetMouseFocus(SDL_Window *window) {
    SDL_mouse.focus = window;
    SDL_mouse.has_position = false;
}

void SDL_SendTouchMotion(Uint64 timestamp, SDL_TouchID id, SDL_FingerID finger, SDL_Window *window,
    float x, float y, float pressure) {
    (void)timestamp;
    (void)id;
    (void)finger;
    (void)window;
    (void)x;
    (void)y;
    (void)pressure;
    ++unexpectedCalls;
}

bool SDLCALL SDL_GetRectIntersection(const SDL_Rect *a, const SDL_Rect *b, SDL_Rect *result) {
    (void)a;
    (void)b;
    (void)result;
    ++unexpectedCalls;
    return false;
}

Uint64 SDLCALL SDL_GetTicksNS(void) {
    return clockTicks;
}

bool SDL_GetStringBoolean(const char *value, bool defaultValue) {
    if (!value || !*value) {
        return defaultValue;
    }
    return strcmp(value, "0") != 0 && strcmp(value, "false") != 0;
}

float SDLCALL SDL_modff(float value, float *integer) {
    return modff(value, integer);
}

float SDLCALL SDL_truncf(float value) {
    return truncf(value);
}

void FixtureWarp(float x, float y) {
    ++counts.warpRequests;
    SDL_WarpMouseInWindow(&windows[0], x, y);
}

void FixtureMousePosition(float *x, float *y) {
    ++counts.mouseQueries;
    SDL_GetMouseState(x, y);
}

SDL_MouseButtonFlags FixtureDrain(float *x, float *y) {
    ++counts.relativeDrains;
    return SDL_GetRelativeMouseState(x, y);
}

void FixturePump(void) {
    ++counts.pumps;
}

float SDLCALL SDL_floorf(float value) {
    return floorf(value);
}

float SDLCALL SDL_ceilf(float value) {
    return ceilf(value);
}


void *SDLCALL SDL_memset_REAL(void *destination, int value, size_t length) {
    return memset(destination, value, length);
}

int SDLCALL SDL_atoi(const char *value) {
    return atoi(value);
}

double SDLCALL SDL_atof(const char *value) {
    return atof(value);
}

bool SDLCALL SDL_AddHintCallback(const char *name, SDL_HintCallback callback, void *userdata) {
    ++counts.hintCallbacks;
    callback(userdata, name, NULL, NULL);
    return true;
}

SDL_HashTable *SDL_CreateHashTable(int capacity, bool threadsafe, SDL_HashCallback hash,
    SDL_HashKeyMatchCallback match, SDL_HashDestroyCallback destroy, void *userdata) {
    if (capacity != 0 || !threadsafe || hash != SDL_HashID || match != SDL_KeyMatchID ||
        destroy != SDL_DestroyHashValue || userdata != NULL) { exit(31); }
    ++counts.hashCreates;
    return NULL; /* Deliberate typed allocation-failure seam; actual PreInit return is observed. */
}

Uint32 SDL_HashID(void *unused, const void *key) {
    (void)unused;
    (void)key;
    ++unexpectedCalls;
    return 0;
}

bool SDL_KeyMatchID(void *unused, const void *a, const void *b) {
    (void)unused;
    (void)a;
    (void)b;
    ++unexpectedCalls;
    return false;
}

void SDL_DestroyHashValue(void *unused, const void *key, const void *value) {
    (void)unused;
    (void)key;
    (void)value;
    ++unexpectedCalls;
}

int SDL_AddTouch(SDL_TouchID id, SDL_TouchDeviceType type, const char *name) {
    (void)id;
    (void)type;
    (void)name;
    ++unexpectedCalls;
    return -1;
}

void SDL_DelTouch(SDL_TouchID id) {
    (void)id;
    ++unexpectedCalls;
}
