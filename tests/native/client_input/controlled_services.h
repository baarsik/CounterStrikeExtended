#pragma once

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>
#include <SDL3/SDL.h>

#if defined(_WIN32)
#include <windows.h>
#endif

// Exercise the native non-Windows caller branch with every compiler, without
// pretending that this is Windows platform-policy or DLL ABI qualification.
#if CSE_TEST_NATIVE_BRANCH && defined(_WIN32)
#undef _WIN32
#endif

using byte = unsigned char;
using vec3_t = float[3];
using string_t = int;
struct edict_s;
using edict_t = edict_s;
#include "cvardef.h"
#include "usercmd.h"
#include "progdefs.h"
#include "kbutton.h"
#include "in_defs.h"
#include "keydefs.h"
#include "sdl3_input.h"
#include "native_mouse_polling.h"

#define CL_DLLEXPORT
constexpr int MOUSE_BUTTON_COUNT = 5;

struct TestServices {
    float relativeX = 0.0f;
    float relativeY = 0.0f;
    int relativeReads = 0;
    int eventPumps = 0;
    int joystickCloses = 0;
    int startupJoystick = 0;
    int joystickMoves = 0;
    int getAngles = 0;
    int setAngles = 0;
    int stopPitchDrift = 0;
    int rawReads = 0;
    int desktopWarpsAtRawRead = 0;
    int sensitivityWrites = 0;
    int positionReads = 0;
    int desktopWarps = 0;
    int spiReads = 0;
    int spiWrites = 0;
    int cursorX = 327;
    int cursorY = 249;
    bool cursorVisible = false;
    bool noClipping = false;
    float angles[3] = {};
    std::set<std::string> parameters;
    std::vector<std::pair<int, int>> keys;
    std::vector<std::string> commands;
};

extern TestServices services;
int Checks();
void Check(bool condition, const char *expression, int line);
#define CHECK(expression) Check((expression), #expression, __LINE__)

cvar_t *Value(const char *name, float value);
cvar_t *RegisterVariable(const char *name, const char *value, int flags);
cvar_t *GetCvarPointer(const char *name);
float GetCvarFloat(const char *name);
void SetCvarValue(const char *name, float value);
void GetViewAngles(float *angles);
void SetViewAngles(float *angles);
void GetMousePosition(int *x, int *y);
int GetCenterX();
int GetCenterY();
int CheckParm(const char *name, char **);
int IsNoClipping();
void KeyEvent(int key, int down);
int AddCommand(const char *name, void (*callback)());
void QueueMotion(float x, float y);
void ResetServices();
void PrintDiagnostic(const char *format, ...);

// The engine and HUD are controlled caller seams. The exchanged scalar/record
// types above and the function bodies are real; this is not an ABI fixture.
struct TestEngine {
    void (*Con_Printf)(const char *, ...) = PrintDiagnostic;
    cvar_t *(*pfnRegisterVariable)(const char *, const char *, int) = RegisterVariable;
    cvar_t *(*pfnGetCvarPointer)(const char *) = GetCvarPointer;
    void (*Cvar_SetValue)(const char *, float) = SetCvarValue;
    void (*GetViewAngles)(float *) = ::GetViewAngles;
    void (*SetViewAngles)(float *) = ::SetViewAngles;
    void (*GetMousePosition)(int *, int *) = ::GetMousePosition;
    int (*GetWindowCenterX)() = GetCenterX;
    int (*GetWindowCenterY)() = GetCenterY;
    int (*CheckParm)(const char *, char **) = ::CheckParm;
    int (*IsNoClipping)() = ::IsNoClipping;
    void (*Key_Event)(int, int) = KeyEvent;
    int (*pfnAddCommand)(const char *, void (*)()) = AddCommand;
};

struct TestHud {
    float sensitivity = 0.0f;
    int m_iIntermission = 0;
    int m_iFOV = 90;
    cvar_t *default_fov = nullptr;

    float GetSensitivity() const {
        return sensitivity;
    }
};

namespace vgui2 {
struct TestSurface {
    bool IsCursorVisible() const {
        return services.cursorVisible;
    }
};

inline TestSurface *surface() {
    static TestSurface instance;
    return &instance;
}
}

#if defined(_WIN32)
BOOL OwnedGetCursorPos(POINT *point);
BOOL OwnedSetCursorPos(int x, int y);
BOOL OwnedSystemParametersInfo(UINT action, UINT parameter, void *data, UINT flags);
LONG OwnedInterlockedExchange(LONG *destination, LONG value);
HANDLE OwnedCreateEvent(void *, BOOL, BOOL, const char *);
HANDLE OwnedCreateThread(void *, SIZE_T, LPTHREAD_START_ROUTINE, void *, DWORD, DWORD *);
BOOL OwnedSetEvent(HANDLE);
DWORD OwnedWaitForSingleObject(HANDLE, DWORD);
BOOL OwnedCloseHandle(HANDLE);
BOOL OwnedTerminateThread(HANDLE, DWORD);
#undef GetCursorPos
#undef SetCursorPos
#undef SystemParametersInfo
#define GetCursorPos OwnedGetCursorPos
#define SetCursorPos OwnedSetCursorPos
#define SystemParametersInfo OwnedSystemParametersInfo
#if CSE_TEST_POLLING
constexpr auto FixtureSleepCondition = &::SleepConditionVariableSRW;
BOOL OwnedSleepCondition(PCONDITION_VARIABLE condition, PSRWLOCK lock, DWORD timeout, ULONG flags);
#define SleepConditionVariableSRW OwnedSleepCondition
#endif
#if !CSE_TEST_POLLING
#undef InterlockedExchange
#undef CreateEvent
#undef CreateThread
#undef SetEvent
#undef WaitForSingleObject
#undef CloseHandle
#undef TerminateThread
#define InterlockedExchange OwnedInterlockedExchange
#define CreateEvent OwnedCreateEvent
#define CreateThread OwnedCreateThread
#define SetEvent OwnedSetEvent
#define WaitForSingleObject OwnedWaitForSingleObject
#define CloseHandle OwnedCloseHandle
#define TerminateThread OwnedTerminateThread
#endif
#endif
