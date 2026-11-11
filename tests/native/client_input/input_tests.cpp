#include "controlled_services.h"
#include <cstring>
#include <limits>
#if defined(__APPLE__)
#include "sdl3_native_mouse_bridge.h"
static bool gameplayRelativeMode = false;
extern "C" std::uint32_t CSE_SDL_NATIVE_MOUSE_CALL CseSdl_QueryMouseModeV1(std::uint32_t) {
    return gameplayRelativeMode ? CseSdlMouseModeRelative : CseSdlMouseModeAbsolute;
}
#endif
#if defined(_WIN32) && defined(_WIN64)
#include "native_mouse_poller_win.h"
#include <type_traits>
#endif

TestEngine gEngfuncs;
TestHud gHUD;
int iMouseInUse = 0;
kbutton_t in_strafe{};
kbutton_t in_mlook{};
kbutton_t in_speed{};
kbutton_t in_jlook{};
cvar_t *m_pitch;
cvar_t *m_yaw;
cvar_t *m_forward;
cvar_t *m_side;
cvar_t *lookstrafe;
cvar_t *lookspring;
cvar_t *cl_pitchdown;
cvar_t *cl_pitchup;
cvar_t *cl_yawspeed;
cvar_t *cl_sidespeed;
cvar_t *cl_forwardspeed;
cvar_t *cl_pitchspeed;
cvar_t *cl_movespeedkey;

#if CSE_GAME_CS
#include "cs_state.inc"
#else
#include "hl_state.inc"
#endif

globalvars_t fixtureGlobals{};
globalvars_t *gpGlobals = &fixtureGlobals;

void V_StopPitchDrift() {
    ++services.stopPitchDrift;
}

void IN_StartupJoystick() {
    ++services.startupJoystick;
}

void IN_JoyMove(float, usercmd_t *) {
    ++services.joystickMoves;
}

void Joy_AdvancedUpdate_f() {
    CHECK(false);
}

#if defined(_WIN32) && !defined(_WIN64)
long s_mouseDeltaX = 0;
long s_mouseDeltaY = 0;
POINT old_mouse_pos{};

long ThreadInterlockedExchange(long *destination, long value) {
    return InterlockedExchange(destination, value);
}

DWORD WINAPI MousePos_ThreadFunction(LPVOID) {
    CHECK(false);
    return 0;
}
#endif

#define CVAR_GET_FLOAT GetCvarFloat
#if defined(_MSC_VER)
#pragma warning(push)
// Retained mouse math deliberately converts integer motion and double 0.5 to
// float. Keep those actual expressions, with the same rounding points.
#pragma warning(disable : 4244 4305)
#endif
#if CSE_GAME_CS
#include "cs_functions.inc"
#else
#include "hl_functions.inc"
#endif
#if defined(_MSC_VER)
#pragma warning(pop)
#endif
#undef CVAR_GET_FLOAT

static void SetVisible(bool visible) {
#if CSE_GAME_CS
    services.cursorVisible = visible;
#else
    g_iVisibleMouse = visible ? 1 : 0;
#endif
}

static void Reset(float raw = 1.0f) {
    ResetServices();
    fixtureGlobals = globalvars_t{};
    gpGlobals = &fixtureGlobals;
    gHUD = TestHud{};
    gHUD.default_fov = Value("default_fov", 90.0f);
    Value("zoom_sensitivity_ratio", 1.0f);
    Value("m_rawinput", raw);
    m_pitch = Value("m_pitch", 0.25f);
    m_yaw = Value("m_yaw", 0.5f);
    m_forward = Value("m_forward", 3.0f);
    m_side = Value("m_side", 2.0f);
    lookstrafe = Value("lookstrafe", 0.0f);
    lookspring = Value("lookspring", 0.0f);
    cl_pitchdown = Value("cl_pitchdown", 80.0f);
    cl_pitchup = Value("cl_pitchup", 70.0f);
    cl_yawspeed = Value("cl_yawspeed", 210.0f);
    cl_sidespeed = Value("cl_sidespeed", 400.0f);
    cl_forwardspeed = Value("cl_forwardspeed", 400.0f);
    cl_pitchspeed = Value("cl_pitchspeed", 225.0f);
    cl_movespeedkey = Value("cl_movespeedkey", 0.3f);
    iMouseInUse = 0;
    g_iVisibleMouse = 0;
    in_strafe = kbutton_t{};
    in_mlook = kbutton_t{};
    in_speed = kbutton_t{};
    in_jlook = kbutton_t{};
    mouseinitialized = 0;
    mouseactive = 0;
    mouse_buttons = 0;
    mouse_oldbuttonstate = 0;
    old_mouse_x = 0;
    old_mouse_y = 0;
    mx_accum = 0;
    my_accum = 0;
    mouse_x = 0.0f;
    mouse_y = 0.0f;
    s_relativeMotion.Reset();
    s_pJoystick = nullptr;
    joy_avail = 0;
#if CSE_GAME_CS
    m_bRawInput = false;
    s_flRawInputUpdateTime = 0.0;
#else
    m_rawinput = nullptr;
#endif
#if defined(_WIN32)
    restore_spi = 0;
    mouseparmsvalid = 0;
    m_bMouseThread = false;
#if !defined(_WIN64)
    s_hMouseThread = nullptr;
    s_hMouseQuitEvent = nullptr;
    s_hMouseDoneQuitEvent = nullptr;
    s_mouseDeltaX = 0;
    s_mouseDeltaY = 0;
#endif
#endif
    IN_Init();
    CHECK(mouseinitialized == 1 && mouse_buttons == MOUSE_BUTTON_COUNT);
    CHECK(services.startupJoystick == 1);
    CHECK(services.commands.size() == 2);
    CHECK(services.commands[0] == "force_centerview" && services.commands[1] == "joyadvancedupdate");
    CHECK((sensitivity->flags & FCVAR_FILTERSTUFFTEXT) != 0);
    IN_ActivateMouse();
    CHECK(mouseactive == 1);
    sensitivity->value = 1.0f;
}

static usercmd_t Move(float x, float y) {
    usercmd_t command{};
    QueueMotion(x, y);
    IN_Move(0.01f, &command);
    return command;
}

static void TestRawTiming() {
#if CSE_GAME_CS
    Reset(-1.0f);
    CHECK(!m_bRawInput && services.rawReads == 1);
    fixtureGlobals.time = 1.0f;
    IN_ResetMouse();
    CHECK(!m_bRawInput && services.rawReads == 1 && s_flRawInputUpdateTime == 0.0);
    fixtureGlobals.time = std::nextafter(1.0f, 2.0f);
    IN_ResetMouse();
    CHECK(m_bRawInput && services.rawReads == 2);
    const double refreshed = s_flRawInputUpdateTime;
    CHECK(refreshed == fixtureGlobals.time);
    Value("m_rawinput", 0.0f);
    fixtureGlobals.time = 2.0f;
    IN_ResetMouse();
    CHECK(m_bRawInput && services.rawReads == 2 && s_flRawInputUpdateTime == refreshed);
    fixtureGlobals.time = 3.0f;
    IN_ResetMouse();
    CHECK(!m_bRawInput && services.rawReads == 3 && s_flRawInputUpdateTime == 3.0);
    Value("m_rawinput", 1.0f);
    fixtureGlobals.time = 4.0f;
    IN_ResetMouse();
    CHECK(!m_bRawInput && services.rawReads == 3);
    fixtureGlobals.time = 4.5f;
    gpGlobals = nullptr;
    IN_ResetMouse();
    CHECK(!m_bRawInput && services.rawReads == 3);
    gpGlobals = &fixtureGlobals;
    IN_ResetMouse();
    CHECK(m_bRawInput && services.rawReads == 4);
    fixtureGlobals.time = 0.0f;
    Value("m_rawinput", 0.0f);
    IN_ResetMouse();
    CHECK(m_bRawInput && services.rawReads == 4);
#else
    Reset(0.0f);
    CHECK(!IN_UseRawInput());
    Value("m_rawinput", -1.0f);
    CHECK(IN_UseRawInput());
    Value("m_rawinput", 0.0f);
    CHECK(!IN_UseRawInput());
    m_rawinput = nullptr;
    CHECK(!IN_UseRawInput());
#endif
}

static void TestRelativeAndFilter() {
    Reset();
    in_mlook.state = 1;
    Move(0.25f, -0.25f);
    CHECK(services.angles[YAW] == 0.0f && services.angles[PITCH] == 0.0f);
    Move(0.75f, -0.75f);
    CHECK(services.angles[YAW] == -0.5f && services.angles[PITCH] == -0.25f);
    QueueMotion(2.0f, 4.0f);
    IN_Accumulate();
    CHECK(mx_accum == 2 && my_accum == 4);
    Move(2.0f, 4.0f);
    CHECK(mx_accum == 0 && my_accum == 0);
    CHECK(services.angles[YAW] == -2.5f && services.angles[PITCH] == 1.75f);
    m_filter->value = 1.0f;
    Move(0.0f, 0.0f);
    CHECK(mouse_x == 2.0f && mouse_y == 4.0f);
    CHECK(services.angles[YAW] == -3.5f && services.angles[PITCH] == 2.75f);
    Move(0.0f, 0.0f);
    CHECK(mouse_x == 0.0f && mouse_y == 0.0f);
    CHECK(services.relativeReads == 6);
    Reset();
    Move(0.75f, 0.0f);
#if CSE_GAME_CS
    Value("m_rawinput", 0.0f);
    fixtureGlobals.time = 1.0f;
    IN_ResetMouse();
    Move(0.25f, 0.0f);
    CHECK(old_mouse_x == 1); // Exact one second does not reset the CS remainder.
    Move(0.75f, 0.0f);
    fixtureGlobals.time = 2.0f;
    IN_ResetMouse();
#else
    Value("m_rawinput", 0.0f);
#endif
    Move(0.25f, 0.0f);
    CHECK(old_mouse_x == 0); // Actual raw-state transition resets the remainder.
}

static void TestMovementBranches() {
    Reset();
    in_mlook.state = 1;
    usercmd_t command = Move(4.0f, 8.0f);
    CHECK(services.angles[YAW] == -2.0f && services.angles[PITCH] == 2.0f);
    CHECK(command.forwardmove == 0.0f && command.sidemove == 0.0f && command.upmove == 0.0f);
    CHECK(services.stopPitchDrift == 1);
    services.angles[PITCH] = 79.0f;
    Move(0.0f, 8.0f);
    CHECK(services.angles[PITCH] == 80.0f);
    services.angles[PITCH] = -69.0f;
    Move(0.0f, -8.0f);
    CHECK(services.angles[PITCH] == -70.0f);
    Reset();
    in_strafe.state = 1;
    command = Move(4.0f, 8.0f);
    CHECK(command.sidemove == 8.0f && command.forwardmove == -24.0f && command.upmove == 0.0f);
    CHECK(services.angles[YAW] == 0.0f);
    services.noClipping = true;
    command = Move(4.0f, 8.0f);
    CHECK(command.sidemove == 8.0f && command.upmove == -24.0f && command.forwardmove == 0.0f);
    Reset();
    in_mlook.state = 1;
    lookstrafe->value = 1.0f;
    command = Move(4.0f, 8.0f);
    CHECK(command.sidemove == 8.0f && services.angles[YAW] == 0.0f && services.angles[PITCH] == 2.0f);
    Force_CenterView_f();
    CHECK(services.angles[PITCH] == 0.0f);
    iMouseInUse = 1;
    services.angles[PITCH] = 9.0f;
    Force_CenterView_f();
    CHECK(services.angles[PITCH] == 9.0f);
}

static void TestSuppressionAndButtons() {
    Reset();
    in_mlook.state = 1;
    iMouseInUse = 1;
    Move(3.0f, 4.0f);
    CHECK(services.relativeReads == 0 && services.joystickMoves == 1 && services.stopPitchDrift == 0);
    IN_Accumulate();
    CHECK(mx_accum == 0 && services.relativeReads == 0);
    iMouseInUse = 0;
    SetVisible(true);
    Move(0.0f, 0.0f);
    CHECK(services.relativeReads == 0 && services.stopPitchDrift == 1 && services.setAngles == 1);
    IN_Accumulate();
    CHECK(services.relativeReads == 0);
    IN_MouseEvent(31);
    CHECK(services.keys.empty() && mouse_oldbuttonstate == 0);
    SetVisible(false);
    gHUD.m_iIntermission = 1;
    Move(0.0f, 0.0f);
    CHECK(services.relativeReads == 0 && services.stopPitchDrift == 2);
    gHUD.m_iIntermission = 0;
    Move(0.0f, 0.0f);
    CHECK(old_mouse_x == 3 && old_mouse_y == 4);
    IN_MouseEvent(21);
    CHECK(services.keys.size() == 3);
    CHECK(services.keys[0] == std::make_pair(K_MOUSE1, 1));
    CHECK(services.keys[1] == std::make_pair(K_MOUSE1 + 2, 1));
    CHECK(services.keys[2] == std::make_pair(K_MOUSE1 + 4, 1));
    IN_MouseEvent(10);
    CHECK(services.keys.size() == 8);
    for (int button = 0; button < MOUSE_BUTTON_COUNT; ++button) {
        CHECK(services.keys[3 + button] == std::make_pair(K_MOUSE1 + button, button % 2));
    }
    iMouseInUse = 1;
    IN_MouseEvent(0);
    CHECK(mouse_oldbuttonstate == 10 && services.keys.size() == 8);
}

static void TestLifecycleAndClear() {
    Reset();
    Move(0.75f, 0.5f);
    mx_accum = 8;
    my_accum = 9;
    old_mouse_x = 10;
    old_mouse_y = 11;
    mouse_oldbuttonstate = 31;
    IN_ClearStates();
    CHECK(mx_accum == 0 && my_accum == 0 && mouse_oldbuttonstate == 0);
    CHECK(old_mouse_x == 10 && old_mouse_y == 11);
    Move(0.25f, 0.5f);
    CHECK(old_mouse_x == 0 && old_mouse_y == 0);
    IN_DeactivateMouse();
    CHECK(mouseactive == 0);
    mx_accum = 8;
    my_accum = 9;
    mouse_oldbuttonstate = 31;
    IN_ClearStates();
    CHECK(mx_accum == 8 && my_accum == 9 && mouse_oldbuttonstate == 31);
    s_relativeMotion.TakeX(0.75f);
    IN_ClearStates();
    CHECK(s_relativeMotion.TakeX(0.25f) == 0);
    const int reads = services.relativeReads;
    Move(2.0f, 4.0f);
    IN_Accumulate();
    CHECK(services.relativeReads == reads);
    CHECK(services.joystickMoves == 3);
    IN_ActivateMouse();
    CHECK(mouseactive == 1);
    s_pJoystick = reinterpret_cast<SDL_Gamepad *>(static_cast<std::uintptr_t>(0x12345678));
    joy_avail = 1;
    IN_Shutdown();
    CHECK(s_pJoystick == nullptr && joy_avail == 0 && mouseactive == 0 && services.joystickCloses == 1);
    IN_Shutdown();
    CHECK(services.joystickCloses == 1);
    mouseinitialized = 0;
    IN_ActivateMouse();
    CHECK(mouseactive == 0);
    services.parameters.insert("-nomouse");
    mouse_buttons = 0;
    IN_StartupMouse();
    CHECK(mouseinitialized == 0 && mouse_buttons == 0);
    struct GuardedPosition {
        std::uint64_t before = 0xfedcba9876543210ULL;
        int x = 0;
        int y = 0;
        std::uint64_t after = 0x123456789abcdef0ULL;
    };
    auto *position = new GuardedPosition;
    if (sizeof(void *) == 8) {
        CHECK(reinterpret_cast<std::uintptr_t>(position) > std::numeric_limits<std::uint32_t>::max());
    }
    IN_GetMousePos(&position->x, &position->y);
    CHECK(position->x == services.cursorX && position->y == services.cursorY);
    CHECK(position->before == 0xfedcba9876543210ULL && position->after == 0x123456789abcdef0ULL);
    delete position;
}

static void TestExistingRelativeReset() {
#if !CSE_GAME_CS
    Reset(0.0f);
    QueueMotion(2.0f, 4.0f);
    s_relativeMotion.TakeX(0.75f);
    IN_ResetRelativeMouseState();
    CHECK(services.relativeReads == 0 && services.eventPumps == 0 && services.relativeX == 2.0f);
    CHECK(s_relativeMotion.TakeX(0.25f) == 0);
    Value("m_rawinput", 1.0f);
    IN_ResetRelativeMouseState();
    CHECK(services.relativeReads == 1 && services.eventPumps == 1);
    CHECK(services.relativeX == 0.0f && services.relativeY == 0.0f);
#endif
}

static void TestScaling() {
    Reset();
    sensitivity->value = 3.0f;
    float x = 3.0f;
    float y = 4.0f;
    IN_ScaleMouse(&x, &y);
    CHECK(x == 9.0f && y == 12.0f);
    m_customaccel->value = 1.0f;
    m_customaccel_scale->value = 0.5f;
    m_customaccel_exponent->value = 1.0f;
    gHUD.m_iFOV = 45;
    x = 3.0f;
    y = 4.0f;
    IN_ScaleMouse(&x, &y);
#if CSE_GAME_CS
    CHECK(x == 8.25f && y == 11.0f);
#else
    CHECK(x == 16.5f && y == 22.0f);
#endif
    m_customaccel_max->value = 4.0f;
    x = 3.0f;
    y = 4.0f;
    IN_ScaleMouse(&x, &y);
#if CSE_GAME_CS
    CHECK(x == 6.0f && y == 8.0f);
#else
    CHECK(x == 12.0f && y == 16.0f);
#endif
    m_customaccel->value = 2.0f;
    x = 3.0f;
    y = 4.0f;
    IN_ScaleMouse(&x, &y);
#if CSE_GAME_CS
    CHECK(x == 3.0f && y == 2.0f);
#else
    CHECK(x == 6.0f && y == 4.0f);
#endif
    m_customaccel->value = 0.0f;
    sensitivity->value = 20000.0f;
    x = 1.0f;
    y = 2.0f;
    IN_ScaleMouse(&x, &y);
#if CSE_GAME_CS
    CHECK(x == 20000.0f && y == 40000.0f && services.sensitivityWrites == 0);
#else
    CHECK(x == 10000.0f && y == 20000.0f && services.sensitivityWrites == 1);
#endif
    const int writes = services.sensitivityWrites;
    sensitivity->value = -1.0f;
    gHUD.sensitivity = 1.5f;
    x = 2.0f;
    y = 4.0f;
    IN_ScaleMouse(&x, &y);
    CHECK(x == 3.0f && y == 6.0f && services.sensitivityWrites == writes);
}

#if UINTPTR_MAX > UINT32_MAX
static void TestCameraHandoff() {
    Reset();
    CHECK(s_relativeMotion.TakeX(0.75f) == 0);
    CHECK(s_relativeMotion.TakeY(-0.75f) == 0);
    mx_accum = 13;
    my_accum = -7;
    old_mouse_x = 23;
    old_mouse_y = -29;
    mouse_oldbuttonstate = 5;
    QueueMotion(100.0f, -80.0f);
    const int reads = services.relativeReads;
    const int pumps = services.eventPumps;
    iMouseInUse = 1;
    IN_SetCameraMouseOwnership(true);
    CHECK(iMouseInUse == 1 && mx_accum == 0 && my_accum == 0);
    CHECK(old_mouse_x == 23 && old_mouse_y == -29 && mouse_oldbuttonstate == 5);
    CHECK(services.relativeReads == reads + 1 && services.eventPumps == pumps);
    CHECK(services.relativeX == 0.0f && services.relativeY == 0.0f);
    CHECK(s_relativeMotion.TakeX(0.5f) == 0 && s_relativeMotion.TakeY(-0.5f) == 0);
    QueueMotion(60.0f, 70.0f);
    IN_SetCameraMouseOwnership(true); // Nested intent remains owned.
    CHECK(services.relativeReads == reads + 1 && services.relativeX == 60.0f);
    IN_DeactivateMouse();
    IN_ClearStates();
    IN_ActivateMouse();
    CHECK(old_mouse_x == 23 && old_mouse_y == -29 && mouse_oldbuttonstate == 5);
    iMouseInUse = 0;
    IN_SetCameraMouseOwnership(false);
    CHECK(services.relativeReads == reads + 2 && services.relativeX == 0.0f);
    CHECK(services.eventPumps == pumps && old_mouse_x == 23 && old_mouse_y == -29);
    CHECK(s_relativeMotion.TakeX(0.5f) == 0 && s_relativeMotion.TakeY(-0.5f) == 0);
    IN_SetCameraMouseOwnership(false);
    CHECK(services.relativeReads == reads + 2);
    s_relativeMotion.Reset();
    const auto command = Move(1.0f, 2.0f);
    CHECK(command.forwardmove == -6.0f && services.angles[YAW] == -0.5f);
}
#endif


#if defined(__APPLE__)
namespace gameplay_loop {
cvar_t m_rawinput{};
cvar_t cl_mousegrab{};
bool s_bCursorVisible = false;
struct Surface {
    bool IsCursorVisible() const {
        return services.cursorVisible;
    }
} g_BaseUISurface;
int BUsesSDLInput() {
    return 1;
}
void ClientDLL_IN_Accumulate() {
    IN_Accumulate();
}
struct Game {
    bool m_bActiveApp = true;
    bool m_bCursorVisible = false;
    bool m_bExpectSyntheticMouseMotion = false;
    int m_nMouseTargetX = 320;
    int m_nMouseTargetY = 240;
    int m_nWarpDelta = 40;
    SDL_Window *m_hSDLWindow = nullptr;
    void Motion(float x, float y) {
        SDL_Event ev{};
        ev.type = SDL_EVENT_MOUSE_MOTION;
        ev.motion.x = x;
        ev.motion.y = y;
        do {
#include "engine_motion.inc"
        } while (false);
    }
};
void DuringPump() {
    QueueMotion(17.75f, -4.75f);
}
void TestAcceptedMotion() {
    for (bool captured : {false, true}) {
        for (bool duringWarp : {false, true}) {
            gameplayRelativeMode = captured;
            for (int raw = 0; raw < 2; ++raw) {
                for (int grab = 0; grab < 2; ++grab) {
                    Reset(static_cast<float>(raw));
                    m_rawinput.value = static_cast<float>(raw);
                    cl_mousegrab.value = static_cast<float>(grab);
                    Game game;
                    QueueMotion(80.5f, -12.5f);
                    if (duringWarp) {
                        services.onWarp = DuringPump;
                    } else {
                        services.onPump = DuringPump;
                    }
                    game.Motion(400.5f, 227.5f);
#if defined(__APPLE__)
                    if (captured) {
                        CHECK(services.windowWarps == 0);
                    }
#endif
                    // Motion at the next input opportunity when this transport
                    // does not need the controlled warp/pump boundary.
                    if (services.onPump || services.onWarp) {
                        services.onPump = nullptr;
                        services.onWarp = nullptr;
                        DuringPump();
                    }
                    Move(0.0f, 0.0f);
                    std::printf("gameplay game=%s raw=%d grab=%d captured=%d during-warp=%d "
                                "accepted=98.25,-17.25 yaw=%.2f warps=%d reads=%d\n",
                                CSE_GAME_CS ? "CS" : "HL", raw, grab, captured, duringWarp,
                                services.angles[YAW], services.windowWarps, services.relativeReads);
                    CHECK(services.angles[YAW] == -49.0f);
                    Move(0.75f, 0.0f);
                    CHECK(services.angles[YAW] == -49.5f);
                }
            }
        }
    }
}
}
#endif

int main() {
#if defined(__APPLE__)
    gameplay_loop::TestAcceptedMotion();
#endif
    TestRawTiming();
    TestRelativeAndFilter();
    TestMovementBranches();
    TestSuppressionAndButtons();
    TestLifecycleAndClear();
    TestExistingRelativeReset();
    TestScaling();
#if UINTPTR_MAX > UINT32_MAX
    TestCameraHandoff();
#endif
#if CSE_TEST_NATIVE_BRANCH
    CHECK(services.desktopWarps == 0 && services.spiReads == 0 && services.spiWrites == 0);
#endif
    std::printf("game=%s native-branch=%d checks=%d fake-services-only\n", CSE_GAME_CS ? "cs" : "hl",
        CSE_TEST_NATIVE_BRANCH, Checks());
    return 0;
}
