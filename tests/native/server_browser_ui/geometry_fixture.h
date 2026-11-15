#pragma once
#include "vgui/ISurface.h"
#include "vgui/IPanel.h"
#include "Panel.h"
#include <array>
#include <cstring>
#include <string>
#include <vector>

namespace geometry_fixture {
using namespace vgui2;
// Only the engine's video and embedded-root inputs are controlled. The extracted
// complete geometry methods below are the same methods used by the game surface.
struct SCREENINFO {
    int iSize;
    int iWidth;
    int iHeight;
};
inline int screenWidth = 1600;
inline int screenHeight = 1200;
inline void ScreenInfo(SCREENINFO *info) {
    info->iWidth = screenWidth;
    info->iHeight = screenHeight;
}
struct EngineFunctions {
    void (*pfnGetScreenInfo)(SCREENINFO *) = ScreenInfo;
};
inline EngineFunctions gEngfuncs;
class BaseUISurface final : public ISurface {
public:
    explicit BaseUISurface(ISurface *real) : real_(real) {}
    // Passive paint observation; every character still reaches the actual surface.
    std::wstring paintedText;
    bool observeText = false;
    std::vector<Color> paintedPolyLineColors;
    Color paintColor;
    bool observePolyLines = false;
    std::vector<Color> paintedLineColors;
    bool observeLines = false;
    std::vector<Color> paintedTextColors;
    Color paintTextColor;
    bool observeTextColors = false;
    std::vector<Color> paintedFilledRectColors;
    std::vector<std::array<int, 4>> paintedFilledRects;
    bool observeFilledRects = false;
    bool forceCursorWithin = false;
    bool forceWindowFocus = false;
    void InspectionRoot(VPANEL root) {
        _embeddedPanel = root;
    }
#include "geometry_forward.inc"
    void Root(VPANEL root) {
        _embeddedPanel = root;
        real_->SetEmbeddedPanel(root);
    }
    void Resize(int width, int height) {
        screenWidth = width;
        screenHeight = height;
        m_iSurfaceBounds[2] = width;
        m_iSurfaceBounds[3] = height;
        if (_embeddedPanel) {
            vgui2::ipanel()->SetSize(_embeddedPanel, width, height);
        }
    }

private:
    ISurface *real_;
    VPANEL _embeddedPanel = 0;
    int m_iSurfaceBounds[4]{0, 0, 1600, 1200};
    int _proportionalBaseWidth = 640;
    int _proportionalBaseHeight = 480;
    int _proportionalBaseWidth_HD = 1280;
    int _proportionalBaseHeight_HD = 720;
};
#include "geometry_bodies.inc"
inline BaseUISurface *surface = nullptr;
inline void *Factory(const char *name, int *) {
    return std::strcmp(name, "VGUI_Surface026") == 0 ? surface : nullptr;
}
inline vgui2::Panel *embeddedRoot = nullptr;
inline vgui2::Panel *CreateParent(BaseUISurface &geometry) {
    embeddedRoot = new vgui2::Panel(nullptr, "EngineEmbeddedRoot");
    embeddedRoot->SetSize(screenWidth, screenHeight);
    geometry.Root(embeddedRoot->GetVPanel());
    auto *parent = new vgui2::Panel(embeddedRoot, "ActualHDTaskbarContext");
    parent->SetHDProportional(true);
    parent->SetProportional(true);
    parent->SetSize(screenWidth, screenHeight);
    return parent;
}
}
