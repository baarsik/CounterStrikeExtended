#include "VGUI_Font.h"
#include <cstddef>
#include <type_traits>

using VguiFont = vgui::Font;
using RasterMethod = void (VguiFont::*)(int, int, int, int, int, unsigned char*);
using MetricsMethod = void (VguiFont::*)(int, int&, int&, int&);
using TextMethod = void (VguiFont::*)(const char*, int&, int&);
static_assert(std::is_same<decltype(&VguiFont::getCharRGBA), RasterMethod>::value, "VGUI1 raster signature");
static_assert(std::is_same<decltype(&VguiFont::getCharABCwide), MetricsMethod>::value, "VGUI1 ABC signature");
static_assert(std::is_same<decltype(&VguiFont::getTextSize), TextMethod>::value, "VGUI1 byte text signature");
static_assert(!std::has_virtual_destructor<VguiFont>::value, "Retained Font has no virtual destructor");
static_assert(sizeof(VguiFont) == sizeof(void*) * 4, "Selected Font layout including native tail padding");
#if !defined(_WIN32)
static_assert(std::is_same<decltype(&VguiFont::getWide), int (VguiFont::*)()>::value,
    "Existing native-only getWide slot remains part of the selected header");
#endif
// Force the actual compiler record layout without constructing or executing a font.
char font_layout_probe[sizeof(VguiFont)];
