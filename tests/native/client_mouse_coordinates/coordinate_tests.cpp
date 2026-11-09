#include "controlled_window.h"
#include <array>

struct Output {
    std::uint64_t before = 0x123456789abcdef0ULL;
    int x = 101;
    int y = 202;
    int centerX = 303;
    int centerY = 404;
    std::uint64_t after = 0xfedcba9876543210ULL;
};

static bool Query(Output &output, SDL_Window *window = ownedWindow) {
    return cse::TryGetEngineMousePositionAndCenter(window, &output.x, &output.y, &output.centerX, &output.centerY);
}

static void CheckGuards(const Output &output) {
    CHECK(output.before == 0x123456789abcdef0ULL && output.after == 0xfedcba9876543210ULL);
}

static void CheckUnchanged(const Output &output) {
    CHECK(output.x == 101 && output.y == 202 && output.centerX == 303 && output.centerY == 404);
    CheckGuards(output);
}

static void CheckCalls(bool expectMouse = true) {
    CHECK(controlled.sizeReads == 1 && controlled.windowedReads == 1);
    CHECK(controlled.pixelReads == (controlled.windowed ? 1 : 0));
    CHECK(controlled.modeReads == (controlled.windowed ? 0 : 1));
    CHECK(controlled.aspectXReads == (controlled.windowed ? 0 : 1));
    CHECK(controlled.aspectYReads == (controlled.windowed ? 0 : 1));
    CHECK(controlled.mouseReads == (expectMouse ? 1 : 0));
    CHECK(controlled.forbiddenCalls == 0);
}

static void CheckNoCalls() {
    CHECK(controlled.sizeReads == 0 && controlled.pixelReads == 0 && controlled.mouseReads == 0);
    CHECK(controlled.windowedReads == 0 && controlled.modeReads == 0);
    CHECK(controlled.aspectXReads == 0 && controlled.aspectYReads == 0 && controlled.forbiddenCalls == 0);
}

static void TestCoordinates() {
    ResetWindow();
    auto *output = new Output;
    if (sizeof(void *) == 8) {
        CHECK(reinterpret_cast<std::uintptr_t>(output) > std::numeric_limits<std::uint32_t>::max());
    }
    CHECK(Query(*output));
    CHECK(output->x == 1000 && output->y == 600 && output->centerX == 1000 && output->centerY == 600);
    CHECK(output->centerX != controlled.desktopX + controlled.pixelWidth / 2);
    CHECK(output->centerY != controlled.desktopY + controlled.pixelHeight / 2);
    CHECK(output->centerX != static_cast<int>(vid.width / 2));
    CheckCalls();
    CheckGuards(*output);
    delete output;

    ResetWindow();
    controlled.windowed = false;
    controlled.windowWidth = 1920;
    controlled.windowHeight = 1080;
    controlled.aspectX = 4.0f / 3.0f;
    controlled.mouseX = 960.0f;
    controlled.mouseY = 540.0f;
    Output fullscreen;
    CHECK(Query(fullscreen));
    CHECK(fullscreen.x == 640 && fullscreen.y == 480 && fullscreen.centerX == 640 && fullscreen.centerY == 480);
    CheckCalls();

    ResetWindow();
    controlled.windowed = false;
    controlled.windowWidth = 500;
    controlled.windowHeight = 500;
    controlled.renderWidth = 1280;
    controlled.renderHeight = 720;
    controlled.aspectY = 16.0f / 9.0f;
    controlled.mouseX = 125.0f;
    controlled.mouseY = 250.0f;
    Output letterbox;
    CHECK(Query(letterbox));
    CHECK(letterbox.x == 320 && letterbox.y == 360 && letterbox.centerX == 640 && letterbox.centerY == 360);
    CheckCalls();

    ResetWindow();
    controlled.windowed = false;
    controlled.windowWidth = 997;
    controlled.windowHeight = 599;
    controlled.renderWidth = 1281;
    controlled.renderHeight = 721;
    controlled.aspectX = 1.25f;
    controlled.aspectY = 0.75f;
    controlled.mouseX = -17.25f;
    controlled.mouseY = 300.125f;
    Output odd;
    CHECK(Query(odd));
    CHECK(odd.centerX == 640 && odd.centerY == 360);
    CHECK(odd.x == cse::WindowToRenderCoordinate(controlled.mouseX, 997, 1281, 1.25f));
    CHECK(odd.y == cse::WindowToRenderCoordinate(controlled.mouseY, 599, 721, 0.75f));
    CheckCalls();
}

static void TestLegacyArithmeticCorpus() {
    constexpr std::array<int, 5> extents = {{1, 3, 599, 997, 8192}};
    constexpr std::array<float, 5> coordinates = {{-30000.125f, -0.75f, 0.0f, 0.75f, 30000.125f}};
    constexpr std::array<float, 5> aspects = {{0.5f, 0.75f, 1.0f, 1.25f, 4.0f}};
    for (int window : extents) {
        for (int render : extents) {
            for (float position : coordinates) {
                for (float aspect : aspects) {
                    ResetWindow();
                    controlled.windowed = false;
                    controlled.windowWidth = window;
                    controlled.windowHeight = render;
                    controlled.renderWidth = render;
                    controlled.renderHeight = window;
                    controlled.mouseX = position;
                    controlled.mouseY = -position;
                    controlled.aspectX = aspect;
                    controlled.aspectY = aspects[aspects.size() - 1] / aspect;
                    Output output;
                    CHECK(Query(output));
                    CHECK(output.x == cse::WindowToRenderCoordinate(position, window, render, aspect));
                    CHECK(output.y == cse::WindowToRenderCoordinate(-position, render, window, controlled.aspectY));
                    CHECK(output.centerX == render / 2 && output.centerY == window / 2);
                    CheckCalls();
                    CheckGuards(output);
                }
            }
        }
    }
}

static void ExpectFailure(int caseLine) {
    Output output;
    const bool succeeded = Query(output);
    if (succeeded) {
        std::fprintf(stderr, "unexpected-success-case:%d\n", caseLine);
    }
    CHECK(!succeeded);
    CheckUnchanged(output);
    CHECK(controlled.forbiddenCalls == 0);
}

static void TestMissingAndExtentFailures() {
    ResetWindow();
    Output output;
    CHECK(!Query(output, nullptr));
    CheckNoCalls();
    CheckUnchanged(output);
    videomode = nullptr;
    CHECK(!Query(output));
    CheckNoCalls();
    CheckUnchanged(output);
    ResetWindow();
    CHECK(!cse::TryGetEngineMousePositionAndCenter(ownedWindow, nullptr, &output.y, &output.centerX, &output.centerY));
    CHECK(!cse::TryGetEngineMousePositionAndCenter(ownedWindow, &output.x, nullptr, &output.centerX, &output.centerY));
    CHECK(!cse::TryGetEngineMousePositionAndCenter(ownedWindow, &output.x, &output.y, nullptr, &output.centerY));
    CHECK(!cse::TryGetEngineMousePositionAndCenter(ownedWindow, &output.x, &output.y, &output.centerX, nullptr));
    CheckNoCalls();
    CheckUnchanged(output);
    ResetWindow();
    controlled.sizeSucceeds = false;
    ExpectFailure(__LINE__);
    CHECK(controlled.sizeReads == 1 && controlled.pixelReads == 0 && controlled.mouseReads == 0);
    ResetWindow();
    controlled.pixelsSucceed = false;
    ExpectFailure(__LINE__);
    CHECK(controlled.sizeReads == 1 && controlled.pixelReads == 1 && controlled.mouseReads == 0);
    for (int invalid : {0, -1, std::numeric_limits<int>::min()}) {
        for (int field = 0; field < 6; ++field) {
            ResetWindow();
            if (field == 0) {
                controlled.windowWidth = invalid;
            } else if (field == 1) {
                controlled.windowHeight = invalid;
            } else if (field == 2) {
                controlled.pixelWidth = invalid;
            } else if (field == 3) {
                controlled.pixelHeight = invalid;
            } else {
                controlled.windowed = false;
                if (field == 4) {
                    controlled.renderWidth = invalid;
                } else {
                    controlled.renderHeight = invalid;
                }
            }
            ExpectFailure(__LINE__);
            CHECK(controlled.mouseReads == 0);
        }
    }
    for (int axis = 0; axis < 2; ++axis) {
        ResetWindow();
        controlled.windowed = false;
        controlled.modeWritesWidth = axis != 0;
        controlled.modeWritesHeight = axis != 1;
        ExpectFailure(__LINE__);
        CHECK(controlled.mouseReads == 0);
    }
}

static void SetUnitSpace() {
    ResetWindow();
    controlled.windowed = false;
    controlled.windowWidth = 1;
    controlled.windowHeight = 1;
    controlled.renderWidth = 1;
    controlled.renderHeight = 1;
    controlled.mouseX = 0.0f;
    controlled.mouseY = 0.0f;
}

static void TestNumericFailuresAndBoundaries() {
    const float positiveInfinity = std::numeric_limits<float>::infinity();
    const float quietNan = std::numeric_limits<float>::quiet_NaN();
    for (float invalid : {0.0f, -0.0f, -1.0f, positiveInfinity, -positiveInfinity, quietNan}) {
        for (int axis = 0; axis < 2; ++axis) {
            SetUnitSpace();
            if (axis == 0) {
                controlled.aspectX = invalid;
            } else {
                controlled.aspectY = invalid;
            }
            ExpectFailure(__LINE__);
        }
    }
    for (float invalid : {positiveInfinity, -positiveInfinity, quietNan}) {
        for (int axis = 0; axis < 2; ++axis) {
            SetUnitSpace();
            if (axis == 0) {
                controlled.mouseX = invalid;
            } else {
                controlled.mouseY = invalid;
            }
            ExpectFailure(__LINE__); // Valid X plus invalid Y must not partially commit X.
        }
    }
    constexpr float signedIntegerUpperExclusive = 2147483648.0f;
    SetUnitSpace();
    controlled.mouseX = signedIntegerUpperExclusive;
    ExpectFailure(__LINE__);
    SetUnitSpace();
    controlled.mouseX = 7.0f;
    controlled.mouseY = signedIntegerUpperExclusive;
    ExpectFailure(__LINE__); // A valid first axis must remain uncommitted if the second fails.
    SetUnitSpace();
    controlled.mouseX = std::nextafter(-signedIntegerUpperExclusive, -positiveInfinity);
    ExpectFailure(__LINE__);
    SetUnitSpace();
    controlled.mouseX = std::nextafter(signedIntegerUpperExclusive, 0.0f);
    controlled.mouseY = -signedIntegerUpperExclusive;
    Output limits;
    CHECK(Query(limits));
    CHECK(limits.x == 2147483520 && limits.y == std::numeric_limits<int>::min());
    CheckCalls();
    SetUnitSpace();
    controlled.mouseX = -1000117.0f;
    controlled.aspectX = 2147.232421875f;
    Output negativeFraction;
    CHECK(Query(negativeFraction));
    CHECK(negativeFraction.x == std::numeric_limits<int>::min()); // Before truncation: -2147483648.0683594.
    CHECK(negativeFraction.x == cse::WindowToRenderCoordinate(-1000117.0f, 1, 1, 2147.232421875f));
    SetUnitSpace();
    controlled.mouseX = 1000419.0f;
    controlled.aspectX = 2146.584228515625f;
    Output positiveFraction;
    CHECK(Query(positiveFraction));
    CHECK(positiveFraction.x == std::numeric_limits<int>::max()); // Before truncation: 2147483647.307373.
    CHECK(positiveFraction.x == cse::WindowToRenderCoordinate(1000419.0f, 1, 1, 2146.584228515625f));
    SetUnitSpace();
    controlled.mouseX = std::numeric_limits<float>::max();
    controlled.renderWidth = 2;
    ExpectFailure(__LINE__); // The float multiply overflows before division or truncation.
    SetUnitSpace();
    controlled.mouseX = -signedIntegerUpperExclusive;
    controlled.windowWidth = 2;
    controlled.renderWidth = 2;
    controlled.aspectX = 0.5f;
    ExpectFailure(__LINE__); // scaled-center overflows even though aspect compression could rescue the result.
    SetUnitSpace();
    controlled.mouseX = 2.0f;
    controlled.aspectX = 1073741824.0f;
    ExpectFailure(__LINE__); // First int conversion is safe; the adjusted conversion is not.
    SetUnitSpace();
    controlled.mouseX = 0.0f;
    controlled.renderWidth = 2;
    controlled.aspectX = signedIntegerUpperExclusive;
    Output minimumDifference;
    CHECK(Query(minimumDifference));
    CHECK(minimumDifference.x == -2147483647 && minimumDifference.centerX == 1);
    CHECK(minimumDifference.x - minimumDifference.centerX == std::numeric_limits<int>::min());
    SetUnitSpace();
    controlled.mouseX = 511.0f;
    controlled.windowWidth = 1024;
    controlled.renderWidth = 1024;
    controlled.aspectX = std::nextafter(signedIntegerUpperExclusive, positiveInfinity);
    ExpectFailure(__LINE__); // Final position fits int; position-center does not.
    SetUnitSpace();
    controlled.mouseX = std::numeric_limits<float>::denorm_min();
    controlled.mouseY = -std::numeric_limits<float>::denorm_min();
    controlled.aspectX = std::numeric_limits<float>::denorm_min();
    Output subnormal;
    CHECK(Query(subnormal));
    CHECK(subnormal.x == 0 && subnormal.y == 0 && subnormal.centerX == 0 && subnormal.centerY == 0);
    SetUnitSpace();
    controlled.windowWidth = std::numeric_limits<int>::max();
    controlled.renderWidth = std::numeric_limits<int>::max();
    controlled.mouseX = 1073741824.0f;
    Output maximumExtent;
    CHECK(Query(maximumExtent));
    CHECK(maximumExtent.x == 1073741824 && maximumExtent.centerX == 1073741823);
}

static void TestControlledPrivateSampleFailures() {
#if INTPTR_MAX > INT32_MAX
    for (bool notReady : {false, true}) {
        ResetWindow();
        controlled.sampleReady = !notReady;
        controlled.sampleSucceeds = notReady;
        Output output;
        CHECK(!Query(output));
        CheckUnchanged(output);
        CheckCalls(false);
        CHECK(controlled.sampleRequests == 1);
    }
#endif
}

int main() {
    TestCoordinates();
    TestLegacyArithmeticCorpus();
    TestMissingAndExtentFailures();
    TestNumericFailuresAndBoundaries();
    TestControlledPrivateSampleFailures();
    std::printf("checks=%d actual-helper controlled-private-sample-only\n", CheckCount());
    return 0;
}
