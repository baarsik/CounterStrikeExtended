#include "VGUI_App.h"
#include "VGui1AppPlatform.h"
#include <type_traits>

static_assert(std::is_same<decltype(&vgui::App::setRegistryString),
    bool (vgui::App::*)(const char*, const char*)>::value, "Real App string-write return is bool");
static_assert(std::is_same<decltype(&vgui::App::getRegistryString),
    bool (vgui::App::*)(const char*, char*, int)>::value, "Real App string-read return is bool");
static_assert(std::is_same<decltype(&vgui::App::setRegistryInteger),
    bool (vgui::App::*)(const char*, int)>::value, "Real App integer-write return is bool");
static_assert(std::is_same<decltype(&vgui::App::getRegistryInteger),
    bool (vgui::App::*)(const char*, int&)>::value, "Real App integer-read return is bool");
static_assert(!cse_vgui1_app::ColdLoadReady && !cse_vgui1_app::NativeSurfaceReady,
    "Unselected component does not qualify module or Surface readiness");
