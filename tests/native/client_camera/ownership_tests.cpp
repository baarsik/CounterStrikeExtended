#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <vector>
#include "cvardef.h"
#include "kbutton.h"
#include "client_camera_mouse.h"
#include "native_mouse_polling.h"

using vec3_t = float[3];
struct POINT {
    int x;
    int y;
};
constexpr int PITCH = 0;
constexpr int YAW = 1;
int mouse_x = 0;
int mouse_y = 0;

static int checks = 0;
#define CHECK(expression) do { ++checks; if (!(expression)) { \
    std::fprintf(stderr, "camera:%d:%s\n", __LINE__, #expression); std::abort(); \
} } while (false)

struct Services {
    bool querySucceeds = true;
    int queries = 0;
    int oldQueries = 0;
    int legacyCenters = 0;
    int lastWarpQuery = -1;
    int x = 73;
    int y = 74;
    int centerX = 320;
    int centerY = 240;
    std::vector<bool> ownership;
    std::vector<std::pair<int, int>> warps;
};
static Services services;
static float viewAngles[3] = {11.0f, 19.0f, 0.0f};
int Query(int *x, int *y, int *centerX, int *centerY) {
    ++services.queries;
    if (!services.querySucceeds) {
        return 0;
    }
    *x = services.x;
    *y = services.y;
    *centerX = services.centerX;
    *centerY = services.centerY;
    return 1;
}

struct Engine {
    int (*pfnGetClientMousePositionAndCenter)(int *, int *, int *, int *) = Query;
    void GetMousePosition(int *x, int *y) {
        ++services.oldQueries;
        *x = 999;
        *y = 888;
    }
    void GetViewAngles(float *output) {
        std::memcpy(output, viewAngles, sizeof(viewAngles));
    }
    void Cvar_SetValue(const char *, float) {}
    int GetMaxClients() {
        return 1;
    }
    int GetWindowCenterX() {
        ++services.legacyCenters;
        return 999;
    }
    int GetWindowCenterY() {
        ++services.legacyCenters;
        return 888;
    }
    void pfnSetMousePos(int x, int y) {
        services.lastWarpQuery = services.queries;
        services.warps.emplace_back(x, y);
    }
};
Engine gEngfuncs;
struct Hud {
    float sensitivity = 2.0f;
    float GetSensitivity() const {
        return sensitivity;
    }
};
Hud gHUD;

extern int iMouseInUse;
void IN_SetCameraMouseOwnership(bool owned) {
    CHECK((iMouseInUse != 0) == owned);
    if (!owned && services.querySucceeds && gEngfuncs.pfnGetClientMousePositionAndCenter) {
        CHECK(!services.warps.empty());
        CHECK(services.warps.back() == std::make_pair(services.centerX, services.centerY));
        CHECK(services.lastWarpQuery == services.queries);
    }
    services.ownership.push_back(owned);
}
float CL_KeyState(kbutton_t *key) {
    return (key->state & 1) ? 1.0f : 0.0f;
}

struct moveclip_t {};
#define CL_DLLEXPORT
#define EXPORT
#define VectorCopy(source, destination) std::memcpy(destination, source, sizeof(vec3_t))
// The HL body retains unused locals and its original narrowing arithmetic.
// Keep those imported-body policies without changing the new helper's checks.
// They are not executable seams; owning CPP compilation is a separate check.
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-variable"
#pragma clang diagnostic ignored "-Wunused-parameter"
#elif defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable: 4100 4101 4244)
#endif
#if CSE_CAMERA_CS
#include "cs_camera.inc"
#else
#include "hl_camera.inc"
#endif
#if defined(__clang__)
#pragma clang diagnostic pop
#elif defined(_MSC_VER)
#pragma warning(pop)
#endif

static cvar_t command{};
static cvar_t snap{};
static cvar_t idealPitch{};
static cvar_t idealYaw{};
static cvar_t idealDistance{};
static cvar_t maxPitch{};
static cvar_t minPitch{};
static cvar_t maxYaw{};
static cvar_t minYaw{};
static cvar_t maxDistance{};
static cvar_t minDistance{};

static void Reset() {
    CAM_EndDistance();
    CAM_EndMouseMove();
    services = Services{};
    gEngfuncs.pfnGetClientMousePositionAndCenter = Query;
    gHUD.sensitivity = 2.0f;
    cam_command = &command;
    cam_snapto = &snap;
    cam_idealpitch = &idealPitch;
    cam_idealyaw = &idealYaw;
    cam_idealdist = &idealDistance;
    c_maxpitch = &maxPitch;
    c_minpitch = &minPitch;
    c_maxyaw = &maxYaw;
    c_minyaw = &minYaw;
    c_maxdistance = &maxDistance;
    c_mindistance = &minDistance;
    command.value = 0.0f;
    snap.value = 1.0f;
    idealPitch.value = 10.0f;
    idealYaw.value = 20.0f;
    idealDistance.value = 100.0f;
    maxPitch.value = 90.0f;
    minPitch.value = 0.0f;
    maxYaw.value = 135.0f;
    minYaw.value = -135.0f;
    maxDistance.value = 200.0f;
    minDistance.value = 30.0f;
    std::memset(cam_ofs, 0, sizeof(cam_ofs));
    cam_thirdperson = 1;
    cam_mousemove = 0;
    cam_distancemove = 0;
    iMouseInUse = 0;
    cam_mouse.x = 5;
    cam_mouse.y = 7;
    cam_old_mouse_x = -17;
    cam_old_mouse_y = -19;
#if !CSE_CAMERA_CS
    (void)cam_move;
#endif
}

int main() {
    if (sizeof(void *) <= sizeof(std::uint32_t)) {
        std::puts("Native camera requires a 64-bit fixture process");
        return 1;
    }
    int history = 97;
    CHECK(cse::TryCameraMouseHistory((std::numeric_limits<int>::max)(), 0.0f, true, history));
    CHECK(history == (std::numeric_limits<int>::max)());
    CHECK(!cse::TryCameraMouseHistory((std::numeric_limits<int>::max)(), 1.0f, false, history));
    CHECK(history == (std::numeric_limits<int>::max)());
    CHECK(!cse::TryCameraMouseHistory((std::numeric_limits<int>::min)(), -1.0f, false, history));
    CHECK(cse::TryCameraMouseHistory(-3, 0.5f, false, history) && history == -1);
    CHECK(cse::TryCameraMouseHistory(3, -0.5f, false, history) && history == -1);
    CHECK(cse::TryCameraMouseHistory(37, 0.0f, false, history) && history == 0);
    cse::CameraMouseSnapshot snapshot;
    snapshot.x = 123;
    snapshot.y = 456;
    CHECK(!cse::TryReadCameraMouse(nullptr, 1.0f, false, snapshot));
    CHECK(snapshot.x == 123 && snapshot.y == 456);
    services.x = 1;
    services.y = (std::numeric_limits<int>::max)();
    CHECK(!cse::TryReadCameraMouse(Query, 2.0f, false, snapshot));
    CHECK(snapshot.x == 123 && snapshot.y == 456);
    Reset();
    services.querySucceeds = false;
    CAM_StartMouseMove();
    CHECK(!cam_mousemove && !iMouseInUse && services.queries == 1 && services.ownership.empty());
    CHECK(cam_mouse.x == 5 && cam_mouse.y == 7 && cam_old_mouse_x == -17 && cam_old_mouse_y == -19);
    Reset();
    CAM_StartMouseMove();
    CHECK(cam_mousemove && iMouseInUse && services.queries == 1 && services.oldQueries == 0);
    CHECK(cam_mouse.x == 73 && cam_mouse.y == 74 && cam_old_mouse_x == 146 && cam_old_mouse_y == 148);
    CHECK(services.ownership.size() == 1 && services.ownership[0]);
    CAM_StartDistance();
    CHECK(cam_distancemove && cam_mousemove && iMouseInUse);
    CAM_EndMouseMove();
    CHECK(cam_distancemove && cam_mousemove && iMouseInUse && services.ownership.size() == 1);
    CAM_EndDistance();
    CHECK(!cam_distancemove && !cam_mousemove && !iMouseInUse);
    CHECK(services.ownership.size() == 2 && !services.ownership[1]);
    CAM_EndDistance();
    CAM_EndMouseMove();
    CHECK(services.ownership.size() == 2);
    Reset();
    CAM_StartDistance();
    CAM_StartMouseMove();
    CAM_EndDistance();
    CHECK(!cam_distancemove && cam_mousemove && iMouseInUse && services.ownership.size() == 1);
    CAM_EndMouseMove();
    CHECK(!iMouseInUse && services.ownership.size() == 2 && !services.ownership.back());
    Reset();
    CAM_StartDistance();
    CAM_ClearStates();
    CHECK(!cam_mousemove && !cam_distancemove && !iMouseInUse && !cam_thirdperson);
    CHECK(services.ownership.size() == 2 && !services.ownership.back());
    Reset();
    CAM_StartMouseMove();
    CAM_ToFirstPerson();
    CHECK(!cam_mousemove && !cam_distancemove && !iMouseInUse && !cam_thirdperson);
    CHECK(services.ownership.size() == 2 && !services.ownership.back());
    Reset();
    gHUD.sensitivity = 0.0f;
    CAM_StartMouseMove();
    CHECK(cam_old_mouse_x == 73 && cam_old_mouse_y == 74);
    CAM_EndMouseMove();
    CAM_StartDistance();
    CHECK(cam_old_mouse_x == 0 && cam_old_mouse_y == 0);
    Reset();
    gHUD.sensitivity = std::numeric_limits<float>::infinity();
    CAM_StartDistance();
    CHECK(!iMouseInUse && !cam_distancemove && services.ownership.empty());
    CHECK(cam_old_mouse_x == -17 && cam_old_mouse_y == -19);
    Reset();
    CAM_StartMouseMove();
    services.centerX = 640;
    services.centerY = 480;
    services.x = 648;
    services.y = 484;
    const int beforeThink = services.queries;
    CAM_Think();
    CHECK(services.queries == beforeThink + 1 && services.oldQueries == 0 && services.legacyCenters == 0);
    CHECK(services.warps.size() == 1 && services.warps.back() == std::make_pair(640, 480));
    CHECK(idealPitch.value == 11.0f && idealYaw.value == 22.0f);
    CHECK(cam_ofs[PITCH] == 22.0f && cam_ofs[YAW] == 41.0f);
    CHECK(cam_ofs[2] == (CSE_CAMERA_CS ? 150.0f : 100.0f));
    CHECK(cam_old_mouse_x == 1296 && cam_old_mouse_y == 968);
    float oldOffset[3];
    std::memcpy(oldOffset, cam_ofs, sizeof(oldOffset));
    const float oldPitch = idealPitch.value;
    const float oldYaw = idealYaw.value;
    const float oldDistance = idealDistance.value;
    services.querySucceeds = false;
    CAM_Think();
    CHECK(std::memcmp(oldOffset, cam_ofs, sizeof(oldOffset)) == 0);
    CHECK(idealPitch.value == oldPitch && idealYaw.value == oldYaw && idealDistance.value == oldDistance);
    CHECK(services.warps.size() == 1 && cam_old_mouse_x == 1296 && cam_old_mouse_y == 968);
    services.querySucceeds = true;
    services.x = 318;
    services.y = 236;
    services.centerX = 320;
    services.centerY = 240;
    CAM_Think();
    CHECK(idealYaw.value == 21.5f && idealPitch.value == 10.0f);
    CHECK(services.warps.size() == 2 && services.warps.back() == std::make_pair(320, 240));
    CAM_EndMouseMove();
    CAM_StartDistance();
    CAM_Think();
    CHECK(cam_ofs[2] == (CSE_CAMERA_CS ? 148.0f : 98.0f));
    CHECK(idealDistance.value == (CSE_CAMERA_CS ? 150.0f : 98.0f));
    CHECK(services.warps.size() == 4 && services.oldQueries == 0 && services.legacyCenters == 0);
    Reset();
    gHUD.sensitivity = 0.0f;
    services.x = (std::numeric_limits<int>::min)();
    services.y = 0;
    services.centerX = 0;
    services.centerY = 0;
    CAM_StartMouseMove();
    CAM_Think();
    CHECK(idealYaw.value == minYaw.value);
    std::printf("Native camera ownership game=%s checks=%d\n", CSE_CAMERA_CS ? "CS" : "HL", checks);
    return 0;
}
