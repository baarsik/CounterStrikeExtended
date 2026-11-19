#include "mathlib.h"
#include "const.h"
#include "pm_defs.h"
#include "pm_movevars.h"
#include "movement_timing.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern playermove_t* pmove;
void PM_WalkMove(void);
int PM_FlyMove(void);

static playermove_t movement;
static movevars_t fixtureMovevars;
static const float rampNormal[3] = {0.8f, 0.36f, 0.48f};

static void Require(int condition, const char* message) {
    if (!condition) {
        fprintf(stderr, "%s\n", message);
        exit(EXIT_FAILURE);
    }
}

static const char* PhysicsInfo(const char* info, const char* key) {
    return strcmp(key, CSE_MOVEMENT_TIMING_KEY) == 0 ? info : "";
}

static pmtrace_t ClearTrace(float* start, float* end, int flags, int ignore) {
    (void)start;
    (void)flags;
    (void)ignore;
    pmtrace_t trace = {0};
    trace.fraction = 1.0f;
    VectorCopy(end, trace.endpos);
    return trace;
}

static pmtrace_t RampTrace(float* start, float* end, int flags, int ignore) {
    // A trace starting at the plane cannot advance while its requested
    // endpoint is still on or within the precision margin of the plane.
    vec3_t displacement;
    VectorSubtract(end, start, displacement);
    if (DotProduct(displacement, rampNormal) > 0.0005f) {
        return ClearTrace(start, end, flags, ignore);
    }
    pmtrace_t trace = {0};
    VectorCopy(start, trace.endpos);
    VectorCopy(rampNormal, trace.plane.normal);
    return trace;
}

static void ResetMovement(unsigned char milliseconds, int negotiated) {
    memset(&movement, 0, sizeof(movement));
    movement.movetype = MOVETYPE_WALK;
    movement.friction = 1.0f;
    movement.cmd.msec = milliseconds;
    movement.frametime = milliseconds * 0.001f;
    movement.movevars = &fixtureMovevars;
    movement.PM_Info_ValueForKey = PhysicsInfo;
    movement.PM_PlayerTrace = ClearTrace;
    if (negotiated) {
        strcpy(movement.physinfo, CSE_MOVEMENT_TIMING_VERSION);
    }
    pmove = &movement;
}

static float RunStamina(unsigned char milliseconds, int steps, int negotiated) {
    ResetMovement(milliseconds, negotiated);
    movement.velocity[0] = 250.0f;
    movement.fuser2 = 650.0f;
    for (int step = 0; step < steps; ++step) {
        PM_WalkMove();
    }
    return movement.velocity[0];
}

static float RunRamp(unsigned char milliseconds, int negotiated) {
    ResetMovement(milliseconds, negotiated);
    movement.onground = -1;
    movement.velocity[0] = 250.0f;
    movement.velocity[2] = -500.0f;
    movement.PM_PlayerTrace = RampTrace;
    PM_FlyMove();
    return Length(movement.velocity);
}

int main(void) {
    const float baseline = RunStamina(10, 1, 0);
    Require(fabsf(baseline - 219.125f) < 0.001f, "expected legacy 100 FPS stamina loss");
    Require(RunStamina(10, 1, 1) == baseline, "100 FPS movement must match legacy exactly");
    Require(fabsf(RunStamina(1, 10, 1) - baseline) < 0.001f, "stamina loss varied with FPS");
    Require(RunStamina(1, 10, 0) < baseline, "unmodified server physics must remain compatible");
    Require(RunStamina(0, 1, 1) == 250.0f, "zero-duration movement applied stamina loss");

    Require(RunRamp(1, 0) == 0.0f, "expected a legacy repeated-plane stall");
    Require(RunRamp(1, 1) > 500.0f, "same ramp plane should preserve tangential speed");
    Require(RunRamp(10, 1) == RunRamp(10, 0), "100 FPS ramp movement must match legacy");
}
