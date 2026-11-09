#include "FontManager.h"
#include "LinuxFont.h"
#include "native/MacFontDiscovery.h"
#include "native/MacFontRegistration.h"
#include <type_traits>

using ScalarLookup = CLinuxFont* (CFontManager::*)(vgui2::HFont, wchar_t);
static_assert(std::is_same<decltype(&CFontManager::GetFontForChar), ScalarLookup>::value,
    "Manager consumes one native wchar value per lookup");
static_assert(sizeof(vgui2::HFont) == 4, "Public logical font handle remains 32 bits");
static_assert(std::is_same<decltype(cse::fonts::FontFile::faceIndex), long>::value,
    "FreeType face index retains its selected native type");
char manager_layout_probe[sizeof(CFontManager)];
char raster_layout_probe[sizeof(CLinuxFont)];
