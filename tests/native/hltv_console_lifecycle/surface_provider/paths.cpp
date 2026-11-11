#include "fixture.h"

namespace {

void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "FAIL actual provider: %s\n", message);
        std::exit(EXIT_FAILURE);
    }
}

void CheckResourcesReleased() {
    Require(liveWindows == 0, "all owned windows destroyed");
    Require(liveDrawingContexts == 0, "all drawing contexts destroyed");
    Require(liveRegions == 0, "all owned regions destroyed");
    Require(timers == 0, "destroying windows retires their timers");
    Require(dragDropTargets == 0, "drag/drop registration revoked");
}

}

int main() {
    CWin32Surface surface;
    constexpr int RetirementCycles = 2;
    for (int cycle = 0; cycle < RetirementCycles; ++cycle) {
        VPanel root;
        const VPANEL rootHandle = reinterpret_cast<VPANEL>(&root);
        surface.SetEmbeddedPanel(rootHandle);
        Require(root.Plat() != nullptr && root.IsPopup(), "actual CreatePopup owns the root context");
        Require(surface._popupList.panels.size() == 1, "actual CreatePopup publishes one root popup");
        Require(surface._currentContextPanel == rootHandle, "root context selected");
        Require(staticSurfaceAvailable == 1, "surface available after embedding");
        const int callsBeforeDetach = osCalls;
        surface.SetEmbeddedPanel(0);
        Require(osCalls == callsBeforeDetach, "null detach must not create popup/timer/language OS work");
        Require(surface._embeddedPanel == 0 && surface._currentContextPanel == 0, "null detach clears root views");
        Require(staticSurfaceAvailable == 0, "null detach closes window callback admission");
        Require(root.Plat() != nullptr && surface._popupList.panels.size() == 1,
                "null detach preserves platform/popup ownership until panel deletion");
        surface.SetEmbeddedPanel(0);
        surface.ReleasePanel(rootHandle);
        Require(root.Plat() == nullptr && surface._popupList.panels.empty(), "actual ReleasePanel retires ownership");
        surface.ReleasePanel(rootHandle);
        CheckResourcesReleased();
    }
    VPanel root;
    VPanel child;
    const VPANEL rootHandle = reinterpret_cast<VPANEL>(&root);
    const VPANEL childHandle = reinterpret_cast<VPANEL>(&child);
    surface.SetEmbeddedPanel(rootHandle);
    surface.CreatePopup(childHandle, false, true, false, true, true);
    surface.SetContextPanel(childHandle);
    surface.SetEmbeddedPanel(0);
    Require(surface._currentContextPanel == childHandle, "detach preserves a still-live child drawing context");
    Require(surface._popupList.panels.size() == 2, "detach preserves both popup release obligations");
    surface.ReleasePanel(childHandle);
    Require(surface._currentContextPanel == 0, "child release falls back to detached empty root");
    surface.ReleasePanel(rootHandle);
    Require(surface._popupList.panels.empty(), "all popups released before shutdown");
    CheckResourcesReleased();
    std::puts("PASS actual CWin32Surface setter/CreatePopup/ReleasePanel and VPanel null-detach lifecycle");
}
