#include "fixture.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sstream>

namespace CSE_SMOKE_BODY_NAMESPACE {
using namespace smoke_fixture;
// Scalar projection only; NEVER allocated/cast as a CHud object or ABI layout.
struct HudTimeInput {
    HudTime m_flTime;
};
static HudTimeInput gHUD{1024.5f};
static cl_enginefunc_t gEngfuncs{};
static Queue g_SmokePuffQueue;
static int g_xWindDirection = 1;
static int g_yWindDirection = 1;
static float g_xWindMagnitude = 0;
static float g_yWindMagnitude = 0;
static char extern_i[25];
static TEMPENTITY peer;
static TEMPENTITY owned; // Static zero initialization, with actual SDK type/constructors.
static Control control;
static std::vector<std::string> callEvents;
static std::size_t integerIndex = 0;
static std::size_t fractionIndex = 0;
static std::size_t cvarIndex = 0;
static std::size_t modelIndex = 0;
static float* expectedOrigin = nullptr;
static event_api_t eventApi{};
static efx_api_t effectApi{};

using RandomInteger = decltype(gEngfuncs.pfnRandomLong(0, 0));
static RandomInteger RandomLong(RandomInteger low, RandomInteger high) {
    Require(integerIndex < control.integers.size(), "unexpected integer RNG call");
    const int result = control.integers[integerIndex++];
    Require(result >= low && result <= high, "scripted integer RNG remains inside actual requested bounds");
    callEvents.push_back("long:" + std::to_string(low) + ":" + std::to_string(high) + ":" + std::to_string(result));
    return static_cast<RandomInteger>(result);
}
static float RandomFloat(float low, float high) {
    Require(fractionIndex < control.fractions.size(), "unexpected float RNG call");
    const float result = control.fractions[fractionIndex++];
    Require(result >= low && result <= high, "scripted float RNG is in range");
    callEvents.push_back("float:" + std::to_string(FloatBits(low)) + ":" + std::to_string(FloatBits(high))
        + ":" + std::to_string(FloatBits(result)));
    return result;
}
static float Cvar(char* name) {
    Require(std::strcmp(name, "fastsprites") == 0 && cvarIndex < control.cvars.size(), "expected cvar read");
    const float value = control.cvars[cvarIndex++];
    callEvents.push_back(std::string("cvar:") + name + ":" + std::to_string(FloatBits(value)));
    return value;
}
static int FindModel(const char* name) {
    Require(modelIndex < control.models.size(), "unexpected model lookup");
    const int value = control.models[modelIndex++];
    callEvents.push_back(std::string("model:") + name + ":" + std::to_string(value));
    return value;
}
static TEMPENTITY* Sprite(float* origin, int model, float rate) {
    Require(origin == expectedOrigin, "original origin pointer passed to sprite callback");
    callEvents.push_back("sprite:" + std::to_string(model) + ":" + std::to_string(FloatBits(rate)));
    return control.nullSprite ? nullptr : &owned;
}

// Complete production bodies; no edited branch/numeric implementation mirror.
#include CSE_SMOKE_BODY_INCLUDE

static void Reset(const Control& value) {
    g_SmokePuffQueue.Update(99999);
    Require(g_SmokePuffQueue.count == 0 && !g_SmokePuffQueue.first && !g_SmokePuffQueue.last,
        "actual queue cleanup before controlled case");
    control = value;
    callEvents.clear();
    integerIndex = 0;
    fractionIndex = 0;
    cvarIndex = 0;
    modelIndex = 0;
    g_SmokePuffQueue.maxelements = value.full ? 0 : 60;
    g_SmokePuffQueue.m_flDuration = 1;
    g_xWindDirection = 1;
    g_yWindDirection = 1;
    g_xWindMagnitude = 0;
    g_yWindMagnitude = 0;
    std::memset(extern_i, 'Z', sizeof(extern_i));
    owned.flags = FTENT_GRAVITY;
    owned.entity.curstate.scale = 9;
    owned.entity.curstate.frame = 0;
    owned.entity.curstate.renderamt = 231;
    owned.entity.curstate.rendermode = kRenderTransTexture;
    owned.entity.curstate.renderfx = kRenderFxPulseSlow;
    owned.entity.curstate.rendercolor.r = 91;
    owned.entity.curstate.rendercolor.g = 92;
    owned.entity.curstate.rendercolor.b = 93;
    owned.entity.baseline.fuser1 = 17;
    owned.entity.origin = Vector(101, 102, 103);
    owned.entity.baseline.origin = Vector(201, 202, 203);
    owned.callback = nullptr;
    owned.hitcallback = nullptr;
    peer.entity.curstate.frame = 0;
    peer.entity.curstate.scale = 9;
    peer.entity.curstate.renderamt = 231;
    peer.entity.baseline.fuser1 = 17;
    peer.entity.baseline.origin = Vector(201, 202, 203);
    eventApi.EV_FindModelIndex = FindModel;
    effectApi.R_DefaultSprite = Sprite;
    gEngfuncs.pfnRandomLong = RandomLong;
    gEngfuncs.pfnRandomFloat = RandomFloat;
    gEngfuncs.pfnGetCvarFloat = Cvar;
    gEngfuncs.pEventAPI = &eventApi;
    gEngfuncs.pEfxAPI = &effectApi;
}
static void Create(const Input& value) {
    // Real signature borrows mutable arrays; controlled local copies stay alive
    // through the entire call. There is no retained pointer in the result.
    auto origin = value.origin;
    auto direction = value.direction;
    auto base = value.base;
    expectedOrigin = value.nullInputs ? nullptr : origin.data();
    EV_HLDM_CreateSmoke(expectedOrigin, value.nullInputs ? nullptr : direction.data(), value.speed, value.scale,
        value.red, value.green, value.blue, value.type, value.hasBase ? base.data() : nullptr, value.wind, value.rate);
    Require(origin == value.origin && direction == value.direction && base == value.base, "borrowed vectors unchanged");
}
static void Callback(const CallbackInput& value) {
    owned.entity.curstate.frame = value.frame;
    owned.entity.curstate.renderamt = value.alpha;
    owned.entity.curstate.scale = value.scale;
    owned.entity.baseline.fuser1 = value.target;
    owned.entity.baseline.origin = Vector(value.velocity[0], value.velocity[1], value.velocity[2]);
    g_xWindMagnitude = value.windMagnitude;
    g_yWindMagnitude = value.windMagnitude;
    g_xWindDirection = value.windDirection;
    g_yWindDirection = value.windDirection;
    peer.entity.curstate.frame = value.frame;
    peer.entity.curstate.renderamt = value.alpha;
    peer.entity.curstate.scale = value.scale;
    peer.entity.baseline.fuser1 = value.target;
    peer.entity.baseline.origin = Vector(value.velocity[0], value.velocity[1], value.velocity[2]);
    pmtrace_t plane{};
    plane.plane.normal = Vector(value.normal[0], value.normal[1], value.normal[2]);
    for (int index = 0; index < value.repeats; ++index) {
        TEMPENTITY* target = value.alternatingEntities && index % 2 != 0 ? &peer : &owned;
        if (value.which == 0) {
            SmokeRise_Wind_Expand(target, 0.01f, 1024.5f);
        } else if (value.which == 1) {
            SmokeRise_Wind(target, 0.01f, 1024.5f);
        } else if (value.which == 2) {
            SmokeRise(target, 0.01f, 1024.5f);
        } else {
            HugWalls(target, &plane);
        }
    }
}
static void Consumed() {
    Require(integerIndex == control.integers.size() && fractionIndex == control.fractions.size()
        && cvarIndex == control.cvars.size() && modelIndex == control.models.size(), "complete callback sequence consumed");
}
static std::string Capture() {
    std::ostringstream out;
    const int callback = owned.callback == SmokeRise_Wind_Expand ? 1 : owned.callback == SmokeRise_Wind ? 2
        : owned.callback == SmokeRise ? 3 : owned.callback ? -1 : 0;
    out << "q=" << g_SmokePuffQueue.count << ",first=" << (g_SmokePuffQueue.first != nullptr);
    if (g_SmokePuffQueue.first) {
        out << ",time=" << FloatBits(g_SmokePuffQueue.first->flTimeCreated);
    }
    out << ";flags=" << owned.flags << ";scale=" << FloatBits(owned.entity.curstate.scale)
        << ";frame=" << FloatBits(owned.entity.curstate.frame) << ";amt=" << owned.entity.curstate.renderamt
        << ";mode=" << owned.entity.curstate.rendermode << ";fx=" << owned.entity.curstate.renderfx
        << ";rgb=" << static_cast<unsigned int>(owned.entity.curstate.rendercolor.r) << ','
        << static_cast<unsigned int>(owned.entity.curstate.rendercolor.g) << ','
        << static_cast<unsigned int>(owned.entity.curstate.rendercolor.b)
        << ";target=" << FloatBits(owned.entity.baseline.fuser1) << ";callback=" << callback
        << ";hit=" << (owned.hitcallback == HugWalls ? 1 : owned.hitcallback ? -1 : 0)
        << ";origin=" << FloatBits(owned.entity.origin.x) << ',' << FloatBits(owned.entity.origin.y)
        << ',' << FloatBits(owned.entity.origin.z) << ";velocity=" << FloatBits(owned.entity.baseline.origin.x)
        << ',' << FloatBits(owned.entity.baseline.origin.y) << ',' << FloatBits(owned.entity.baseline.origin.z)
        << ";wind=" << g_xWindDirection << ',' << g_yWindDirection << ',' << FloatBits(g_xWindMagnitude)
        << ',' << FloatBits(g_yWindMagnitude) << ";peer=" << FloatBits(peer.entity.baseline.origin.x)
        << ',' << FloatBits(peer.entity.baseline.origin.y) << ',' << FloatBits(peer.entity.baseline.origin.z)
        << ";extern=";
    for (unsigned char byte : extern_i) {
        out << static_cast<unsigned int>(byte) << ',';
    }
    for (const auto& event : callEvents) {
        out << ';' << event;
    }
    return out.str();
}
}
namespace smoke_fixture {
const Api& CSE_SMOKE_BODY_API() {
    static const Api value{CSE_SMOKE_BODY_NAMESPACE::Reset, CSE_SMOKE_BODY_NAMESPACE::Create,
        CSE_SMOKE_BODY_NAMESPACE::Callback, CSE_SMOKE_BODY_NAMESPACE::Capture, CSE_SMOKE_BODY_NAMESPACE::Consumed};
    return value;
}
}
