#include "VGui1NativeFontSelection.h"
#include "VGUI_Font.h"
#include "VGUI_Scheme.h"
#include <type_traits>

static_assert(std::is_same<decltype(&vgui::Font::getPlat), vgui::BaseFontPlat* (vgui::Font::*)()>::value,
    "Actual borrowed identity method type");
static_assert(std::is_same<decltype(&vgui::Font::getId), int (vgui::Font::*)()>::value,
    "Actual legacy numeric ID method type");
static_assert(std::is_same<decltype(&vgui::Scheme::getFont),
    vgui::Font* (vgui::Scheme::*)(vgui::Scheme::SchemeFont)>::value, "Actual Scheme slot method type");
static_assert(std::is_standard_layout<cse_vgui1_selection::PreparedFont>::value
    && std::is_trivial<cse_vgui1_selection::PreparedFont>::value, "Prepared use record is typed POD");
static_assert(!cse_vgui1_selection::PublisherReady && !cse_vgui1_selection::SurfaceReady
    && !cse_vgui1_selection::ColdLoadReady, "Build-only component cannot qualify provider or cold readiness");
