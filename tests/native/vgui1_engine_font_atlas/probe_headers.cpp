#include "VGUI_EngineSurfaceWrap.h"
#include "IEngineSurface.h"
#include "VGUI_Font.h"
#include "VGui1NativeFontSelection.h"
#include <type_traits>

namespace {
class AccessMethods : public EngineSurfaceWrap {
public:
    using EngineSurfaceWrap::drawSetTextFont;
    using EngineSurfaceWrap::drawPrintText;
    using EngineSurfaceWrap::invalidate;
};

static_assert(std::is_base_of<vgui::SurfaceBase, EngineSurfaceWrap>::value, "Actual complete surface inheritance");
static_assert(std::is_base_of<IBaseInterface, IEngineSurface>::value, "Actual engine interface inheritance");
static_assert(std::is_same<decltype(&AccessMethods::drawSetTextFont),
    void (EngineSurfaceWrap::*)(vgui::Font*)>::value, "Actual font selection declaration");
static_assert(std::is_same<decltype(&AccessMethods::drawPrintText),
    void (EngineSurfaceWrap::*)(const char*, int)>::value, "Actual explicit-length text declaration");
static_assert(std::is_same<decltype(&vgui::Font::getTextSize),
    void (vgui::Font::*)(const char*, int&, int&)>::value, "Actual virtual Font measurement declaration");
static_assert(std::is_same<decltype(&IEngineSurface::drawPrintChar),
    void (IEngineSurface::*)(int, int, int, int, float, float, float, float)>::value,
    "Actual engine surface glyph forwarding declaration");
static_assert(!cse_vgui1_selection::PublisherReady && !cse_vgui1_selection::SurfaceReady
    && !cse_vgui1_selection::ColdLoadReady, "This probe does not promote publisher/surface/cold-load readiness");
}
