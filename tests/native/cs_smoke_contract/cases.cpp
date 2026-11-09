#include "fixture.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>

namespace smoke_fixture {
std::uint32_t FloatBits(float value) {
    std::uint32_t bits = 0;
    static_assert(sizeof(bits) == sizeof(value), "Fixture records exact binary32 values");
    std::memcpy(&bits, &value, sizeof(bits));
    return bits;
}
void Require(bool condition, const char* description) {
    if (!condition) {
        std::fprintf(stderr, "FAIL smoke fixture: %s\n", description);
        std::exit(1);
    }
}
}
namespace {
using namespace smoke_fixture;
int caseCount = 0;
void Equal(const std::string& expected, const std::string& observed, const std::string& id) {
    if (expected != observed) {
        std::fprintf(stderr, "case=%s\nreference=%s\ncandidate=%s\n", id.c_str(), expected.c_str(), observed.c_str());
        Require(false, "exact complete-body logical output / ordered call trace");
    }
    std::printf("%s\t%s\n", id.c_str(), observed.c_str());
    ++caseCount;
}
void CreateCase(const std::string& id, const Input& input, const Control& control) {
    const auto& old = Reference();
    const auto& current = Candidate();
    old.reset(control);
    old.create(input);
    old.checkConsumed();
    const auto expected = old.capture();
    current.reset(control);
    current.create(input);
    current.checkConsumed();
    Equal(expected, current.capture(), id);
}
Control Creation(const Input& input, int selector, float fast, bool failure = false, bool fallback = false) {
    Control control;
    control.nullSprite = failure;
    control.integers.push_back(selector);
    control.models.push_back(fallback ? 0 : DefaultModel);
    if (input.type <= 1) {
        control.cvars.push_back(fast);
        if (fast > 1) {
            control.models.push_back(fallback ? 0 : DefaultModel + 1);
        }
    }
    if (!failure) {
        control.fractions.push_back(input.scale * 0.25f);
        if (input.type <= 1) {
            control.cvars.push_back(fast);
            if (fast != 1) {
                control.cvars.push_back(fast);
            }
            control.cvars.push_back(fast);
        }
        if (input.speed != 0) {
            control.integers.push_back(5);
        }
    }
    return control;
}
void Valid() {
    constexpr int TypeCount = 5;
    for (int type = 0; type < TypeCount; ++type) {
        const int selections = type <= 2 ? 4 : type == 3 ? 2 : 3;
        for (int selector = 0; selector < selections; ++selector) {
            for (int fast = 0; fast < (type <= 1 ? 3 : 1); ++fast) {
                for (int wind = 0; wind < 2; ++wind) {
                    Input input;
                    input.type = type;
                    input.wind = wind != 0;
                    CreateCase("selectors-" + std::to_string(type) + "-" + std::to_string(selector) + "-"
                        + std::to_string(fast) + "-" + std::to_string(wind), input,
                        Creation(input, selector, static_cast<float>(fast)));
                }
            }
        }
        Input input;
        input.type = type;
        Control full;
        full.full = true;
        if (type == 0) {
            full = Creation(input, 3, 0);
            full.full = true;
        }
        CreateCase("full-" + std::to_string(type), input, full);
        CreateCase("allocation-null-" + std::to_string(type), input, Creation(input, 0, 0, true));
        input.speed = 0;
        input.hasBase = false;
        CreateCase("zero-speed-no-base-" + std::to_string(type), input, Creation(input, 0, 0));
    }
    for (int type = 0; type <= 1; ++type) {
        Input input;
        input.type = type;
        for (int fast = 0; fast <= 2; ++fast) {
            CreateCase("fallback-" + std::to_string(type) + "-" + std::to_string(fast), input,
                Creation(input, 0, static_cast<float>(fast), false, true));
        }
        auto changing = Creation(input, 1, 0);
        changing.cvars = {0, 0, 2, 2};
        CreateCase("changing-cvar-" + std::to_string(type), input, changing);
    }
    for (int mask = 0; mask < 8; ++mask) {
        Input input;
        for (int component = 0; component < 3; ++component) {
            if ((mask & (1 << component)) != 0) {
                input.direction[static_cast<std::size_t>(component)] = input.origin[static_cast<std::size_t>(component)];
            }
        }
        for (int hasBase = 0; hasBase < 2; ++hasBase) {
            input.hasBase = hasBase != 0;
            CreateCase("equality-mask-" + std::to_string(mask) + "-" + std::to_string(hasBase), input,
                Creation(input, 0, 0));
        }
    }
    for (int rate : {4, 20, 25, 35}) {
        Input input;
        input.rate = rate;
        input.red = 0;
        input.green = ColorMaximum;
        input.blue = 175;
        input.scale = 1.2f;
        CreateCase("rate-color-" + std::to_string(rate), input, Creation(input, 0, 0));
    }
    for (float scale : {0.1f, 0.16f, 0.2f, 0.25f, 0.26f, 0.3f, 0.35f, 0.4f, 0.45f, 0.5f, 1.0f, 1.2f, 2.0f}) {
        Input input;
        input.scale = scale;
        CreateCase("retained-scale-" + std::to_string(FloatBits(scale)), input, Creation(input, 0, 0));
    }
    for (int speed : {0, 2, 3, 20, 25, 35, 40, 50, 70, 75, 80}) {
        Input input;
        input.speed = speed;
        auto control = Creation(input, 0, 0);
        if (speed != 0) {
            control.integers.back() = 0;
        }
        CreateCase("retained-speed-" + std::to_string(speed), input, control);
    }
    for (float target : {-2.0f, 0.25f, 0.5f, 8.5f}) {
        CallbackInput input;
        input.which = 0;
        input.target = target;
        Control control;
        control.integers = {9};
        Reference().reset(control);
        Reference().callback(input);
        Reference().checkConsumed();
        const auto expected = Reference().capture();
        Candidate().reset(control);
        Candidate().callback(input);
        Candidate().checkConsumed();
        Equal(expected, Candidate().capture(), "expand-target-" + std::to_string(FloatBits(target)));
    }
    // Complete original callback bodies, independently initialized finite data.
    for (int which = 0; which < 4; ++which) {
        for (float frame : {6.0f, std::nextafter(6.0f, 7.0f), 7.0f, std::nextafter(7.0f, 8.0f), 8.0f}) {
            for (int alpha : {74, 75, 200}) {
                CallbackInput input;
                input.which = which;
                input.frame = frame;
                input.alpha = alpha;
                input.windMagnitude = 3.5f;
                Control control;
                if (which == 0 && alpha > 74) {
                    control.integers = {0};
                } else if (which == 1 && frame > 6) {
                    control.integers = {0, 1};
                }
                Reference().reset(control);
                Reference().callback(input);
                Reference().checkConsumed();
                const auto expected = Reference().capture();
                Candidate().reset(control);
                Candidate().callback(input);
                Candidate().checkConsumed();
                Equal(expected, Candidate().capture(), "callback-" + std::to_string(which) + "-"
                    + std::to_string(FloatBits(frame)) + "-" + std::to_string(alpha));
            }
        }
    }
    for (float magnitude : {0.0f, 3.0f, std::nextafter(3.0f, 4.0f), 5.0f}) {
        CallbackInput input;
        input.which = 1;
        input.windMagnitude = magnitude;
        input.repeats = 2;
        input.alternatingEntities = true;
        Control control;
        control.integers = {1, 0, 0, 1};
        Reference().reset(control);
        Reference().callback(input);
        const auto expected = Reference().capture();
        Candidate().reset(control);
        Candidate().callback(input);
        Equal(expected, Candidate().capture(), "shared-wind-" + std::to_string(FloatBits(magnitude)));
    }
    for (const auto& velocity : {std::array<float, 3>{{0, 0, 0}}, std::array<float, 3>{{3, 4, 0}},
                                std::array<float, 3>{{4000, 0, 0}}}) {
        CallbackInput input;
        input.which = 3;
        input.velocity = velocity;
        Control control;
        Reference().reset(control);
        Reference().callback(input);
        const auto expected = Reference().capture();
        Candidate().reset(control);
        Candidate().callback(input);
        Equal(expected, Candidate().capture(), "hug-" + std::to_string(FloatBits(velocity[0])));
    }
}
void Invalid() {
#if defined(__APPLE__)
    for (int type : {-1, 5, (std::numeric_limits<int>::min)(), (std::numeric_limits<int>::max)()}) {
        for (bool full : {false, true}) {
            Control control;
            control.full = full;
            Candidate().reset(control);
            const auto before = Candidate().capture();
            Input input;
            input.type = type;
            input.nullInputs = true;
            Candidate().create(input);
            Candidate().checkConsumed();
            Equal(before, Candidate().capture(), "invalid-" + std::to_string(type) + "-" + std::to_string(full));
        }
    }
#else
    Require(false, "old Windows invalid lane must never execute");
#endif
}
}
int main(int argc, char** argv) {
    Require(argc == 2, "explicit valid or native invalid fixture mode");
    if (std::strcmp(argv[1], "--valid") == 0) {
        Valid();
    } else if (std::strcmp(argv[1], "--invalid") == 0) {
        Invalid();
    } else {
        Require(false, "known mode only");
    }
    std::fprintf(stderr, "PASS %d exact complete-body cases\n", caseCount);
    return 0;
}
