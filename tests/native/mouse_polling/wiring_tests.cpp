// The actual complete input bodies/state and pinned SDK records are shared with
// the retained caller fixture. Only surrounding engine/SDL/Win32 input is fake.
#define main RetainedInputFixtureMain
#include "../client_input/input_tests.cpp"
#undef main
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>

static std::mutex cursorMutex;
static std::condition_variable cursorCondition;
static cse::mouse::Point fakeCursor{};
static bool holdRead = false;
static bool readEntered = false;
static std::atomic<int> cursorReads{0};
static std::atomic<int> diagnostics{0};
static std::atomic<bool> failCursor{false};
static std::atomic<bool> poisonFailedRead{false};
static std::atomic<int> ownerWaits{0};

BOOL OwnedSleepCondition(PCONDITION_VARIABLE condition, PSRWLOCK lock, DWORD timeout, ULONG flags) {
    ++ownerWaits; // Witness actual generation retirement before releasing the held query.
    return FixtureSleepCondition(condition, lock, timeout, flags);
}

BOOL OwnedGetCursorPos(POINT *point) {
    std::unique_lock<std::mutex> lock(cursorMutex);
    ++cursorReads;
    if (failCursor.exchange(false)) {
        if (poisonFailedRead) {
            point->x = 91;
            point->y = 87;
        }
        lock.unlock();
        SetLastError(4321);
        return FALSE;
    }
    const auto snapshot = fakeCursor;
    if (holdRead) {
        readEntered = true;
        cursorCondition.notify_all();
        cursorCondition.wait(lock, [] { return !holdRead; });
    }
    point->x = snapshot.x;
    point->y = snapshot.y;
    return TRUE;
}

BOOL OwnedSetCursorPos(int x, int y) {
    std::lock_guard<std::mutex> guard(cursorMutex);
    ++services.desktopWarps;
    fakeCursor = {x, y};
    return TRUE;
}

BOOL OwnedSystemParametersInfo(UINT action, UINT, void *data, UINT) {
    if (action == SPI_GETMOUSE) {
        ++services.spiReads;
        int *values = static_cast<int *>(data);
        values[0] = 0;
        values[1] = 0;
        values[2] = 1;
    } else {
        ++services.spiWrites;
    }
    return TRUE;
}

void PrintDiagnostic(const char *, ...) {
    ++diagnostics;
}

template<class Predicate>
static void Await(Predicate predicate) {
    const auto start = std::chrono::steady_clock::now();
    while (!predicate() && std::chrono::steady_clock::now() - start < std::chrono::seconds(2)) {
        std::this_thread::yield();
    }
    CHECK(predicate());
}

static void SetCursor(int x, int y) {
    std::lock_guard<std::mutex> guard(cursorMutex);
    fakeCursor = {x, y};
}

static void HoldQuery() {
    std::lock_guard<std::mutex> guard(cursorMutex);
    holdRead = true;
    readEntered = false;
}

static void AwaitQuery() {
    std::unique_lock<std::mutex> lock(cursorMutex);
    CHECK(cursorCondition.wait_for(lock, std::chrono::seconds(2), [] { return readEntered; }));
}

static void ReleaseQuery() {
    std::lock_guard<std::mutex> guard(cursorMutex);
    holdRead = false;
    cursorCondition.notify_all();
}

static void StartThreaded(float raw = 0.0f) {
    Reset(raw);
    IN_DeactivateMouse();
    services.parameters.insert("-mousethread");
    SetCursor(320, 240);
    IN_Init();
    CHECK(s_mousePollerUsable && s_mousePollerOpen);
    IN_ActivateMouse();
    m_mousethread_sleep->value = 1.0f;
    IN_PublishMousePolling();
}

static void AwaitMovement(int x, int y) {
    cse::mouse::Delta total{};
    Await([&] {
        cse::mouse::Delta delta{};
        CHECK(s_mousePoller.Consume(delta));
        total.x += delta.x;
        total.y += delta.y;
        return total.x == x && total.y == y;
    });
}

static void TestCameraAndRecenter() {
    StartThreaded();
    SetCursor(325, 247);
    AwaitMovement(5, 7);
    HoldQuery();
    AwaitQuery();
    SetCursor(329, 253);
    std::atomic<bool> completed{false};
    int waitCount = ownerWaits;
    // The owner thread blocks finitely until the held old-generation query exits.
    std::thread owner([&] {
        IN_ResetMouse();
        completed = true;
    });
    Await([&] { return ownerWaits > waitCount; });
    CHECK(!completed);
    ReleaseQuery();
    owner.join();
    CHECK(completed);
    AwaitMovement(4, 6); // Pre-warp unpolled motion is preserved once.
    HoldQuery();
    AwaitQuery();
    old_mouse_x = 13;
    old_mouse_y = -17;
    mouse_oldbuttonstate = 5;
    mx_accum = 3;
    my_accum = 4;
    s_relativeMotion.TakeX(0.75f);
    QueueMotion(50.0f, 60.0f);
    iMouseInUse = 1; // Root camera publishes combined flags before the hook.
    completed = false;
    waitCount = ownerWaits;
    std::thread camera([&] {
        IN_SetCameraMouseOwnership(true);
        completed = true;
    });
    Await([&] { return ownerWaits > waitCount; });
    CHECK(!completed);
    SetCursor(900, 800);
    ReleaseQuery();
    camera.join();
    CHECK(mx_accum == 0 && my_accum == 0);
    CHECK(old_mouse_x == 13 && old_mouse_y == -17 && mouse_oldbuttonstate == 5);
    CHECK(s_relativeMotion.TakeX(0.5f) == 0 && services.relativeX == 0.0f);
    const int reads = cursorReads;
    const int drains = services.relativeReads;
    const int pumps = services.eventPumps;
    IN_SetCameraMouseOwnership(true);
    usercmd_t command{};
    IN_Move(0.01f, &command);
    IN_Accumulate();
    IN_MouseEvent(0);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    CHECK(cursorReads == reads && services.relativeReads == drains && services.eventPumps == pumps);
    IN_DeactivateMouse();
    IN_ActivateMouse();
    CHECK(cursorReads == reads);
    SetCursor(320, 240); // Camera's final owned recenter occurs before release.
    iMouseInUse = 0;
    IN_SetCameraMouseOwnership(false);
    AwaitMovement(0, 0);
    SetCursor(322, 243);
    AwaitMovement(2, 3);
    IN_Shutdown();
    IN_Shutdown();
    CHECK(!s_mousePollerOpen && !s_mousePollerUsable);
}

static void TestSnapshotsAndClear() {
    StartThreaded(1.0f); // A requested worker exists but never polls while raw.
    const int reads = cursorReads;
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    CHECK(cursorReads == reads);
    Value("m_rawinput", 0.0f);
#if CSE_GAME_CS
    fixtureGlobals.time = 1.0f;
    IN_ResetMouse();
    CHECK(m_bRawInput); // Strict >1 second, not >=.
    fixtureGlobals.time = 1.001f;
    IN_ResetMouse();
    CHECK(!m_bRawInput);
#else
    IN_PublishMousePolling();
#endif
    SetCursor(323, 244);
    AwaitMovement(3, 4);
    gHUD.m_iIntermission = 1;
    usercmd_t command{};
    IN_Move(0.01f, &command);
    const int parked = cursorReads;
    SetCursor(700, 800);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    CHECK(cursorReads == parked);
    gHUD.m_iIntermission = 0;
    IN_PublishMousePolling();
    AwaitMovement(0, 0);
    SetCursor(704, 805);
    // Queue a legitimate batch, then the retained ClearStates retires it.
    HoldQuery();
    AwaitQuery();
    const int waitCount = ownerWaits;
    std::thread owner([] { IN_ClearStates(); });
    Await([&] { return ownerWaits > waitCount; });
    ReleaseQuery();
    owner.join();
    AwaitMovement(0, 0);
    old_mouse_x = 31;
    old_mouse_y = 37;
    mouse_oldbuttonstate = 3;
    IN_DeactivateMouse();
    mx_accum = 8;
    my_accum = 9;
    IN_ClearStates();
    CHECK(mx_accum == 8 && my_accum == 9 && mouse_oldbuttonstate == 3);
    CHECK(old_mouse_x == 31 && old_mouse_y == 37);
    IN_Shutdown();
    CHECK(diagnostics == 0);
}

static void TestVisibilityAndFallback() {
    StartThreaded();
    SetVisible(true);
    mouse_oldbuttonstate = 3;
    IN_MouseEvent(0);
    const int parked = cursorReads;
    SetCursor(700, 800);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    CHECK(cursorReads == parked && mouse_oldbuttonstate == 3);
    SetVisible(false);
    IN_PublishMousePolling();
    AwaitMovement(0, 0);
    SetCursor(702, 803);
    AwaitMovement(2, 3);
    IN_DeactivateMouse();
    IN_ActivateMouse();
    failCursor = true;
    Await([&] { return !s_mousePoller.Ready(); });
    CHECK(s_mousePoller.FirstFailure().kind == cse::mouse::FailureKind::ReadCursor);
    CHECK(s_mousePoller.FirstFailure().error == 4321);
    IN_PublishMousePolling();
    CHECK(!s_mousePollerOpen && !s_mousePollerUsable && diagnostics == 1);
    IN_PublishMousePolling();
    CHECK(diagnostics == 1);
    SetCursor(325, 247);
    mx_accum = 0;
    my_accum = 0;
    sensitivity->value = 1.0f;
    usercmd_t command{};
    IN_Move(0.01f, &command);
    CHECK(old_mouse_x == 5 && old_mouse_y == 7); // Actual direct nonraw fallback.
    IN_Shutdown();
    Reset(0.0f);
    CHECK(!m_bMouseThread && !s_mousePollerUsable && !s_mousePollerOpen);
    SetCursor(323, 244);
    IN_Move(0.01f, &command);
    CHECK(old_mouse_x == 3 && old_mouse_y == 4);
    IN_Shutdown();
    StartThreaded();
    m_mousethread_sleep->value = std::numeric_limits<float>::quiet_NaN();
    CHECK(!IN_PublishMousePolling() && !s_mousePollerOpen && !s_mousePollerUsable);
    CHECK(diagnostics == 2);
    IN_Shutdown();
}

static bool TestDirectFailure(bool fallback, bool accumulate, bool poison) {
    if (fallback) {
        StartThreaded();
        SetCursor(325, 247);
        IN_Accumulate();
        usercmd_t warm{};
        IN_MouseMove(0.01f, &warm);
        CHECK(current_pos.x == 5 && current_pos.y == 7);
        failCursor = true;
        Await([] { return !s_mousePoller.Ready(); });
    } else {
        Reset(0.0f);
        current_pos.x = 350;
        current_pos.y = 260;
    }
    sensitivity->value = 1.0f;
    m_filter->value = 0.0f;
    services.angles[PITCH] = 0.0f;
    services.angles[YAW] = 0.0f;
    mx_accum = 7;
    my_accum = -5;
    old_mouse_x = 11;
    old_mouse_y = -13;
    mouse_x = 2.25f;
    mouse_y = -4.5f;
    mouse_oldbuttonstate = 3;
    const POINT previous = current_pos;
    const int warps = services.desktopWarps;
    const int setters = services.setAngles;
    poisonFailedRead = poison;
    failCursor = true;
    usercmd_t command{};
    command.forwardmove = 100.0f;
    command.sidemove = 20.0f;
    command.upmove = 30.0f;
    if (accumulate) {
        IN_Accumulate();
    } else {
        IN_MouseMove(0.01f, &command);
    }
    const bool safe = mx_accum == 7 && my_accum == -5 && old_mouse_x == 11 && old_mouse_y == -13 &&
        mouse_x == 2.25f && mouse_y == -4.5f && mouse_oldbuttonstate == 3 && current_pos.x == previous.x &&
        current_pos.y == previous.y && command.forwardmove == 100.0f && command.sidemove == 20.0f &&
        command.upmove == 30.0f && services.angles[PITCH] == 0.0f && services.angles[YAW] == 0.0f &&
        services.desktopWarps == warps && services.setAngles == setters + (accumulate ? 0 : 1);
    if (!safe) {
        std::fprintf(stderr, "direct-read-safety: fallback=%d accumulate=%d poison=%d accum=%d,%d history=%d,%d\n",
            fallback, accumulate, poison, mx_accum, my_accum, old_mouse_x, old_mouse_y);
        IN_Shutdown();
        return false;
    }
    SetCursor(320, 240);
    usercmd_t retry{};
    IN_MouseMove(0.01f, &retry);
    CHECK(mx_accum == 0 && my_accum == 0 && old_mouse_x == 7 && old_mouse_y == -5);
    CHECK(retry.forwardmove == 15.0f && services.angles[YAW] == -3.5f);
    usercmd_t next{};
    IN_MouseMove(0.01f, &next);
    CHECK(old_mouse_x == 0 && old_mouse_y == 0 && next.forwardmove == 0.0f);
    CHECK(mouse_oldbuttonstate == 3);
    IN_Shutdown();
    return true;
}

static void TestFailedReadRawRecovery(bool accumulate) {
#if CSE_GAME_CS
    Reset(0.0f);
    sensitivity->value = 1.0f;
    current_pos.x = 350;
    current_pos.y = 260;
    mx_accum = 7;
    my_accum = -5;
    Value("m_rawinput", -1.0f); // Startup >0, subsequent refresh !=0 remain distinct.
    const int reads = services.rawReads;
    const int warps = services.desktopWarps;
    poisonFailedRead = true;
    for (float time : {1.0f, 1.001f}) {
        fixtureGlobals.time = time;
        failCursor = true;
        usercmd_t command{};
        if (accumulate) {
            IN_Accumulate();
        } else {
            IN_MouseMove(0.01f, &command);
        }
        CHECK(mx_accum == 7 && my_accum == -5 && services.desktopWarps == warps);
        CHECK(command.forwardmove == 0.0f && current_pos.x == 350 && current_pos.y == 260);
        CHECK(m_bRawInput == (time > 1.0f));
        CHECK(services.rawReads == reads + (time > 1.0f ? 1 : 0));
    }
    CHECK(s_flRawInputUpdateTime == static_cast<double>(fixtureGlobals.time));
    const int cursorBefore = cursorReads;
    QueueMotion(2.0f, 3.0f);
    usercmd_t raw{};
    IN_MouseMove(0.01f, &raw);
    CHECK(cursorReads == cursorBefore && old_mouse_x == 9 && old_mouse_y == -2);
    CHECK(mx_accum == 0 && my_accum == 0 && raw.forwardmove == 6.0f);
    CHECK(services.desktopWarps == warps);
    IN_Shutdown();
#else
    (void)accumulate;
#endif
}

static void TestNormalRawRefreshOrder() {
#if CSE_GAME_CS
    Reset(0.0f);
    fixtureGlobals.time = 1.001f;
    Value("m_rawinput", -1.0f);
    const int warps = services.desktopWarps;
    IN_ResetMouse();
    CHECK(m_bRawInput && services.desktopWarps == warps + 1);
    CHECK(services.desktopWarpsAtRawRead == warps + 1);
    IN_Shutdown();
#endif
}

static int TestDirectReadSafety() {
    for (bool fallback : {false, true}) {
        for (bool accumulate : {false, true}) {
            for (bool poison : {false, true}) {
                if (!TestDirectFailure(fallback, accumulate, poison)) {
                    return 2;
                }
            }
        }
    }
    TestFailedReadRawRecovery(false);
    TestFailedReadRawRecovery(true);
    TestNormalRawRefreshOrder();
    std::printf("actual-%s checked-direct-read/retry/raw-recovery checks=%d fake-input-only\n",
        CSE_GAME_CS ? "cs" : "hl", Checks());
    return 0;
}

int main(int argc, char **argv) {
    if (argc == 2 && std::strcmp(argv[1], "--direct-read-safety") == 0) {
        return TestDirectReadSafety();
    }
    if (argc == 2 && std::strcmp(argv[1], "--join-failure") == 0) {
        StartThreaded();
        HoldQuery();
        AwaitQuery();
        IN_Shutdown();
        std::fputs("ERROR: shutdown returned with a live worker\n", stderr);
        return 0;
    }
    TestCameraAndRecenter();
    TestSnapshotsAndClear();
    TestVisibilityAndFallback();
    std::printf("actual-%s-native-wiring checks=%d fake-cursor owned-kernel-worker-only\n",
        CSE_GAME_CS ? "cs" : "hl", Checks());
}
