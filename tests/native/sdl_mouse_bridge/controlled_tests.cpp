#include "fixture_api.h"
#include "sdl3_native_mouse_bridge.h"
#include <cstdio>
#include <cstdlib>
#include <initializer_list>

static unsigned checks = 0;

static void Require(bool value, const char *expression, int line) {
    ++checks;
    if (!value) {
        constexpr int assertionFailure = 17;
        std::fprintf(stderr, "bridge:%d:%s\n", line, expression);
        std::exit(assertionFailure);
    }
}

#define REQUIRE(expression) Require((expression), #expression, __LINE__)

static uint32_t Query(uint32_t version = CSE_SDL_MOUSE_MODE_CONTRACT_V1) {
    const uint64_t before = FixtureSnapshot();
    const uint32_t result = CseSdl_QueryMouseModeV1(version);
    REQUIRE(FixtureSnapshot() == before);
    REQUIRE(FixtureCount(FixtureHandlerCalled) == 0);
    REQUIRE(FixtureCount(FixtureUnexpectedEffect) == 0);
    return result;
}

static void GuardCases() {
    for (uint32_t version : {UINT32_C(0), UINT32_C(2), UINT32_MAX}) {
        FixtureReset();
        FixtureSetDevice(1, 0, 1);
        REQUIRE(Query(version) == CseSdlMouseModeContractMismatch);
        for (uint32_t counter = 0; counter < FixtureCounterCount; ++counter) {
            REQUIRE(FixtureCount(counter) == 0);
        }
    }
    FixtureReset();
    FixtureSetCustomHandler(1);
    REQUIRE(Query() == CseSdlMouseModeAbsolute);
    REQUIRE(FixtureCount(FixtureCurrentHandlerCheck) == 0);
    REQUIRE(FixtureCount(FixtureSetterCheck) == 1);
    FixtureReset();
    FixtureSetThread(0, 0);
    FixtureSetDevice(1, 0, 1);
    REQUIRE(Query() == CseSdlMouseModeWrongThread);
    REQUIRE(FixtureCount(FixtureMainCheck) == 1);
    REQUIRE(FixtureCount(FixtureDeviceCheck) == 0);
    REQUIRE(FixtureCount(FixtureInitCheck) == 0);
    FixtureReset();
    FixtureSetReady(0);
    FixtureSetDevice(1, 0, 1);
    REQUIRE(Query() == CseSdlMouseModeNotReady);
    REQUIRE(FixtureCount(FixtureDeviceCheck) == 0);
    FixtureReset();
    FixtureSetDevice(0, 0, 0);
    REQUIRE(Query() == CseSdlMouseModeNotReady);
    REQUIRE(FixtureCount(FixtureVideoCheck) == 0);
    FixtureReset();
    FixtureSetDevice(1, 1, 0);
    REQUIRE(Query() == CseSdlMouseModeNotReady);
    REQUIRE(FixtureCount(FixtureVideoCheck) == 0);
    FixtureReset();
    FixtureSetThread(1, 0);
    REQUIRE(Query() == CseSdlMouseModeWrongThread);
    REQUIRE(FixtureCount(FixtureDefaultCheck) == 0);
    REQUIRE(FixtureCount(FixtureModeCheck) == 0);
}

static void DispatchAndModeCases() {
    for (uint32_t relative = 0; relative < 2; ++relative) {
        FixtureReset();
        FixtureSetRelative(relative);
        FixtureSetForeign(1);
        REQUIRE(Query() == CseSdlMouseModeForeignDispatch);
        REQUIRE(FixtureCount(FixturePublicCheck) == 1);
        REQUIRE(FixtureCount(FixtureDefaultCheck) == 1);
        REQUIRE(FixtureCount(FixtureModeCheck) == 0);
        FixtureReset();
        FixtureSetRelative(relative);
        const uint32_t expected = relative ? static_cast<uint32_t>(CseSdlMouseModeRelative) :
            static_cast<uint32_t>(CseSdlMouseModeAbsolute);
        REQUIRE(Query() == expected);
        REQUIRE(FixtureCount(FixturePublicCheck) == 1);
        REQUIRE(FixtureCount(FixtureDefaultCheck) == 2);
        REQUIRE(FixtureCount(FixtureModeCheck) == 1);
        REQUIRE(FixtureCount(FixtureMouseCheck) == 1);
    }
    FixtureReset();
    constexpr unsigned iterations = 4096;
    for (unsigned index = 0; index < iterations; ++index) {
        const uint32_t relative = index % 2;
        FixtureSetRelative(relative);
        const uint32_t expected = relative ? static_cast<uint32_t>(CseSdlMouseModeRelative) :
            static_cast<uint32_t>(CseSdlMouseModeAbsolute);
        REQUIRE(Query() == expected);
    }
    FixtureSetReady(0);
    REQUIRE(Query() == CseSdlMouseModeNotReady);
    FixtureSetReady(1);
    FixtureSetRelative(0);
    REQUIRE(Query() == CseSdlMouseModeAbsolute);
}

int main() {
    static_assert(sizeof(uint32_t) == 4, "Scalar contract width");
    static_assert(CseSdlMouseModeNotReady == 0 && CseSdlMouseModeAbsolute == 1 &&
        CseSdlMouseModeRelative == 2 && CseSdlMouseModeWrongThread == 3 &&
        CseSdlMouseModeContractMismatch == 4 && CseSdlMouseModeForeignDispatch == 5, "Contract V1 values");
    GuardCases();
    DispatchAndModeCases();
    std::printf("actual-SDK controlled bridge checks=%u; provider/game qualification=false\n", checks);
    return 0;
}
