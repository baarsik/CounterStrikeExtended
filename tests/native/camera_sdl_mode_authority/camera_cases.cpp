#include <cstdio>
#include <cstdint>
#include "fixture_sdl.h"

#if UINTPTR_MAX > UINT32_MAX

static bool fixtureCameraUsesSdk = false;
#define main RetainedCameraRegressionMain
#include "camera_scaffold.inc"
#undef main
#endif

void RunCameraCases() {
#if UINTPTR_MAX > UINT32_MAX
    CHECK(RetainedCameraRegressionMain() == 0);
    for (int mode = 0; mode < 4; ++mode) {
        fixtureCameraUsesSdk = false;
        Reset();
        FixtureReset();
        SDL_Mouse *mouse = SDL_GetMouse();
        if (mode == 3) {
            FixtureInitialCursor(false);
            FixtureWarp(static_cast<float>(FixtureCenterX), static_cast<float>(FixtureCenterY));
            FixtureClock(SDL_MS_TO_NS(110));
            FixtureWarp(static_cast<float>(FixtureCenterX), static_cast<float>(FixtureCenterY));
            CHECK(mouse->relative_mode && mouse->warp_emulation_active);
            CHECK(!SDL_GetWindowRelativeMouseMode(FixtureWindow(0)));
        } else if (mode != 0) {
            CHECK(SDL_SetWindowRelativeMouseMode(FixtureWindow(0), true));
            mouse->relative_mode_warp_motion = mode == 2;
            FixtureWarp(static_cast<float>(FixtureCenterX), static_cast<float>(FixtureCenterY));
        }
        fixtureCameraUsesSdk = true;
        CAM_StartMouseMove();
        const float beforeYaw = cam_idealyaw->value;
        const float beforePitch = cam_idealpitch->value;
        FixtureClearCounts();
        services.queries = 0;
        services.warps.clear();
        FixtureMotion(mode != 0, mode == 0 ? static_cast<float>(FixtureCenterX + FixtureMotionX) : static_cast<float>(FixtureMotionX), mode == 0 ? static_cast<float>(FixtureCenterY + FixtureMotionY) : static_cast<float>(FixtureMotionY));
        CHECK(mouse->x == static_cast<float>(FixtureCenterX + FixtureMotionX) && mouse->y == static_cast<float>(FixtureCenterY + FixtureMotionY));
        CAM_Think();
        CHECK(cam_idealyaw->value == beforeYaw + 10.0f);
        CHECK(cam_idealpitch->value == beforePitch + 6.0f);
        CHECK(mouse->x == static_cast<float>(FixtureCenterX) && mouse->y == static_cast<float>(FixtureCenterY));
        const float firstYaw = cam_idealyaw->value;
        const float firstPitch = cam_idealpitch->value;
        CAM_Think();
        CAM_Think();
        CHECK(cam_idealyaw->value == firstYaw && cam_idealpitch->value == firstPitch);
        const FixtureCounts count = FixtureGetCounts();
        CHECK(services.queries == 3 && services.warps.size() == 3);
        CHECK(count.mouseQueries == 3 && count.warpRequests == 3);
        CHECK(count.backendWarps == (mode == 0 ? 3 : 0));
        CHECK(count.relativeDrains == 0);
        CHECK(mouse->x_accu == static_cast<float>(FixtureMotionX));
        CHECK(mouse->y_accu == static_cast<float>(FixtureMotionY));
        std::printf("CAM game=%s mode=%d one-motion-three-thinks: query=%d warp=%d nativewarp=%d drain=%d "
            "yaw=%.1f pitch=%.1f cached=%.1f,%.1f accumulator=%.1f,%.1f repeated=false\n",
            CSE_CAMERA_CS ? "CS" : "HL", mode, count.mouseQueries, count.warpRequests,
            count.backendWarps, count.relativeDrains, firstYaw - beforeYaw, firstPitch - beforePitch,
            mouse->x, mouse->y, mouse->x_accu, mouse->y_accu);
        fixtureCameraUsesSdk = false;
    }
#else
    std::puts("SDK mode bodies execute on x86; native64 CAM execution is outside this pointer-width profile");
#endif
}
