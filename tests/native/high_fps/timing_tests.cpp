#include "client_command_history.h"
#include "crosshair_recovery.h"
#include "goldsrc_frame_timing.h"
#include "regamedll_movement.h"
#include "trigger_camera_timing.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <initializer_list>
#include <vector>

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

void TestReGameDLLMovementNegotiation() {
    cse::ReGameDLLMovement movement;
    std::vector<std::string_view> commands;
    const auto sendCommand = [&](const char* command) {
        commands.emplace_back(command);
    };

    Require(movement.ConsumeReply("cl_pmove_version 1.0\n") && !movement.PredictionEnabled(),
        "an unsolicited movement version cannot activate prediction");
    for (const char* unsupported : {"", "0.9", "1", "1.0.1.1", "1.0;echo extra", "1.0\n", "256.0"}) {
        movement.Request(unsupported, sendCommand);
        Require(commands.empty(), "a server without a compatible pmove advertisement must receive no commands");
    }
    Require(movement.ConsumeReply("cl_pmove_version 1.0\n") && !movement.PredictionEnabled(),
        "an unsupported advertisement must not start waiting for a reply");

    movement.Request("1.0", sendCommand);
    Require(commands == std::vector<std::string_view>{"cl_pmove_version 255.255.255", "cl_pmove_version 1.0"},
        "request a clamped reply and select 1.0 in order");
    Require(!movement.PredictionEnabled(), "a server without a reply must use legacy prediction");
    for (const char* malformed : {"cl_pmove_version 1.0;echo extra\n", "cl_pmove_version 1.0\nextra\n",
                                 "cl_pmove_version 256.0\n", "cl_pmove_version 1.\n"}) {
        Require(!movement.ConsumeReply(malformed) && !movement.PredictionEnabled(),
            "malformed replies must go through the regular stufftext filter");
    }
    Require(movement.ConsumeReply("cl_pmove_version 1.0\n") && movement.PredictionEnabled(),
        "a supported reply enables ReGameDLL prediction");

    movement.Reset();
    Require(!movement.PredictionEnabled(), "reset must drop the active version");
    movement.Request("1.0", sendCommand);
    Require(movement.ConsumeReply("cl_pmove_version 0.9\n") && !movement.PredictionEnabled(),
        "an older server must use legacy prediction");

    movement.Request("2.0.1", sendCommand);
    Require(movement.ConsumeReply("cl_pmove_version 2.0.1\n") && movement.PredictionEnabled(),
        "a newer server can still predict the selected 1.0");

    movement.Request("1.0", sendCommand);
    movement.Reset();
    Require(movement.ConsumeReply("cl_pmove_version 1.0\n") && !movement.PredictionEnabled(),
        "reset must discard an outstanding request");
}
}

int main() {
    TestPredictionHistory();
    TestCrosshairRecovery();
    TestStaminaAndCamera();
    TestReGameDLLMovementNegotiation();
}
