#include "controlled_services.h"
#include <cstring>

TestServices services;
static std::map<std::string, cvar_t> variables;
static int checks = 0;

int Checks() {
    return checks;
}

void Check(bool condition, const char *expression, int line) {
    ++checks;
    if (!condition) {
        std::fprintf(stderr, "input-check:%d:%s\n", line, expression);
        std::abort();
    }
}

cvar_t *Value(const char *name, float value) {
    cvar_t &variable = variables[name];
    variable.value = value;
    return &variable;
}

cvar_t *RegisterVariable(const char *name, const char *value, int flags) {
    cvar_t *variable = Value(name, std::strtof(value, nullptr));
    variable->flags = flags;
    return variable;
}

cvar_t *GetCvarPointer(const char *name) {
    auto found = variables.find(name);
    return found == variables.end() ? nullptr : &found->second;
}

float GetCvarFloat(const char *name) {
    if (std::strcmp(name, "m_rawinput") == 0) {
        ++services.rawReads;
        services.desktopWarpsAtRawRead = services.desktopWarps;
    }
    cvar_t *variable = GetCvarPointer(name);
    return variable ? variable->value : 0.0f;
}

void SetCvarValue(const char *name, float value) {
    CHECK(std::strcmp(name, "sensitivity") == 0);
    ++services.sensitivityWrites;
    Value(name, value);
}

void GetViewAngles(float *angles) {
    ++services.getAngles;
    std::memcpy(angles, services.angles, sizeof(services.angles));
}

void SetViewAngles(float *angles) {
    ++services.setAngles;
    std::memcpy(services.angles, angles, sizeof(services.angles));
}

void GetMousePosition(int *x, int *y) {
    ++services.positionReads;
    *x = services.cursorX;
    *y = services.cursorY;
}

int GetCenterX() {
    return 320;
}

int GetCenterY() {
    return 240;
}

int CheckParm(const char *name, char **) {
    return services.parameters.count(name) != 0 ? 1 : 0;
}

int IsNoClipping() {
    return services.noClipping ? 1 : 0;
}

void KeyEvent(int key, int down) {
    services.keys.emplace_back(key, down);
}

int AddCommand(const char *name, void (*callback)()) {
    CHECK(callback != nullptr);
    services.commands.emplace_back(name);
    return 1;
}

void QueueMotion(float x, float y) {
    services.relativeX += x;
    services.relativeY += y;
}

void ResetServices() {
    services = TestServices{};
    variables.clear();
}

extern "C" SDL_MouseButtonFlags SDLCALL SDL_GetRelativeMouseState(float *x, float *y) {
    ++services.relativeReads;
    if (x) {
        *x = services.relativeX;
    }
    if (y) {
        *y = services.relativeY;
    }
    services.relativeX = 0.0f;
    services.relativeY = 0.0f;
    return 0;
}

extern "C" void SDLCALL SDL_PumpEvents() {
    ++services.eventPumps;
#if defined(__APPLE__)
    if (services.onPump) {
        const auto callback = services.onPump;
        services.onPump = nullptr;
        callback();
    }
#endif
}

extern "C" void SDLCALL SDL_CloseGamepad(SDL_Gamepad *gamepad) {
    CHECK(gamepad != nullptr);
    ++services.joystickCloses;
}

#if defined(_WIN32) && !CSE_TEST_POLLING
BOOL OwnedGetCursorPos(POINT *point) {
    ++services.positionReads;
    point->x = services.cursorX;
    point->y = services.cursorY;
    return TRUE;
}

BOOL OwnedSetCursorPos(int x, int y) {
    CHECK(x == GetCenterX() && y == GetCenterY());
    ++services.desktopWarps;
    return TRUE;
}

BOOL OwnedSystemParametersInfo(UINT action, UINT, void *data, UINT) {
    int *parameters = static_cast<int *>(data);
    if (action == SPI_GETMOUSE) {
        ++services.spiReads;
        parameters[0] = 6;
        parameters[1] = 10;
        parameters[2] = 2;
    } else {
        CHECK(action == SPI_SETMOUSE);
        ++services.spiWrites;
    }
    return TRUE;
}

LONG OwnedInterlockedExchange(LONG *destination, LONG value) {
    const LONG previous = *destination;
    *destination = value;
    return previous;
}

HANDLE OwnedCreateEvent(void *, BOOL, BOOL, const char *) {
    CHECK(false);
    return nullptr;
}

HANDLE OwnedCreateThread(void *, SIZE_T, LPTHREAD_START_ROUTINE, void *, DWORD, DWORD *) {
    CHECK(false);
    return nullptr;
}

BOOL OwnedSetEvent(HANDLE) {
    CHECK(false);
    return TRUE;
}

DWORD OwnedWaitForSingleObject(HANDLE, DWORD) {
    CHECK(false);
    return WAIT_OBJECT_0;
}

BOOL OwnedCloseHandle(HANDLE) {
    CHECK(false);
    return TRUE;
}

BOOL OwnedTerminateThread(HANDLE, DWORD) {
    CHECK(false);
    return TRUE;
}
#endif

#if !CSE_TEST_POLLING
void PrintDiagnostic(const char *, ...) {
}
#endif

#if defined(__APPLE__)
extern "C" void SDLCALL SDL_WarpMouseInWindow(SDL_Window *, float, float) {
    // Pinned Cocoa WarpMouseGlobal sends a synchronous absolute position after
    // SDL_PerformWarpMouseInWindow clears has_position: this contributes zero motion.
    ++services.windowWarps;
    if (services.onWarp) {
        const auto callback = services.onWarp;
        services.onWarp = nullptr;
        callback();
    }
}
#endif
