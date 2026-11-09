#ifndef CSE_SMOKE_FIXTURE_H
#define CSE_SMOKE_FIXTURE_H
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <sstream>
#include <array>
#include <cstdint>
#include <string>
#include <type_traits>
#include <vector>
// Parse standard declarations before retained cl_util.h defines its legacy fabs macro.
#include "includes.inc"

namespace smoke_fixture {
using HudTime = decltype(CHud::m_flTime);
static_assert(std::is_same<HudTime, float>::value, "The only projected HUD input is actual declared float time");
constexpr int ColorMaximum = 255;
constexpr int DefaultModel = 501; // Owned callback index only, never a loaded model resource.
struct Control {
    std::vector<int> integers;
    std::vector<float> fractions;
    std::vector<float> cvars;
    std::vector<int> models;
    bool full = false;
    bool nullSprite = false;
};
struct Input {
    std::array<float, 3> origin{{11, 22, 33}};
    std::array<float, 3> direction{{1, 2, 3}};
    std::array<float, 3> base{{10, -20, 30}};
    int type = 2;
    int speed = 25;
    float scale = 0.5f;
    int red = 10;
    int green = 20;
    int blue = 30;
    int rate = 35;
    bool wind = false;
    bool hasBase = true;
    bool nullInputs = false;
};
struct CallbackInput {
    int which = 0;
    float frame = 8;
    int alpha = 200;
    float scale = 0.5f;
    float target = 8.5f;
    std::array<float, 3> velocity{{3, 4, 80}};
    std::array<float, 3> normal{{0, 0, 1}};
    float windMagnitude = 0;
    int windDirection = 1;
    int repeats = 1;
    bool alternatingEntities = false;
};
struct Api {
    void (*reset)(const Control&);
    void (*create)(const Input&);
    void (*callback)(const CallbackInput&);
    std::string (*capture)();
    void (*checkConsumed)();
};
const Api& Reference();
const Api& Candidate();
std::uint32_t FloatBits(float value);
void Require(bool condition, const char* description);
}
#endif
