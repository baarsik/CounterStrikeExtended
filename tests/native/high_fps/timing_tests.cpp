#include "client_command_history.h"
#include "crosshair_recovery.h"
#include "goldsrc_frame_timing.h"
#include "trigger_camera_timing.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <initializer_list>

namespace {
void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "%s\n", message);
        std::exit(EXIT_FAILURE);
    }
}

void TestPredictionHistory() {
    constexpr int acknowledged = 100;
    constexpr int highFpsCommandsInFlight = 240;
    Require(CL_CanReplayCommands(acknowledged + highFpsCommandsInFlight, acknowledged),
        "240 commands in flight should fit");
    Require(CL_CommandHistoryIndex(acknowledged) != CL_CommandHistoryIndex(acknowledged + 64),
        "command history must not alias after 64 commands");
    Require(CL_CommandHistoryIndex(acknowledged) ==
            CL_CommandHistoryIndex(acknowledged + CL_COMMAND_HISTORY_SIZE),
        "the command history should wrap at its capacity");
    Require(!CL_CanReplayCommands(acknowledged + CL_COMMAND_HISTORY_SIZE - 2, acknowledged),
        "replay must stop before commands overwrite history it still needs");
}

void TestCrosshairRecovery() {
    constexpr double referenceStep = 0.01;
    float baselineDistance = 15.0f;
    double baselineAlpha = 120.0;
    RecoverCrosshair(baselineDistance, baselineAlpha, referenceStep);
    Require(std::abs(baselineDistance - (15.0 - (0.013 * 15.0 + 0.1))) < 0.000001,
        "100 FPS crosshair distance must match the legacy step");
    Require(baselineAlpha == 122.0, "100 FPS alpha must match the legacy step");

    for (int steps : {1, 2, 10}) {
        float distance = 15.0f;
        double alpha = 120.0;
        for (int i = 0; i < steps; ++i) {
            RecoverCrosshair(distance, alpha, referenceStep / steps);
        }
        Require(std::abs(distance - baselineDistance) < 0.00001, "gap should recover by elapsed time");
        Require(std::abs(alpha - baselineAlpha) < 0.00001, "alpha should retain fractional recovery");
    }
    RecoverCrosshair(baselineDistance, baselineAlpha, 0.0);
    Require(baselineAlpha == 122.0, "zero-time redraw should not advance recovery");
}

void TestStaminaAndCamera() {
    constexpr double staminaRetention = 0.85;
    Require(CseFrameRetention(staminaRetention, 0.01) == staminaRetention,
        "a 10 ms command must keep the original stamina multiplier");
    Require(CseFrameRetention(staminaRetention, 0.0) == 1.0, "zero time must not apply stamina loss");
    double retention = 1.0;
    for (int i = 0; i < 10; ++i) {
        retention *= CseFrameRetention(staminaRetention, 0.001);
    }
    Require(std::abs(retention - staminaRetention) < 0.000001,
        "splitting a 10 ms step must preserve the amount of damping");

    for (int steps : {1, 2, 10}) {
        const float step = 0.01f / steps;
        float error = 90.0f;
        for (int i = 0; i < steps; ++i) {
            error -= error * TriggerCameraTurnRate(step) * step;
        }
        Require(std::abs(error - (90.0f - 90.0f * 0.4f * 0.01f)) < 0.00003f,
            "camera turning must agree with the 100 FPS step");
    }
}
}

int main() {
    TestPredictionHistory();
    TestCrosshairRecovery();
    TestStaminaAndCamera();
}
