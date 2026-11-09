#include "../client_mouse_coordinates/controlled_window.h"

int main() {
    ResetWindow();
    controlled.windowWidth = 640;
    controlled.windowHeight = 480;
    controlled.pixelWidth = 640;
    controlled.pixelHeight = 480;
    controlled.mouseX = 320.0f;
    controlled.mouseY = 240.0f;
    // A10 observed this distinction: one fresh OS displacement, centered SDL cache.
    constexpr int freshOsX = 360;
    constexpr int freshOsY = 264;
    int x = -1;
    int y = -1;
    int centerX = -1;
    int centerY = -1;
    const bool success = cse::TryGetEngineMousePositionAndCenter(ownedWindow, &x, &y, &centerX, &centerY);
    if (!success || x != freshOsX || y != freshOsY || centerX != 320 || centerY != 240) {
        std::fprintf(stderr, "fresh-os-sample: expected=(%d,%d;320,240), actual=(%d,%d;%d,%d), success=%d\n", freshOsX,
                     freshOsY, x, y, centerX, centerY, success ? 1 : 0);
        return 17;
    }
    return 0;
}
