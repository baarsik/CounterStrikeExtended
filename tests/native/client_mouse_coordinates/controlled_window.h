#pragma once

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <SDL3/SDL.h>

// The retained engine headers expect their usual foundational vector typedef.
using vec3_t = float[3];
#include CSE_MOUSE_WINDOW_HEADER

struct ControlledWindow {
    int windowWidth = 1000;
    int windowHeight = 600;
    int pixelWidth = 2000;
    int pixelHeight = 1200;
    int renderWidth = 1280;
    int renderHeight = 960;
    int desktopX = 100;
    int desktopY = 50;
    float aspectX = 1.0f;
    float aspectY = 1.0f;
    float mouseX = 500.0f;
    float mouseY = 300.0f;
    // This is a controlled private sampler, not an OS/window readiness proof.
    bool sampleReady = true;
    bool sampleSucceeds = true;
    int sampleRequests = 0;
    bool windowed = true;
    bool sizeSucceeds = true;
    bool pixelsSucceed = true;
    bool modeWritesWidth = true;
    bool modeWritesHeight = true;
    int sizeReads = 0;
    int pixelReads = 0;
    int windowedReads = 0;
    int modeReads = 0;
    int aspectXReads = 0;
    int aspectYReads = 0;
    int mouseReads = 0;
    int forbiddenCalls = 0;
};

extern ControlledWindow controlled;
extern SDL_Window *ownedWindow;
void ResetWindow();
int CheckCount();
void Check(bool condition, const char *expression, int line);
#define CHECK(expression) Check((expression), #expression, __LINE__)
