#include "mathlib.h"
#include "const.h"
#include "pm_defs.h"
#include "pm_movevars.h"
#include "pm_shared.h"
#include "movement_timing.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern playermove_t* pmove;
void PM_WalkMove(void);
int PM_FlyMove(void);
void PM_SpectatorMove(void);
void PM_CatagorizePosition(void);

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
    // An older CSE server may still send its private movement flag.
    if (strcmp(key, "_cse_pmove") == 0) {
        return info;
    }
    if (strcmp(key, CSE_REGAMEDLL_MOVEMENT_KEY) == 0 && strcmp(info, "ReGameDLL") == 0) {
        return CSE_REGAMEDLL_MOVEMENT_VERSION;
    }
    return "";
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

static void ResetMovement(unsigned char milliseconds, int oldCseFlag) {
    memset(&movement, 0, sizeof(movement));
    movement.movetype = MOVETYPE_WALK;
    movement.friction = 1.0f;
    movement.cmd.msec = milliseconds;
    movement.frametime = milliseconds * 0.001f;
    movement.movevars = &fixtureMovevars;
    movement.PM_Info_ValueForKey = PhysicsInfo;
    movement.PM_PlayerTrace = ClearTrace;
    if (oldCseFlag) {
        strcpy(movement.physinfo, "1");
    }
    pmove = &movement;
}

static float RunStamina(unsigned char milliseconds, int steps, int oldCseFlag) {
    ResetMovement(milliseconds, oldCseFlag);
    movement.velocity[0] = 250.0f;
    movement.fuser2 = 650.0f;
    for (int step = 0; step < steps; ++step) {
        PM_WalkMove();
    }
    return movement.velocity[0];
}

static float RunRamp(unsigned char milliseconds, int oldCseFlag) {
    ResetMovement(milliseconds, oldCseFlag);
    movement.onground = -1;
    movement.velocity[0] = 250.0f;
    movement.velocity[2] = -500.0f;
    movement.PM_PlayerTrace = RampTrace;
    PM_FlyMove();
    return Length(movement.velocity);
}

static int EmptyContents(float* point, int* contents) {
    (void)point;
    if (contents) {
        *contents = CONTENTS_EMPTY;
    }
    return CONTENTS_EMPTY;
}

static pmtrace_t FloorTrace(float* start, float* end, int flags, int ignore) {
    pmtrace_t trace = ClearTrace(start, end, flags, ignore);
    trace.fraction = 0.5f;
    trace.plane.normal[2] = 1.0f;
    trace.endpos[2] = (start[2] + end[2]) * 0.5f;
    return trace;
}

static void CheckGroundSnapping(int movetype, int server, int negotiated, float expectedZ, int expectedGround) {
    ResetMovement(10, 0);
    movement.movetype = movetype;
    movement.server = server;
    movement.origin[2] = 16.0f;
    movement.PM_PointContents = EmptyContents;
    movement.PM_PlayerTrace = FloorTrace;
    if (negotiated) {
        strcpy(movement.physinfo, "ReGameDLL");
    }
    PM_CatagorizePosition();
    Require(movement.origin[2] == expectedZ && movement.onground == expectedGround,
            "ground categorization should match the selected movement version");
}

static void CheckSpectatorCoasting(int negotiated, float expectedDistance) {
    ResetMovement(10, 0);
    movement.iuser1 = OBS_ROAMING;
    movement.forward[0] = 1.0f;
    movement.cmd.forwardmove = 50.0f;
    movement.velocity[0] = 100.0f;
    fixtureMovevars.spectatormaxspeed = 500.0f;
    if (negotiated) {
        strcpy(movement.physinfo, "ReGameDLL");
    }
    PM_SpectatorMove();
    Require(fabsf(movement.origin[0] - expectedDistance) < 0.00001f,
            "spectator should coast only with ReGameDLL movement 1.0");
}

int main(void) {
    const float baseline = RunStamina(10, 1, 0);
    Require(fabsf(baseline - 219.125f) < 0.001f, "expected legacy 100 FPS stamina loss");
    const float stockShortSteps = RunStamina(1, 10, 0);
    Require(stockShortSteps < baseline, "stock stamina must apply its penalty per movement step");
    Require(RunStamina(1, 10, 1) == stockShortSteps, "the old server flag changed stock stamina prediction");

    Require(RunRamp(1, 0) == 0.0f, "expected a legacy repeated-plane stall");
    Require(RunRamp(1, 1) == 0.0f, "the old server flag changed stock ramp prediction");

    CheckGroundSnapping(MOVETYPE_NOCLIP, 0, 0, 15.0f, 0);
    CheckGroundSnapping(MOVETYPE_NOCLIP, 0, 1, 16.0f, -1);
    CheckGroundSnapping(MOVETYPE_NONE, 0, 1, 16.0f, -1);
    CheckGroundSnapping(MOVETYPE_WALK, 0, 1, 15.0f, 0);
    CheckGroundSnapping(MOVETYPE_NOCLIP, 1, 1, 15.0f, 0);
    CheckSpectatorCoasting(0, 0.0f);
    CheckSpectatorCoasting(1, 1.0f);
}
