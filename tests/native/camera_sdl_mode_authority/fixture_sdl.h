#pragma once

#include "SDL_internal.h"

#ifdef __cplusplus
extern "C" {
#endif
#include "events/SDL_mouse_c.h"
#include "events/SDL_events_c.h"
#include "video/SDL_sysvideo.h"

enum {
    FixtureWindowWidth = 640,
    FixtureWindowHeight = 480,
    FixtureCenterX = FixtureWindowWidth / 2,
    FixtureCenterY = FixtureWindowHeight / 2,
    FixtureMotionX = 40,
    FixtureMotionY = 24,
    FixtureFirstWindowId = 11,
    FixtureReplacementWindowId = 99,
    FixtureAssertionFailureExit = 17
};

typedef struct FixtureCounts {
    int hintCallbacks;
    int hashCreates;
    int warpRequests;
    int backendEnable;
    int backendDisable;
    int backendWarps;
    int redraws;
    int grabUpdates;
    int captureUpdates;
    int motionFlushes;
    int pumps;
    int queuedMotion;
    int mouseQueries;
    int relativeDrains;
    int errors;
} FixtureCounts;

void FixtureReset(void);
SDL_Window *FixtureWindow(int index);
void FixtureWindowAlive(int index, bool alive, SDL_WindowID id);
void FixtureSetFocus(SDL_Window *window);
void FixtureFailBackend(bool fail);
FixtureCounts FixtureGetCounts(void);
int FixtureUnexpectedCount(void);
void FixtureClearCounts(void);
void FixtureMotion(bool relative, float x, float y);
void FixtureHint(const char *value);
void FixtureClock(Uint64 ticks);
void FixtureWarp(float x, float y);
void FixtureMousePosition(float *x, float *y);
SDL_MouseButtonFlags FixtureDrain(float *x, float *y);
void FixturePump(void);
void FixtureInitialCursor(bool visible);
void FixtureGetLayout(size_t *mouseBytes, size_t *windowBytes, size_t offsets[8]);
#ifdef __cplusplus
}
#endif
