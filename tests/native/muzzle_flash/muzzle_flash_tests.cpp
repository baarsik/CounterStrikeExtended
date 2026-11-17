#include "client.h"
#include "gl_model.h"
#include "r_efx.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>

extern "C" void CseSteamModuleInitialize() {}

client_state_t cl{};
efx_api_t efx{};
model_t sprites[3]{};
model_t* cl_sprite_muzzleflash[3] = {&sprites[0], &sprites[1], &sprites[2]};
TEMPENTITY entity{};
int allocations = 0;
int submissions = 0;
int frameQueries = 0;
int randomCalls = 0;
bool allocationAvailable = true;
model_t* allocatedModel = nullptr;

void Check(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", message);
        std::exit(EXIT_FAILURE);
    }
}

TEMPENTITY* Allocate(float* origin, model_t* model) {
    ++allocations;
    allocatedModel = model;
    if (!allocationAvailable) {
        return nullptr;
    }
    entity = {};
    std::copy(origin, origin + 3, entity.entity.origin);
    return &entity;
}

int ModelFrameCount(model_t*) {
    ++frameQueries;
    return 8;
}

int RandomLong(int minimum, int) {
    ++randomCalls;
    return minimum;
}

void AppendTEntity(cl_entity_t* value) {
    Check(value == &entity.entity, "submit allocated sprite");
    ++submissions;
}

#include "muzzle_flash.inc"

int main() {
    cl.time = 10;
    efx.CL_TempEntAlloc = Allocate;
    // The recorded custom model transforms an infinite attachment into (+inf,+inf,-inf).
    // Test every axis/sign and NaN as well: no invalid flash may enter the temporary-entity list.
    for (int axis = 0; axis < 3; ++axis) {
        for (float invalid : {std::numeric_limits<float>::infinity(),
            -std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN()}) {
            float origin[3] = {10, 20, 30};
            origin[axis] = invalid;
            R_MuzzleFlash(origin, 11);
            Check(allocations == 0 && submissions == 0 && frameQueries == 0 && randomCalls == 0,
                "nonfinite attachment must not allocate, query, randomize or submit a muzzle flash");
        }
    }
    float origin[3] = {1104.25f, 1401.25f, -49.0f};
    R_MuzzleFlash(origin, 11);
    Check(allocations == 1 && submissions == 1 && allocatedModel == &sprites[1], "finite flash retains model");
    Check(entity.entity.origin[0] == origin[0] && entity.entity.origin[1] == origin[1] &&
        entity.entity.origin[2] == origin[2], "finite attachment position unchanged");
    Check(entity.entity.curstate.scale == 0.1f && entity.entity.curstate.rendermode == kRenderTransAdd &&
        entity.entity.curstate.renderamt == 255 && entity.entity.curstate.renderfx == kRenderFxNone,
        "finite flash retains scale, additive material and opacity");
    Check(entity.entity.curstate.rendercolor.r == 255 && entity.entity.curstate.rendercolor.g == 255 &&
        entity.entity.curstate.rendercolor.b == 255 && entity.frameMax == 8 &&
        entity.die == static_cast<float>(cl.time + 0.01), "finite flash retains color, frames and lifetime");
    R_MuzzleFlash(origin, 0);
    Check(entity.entity.curstate.scale == 0.5f && allocatedModel == &sprites[0], "default rifle flash preserved");
    allocationAvailable = false;
    R_MuzzleFlash(origin, 12);
    Check(allocations == 3 && submissions == 2, "exhausted temporary-entity pool handled");
    std::puts("PASS muzzle flash finite origins, unchanged attributes and pool exhaustion");
}
