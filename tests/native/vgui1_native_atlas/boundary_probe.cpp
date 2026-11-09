#include "VGui1NativeRuntime.h"
#include "VGui1NativeAtlas.h"
#include "VGui1NativeFontSelection.h"
#include "VGui1FontPlatform.h"
#include <type_traits>

using Platform = vgui::BaseFontPlat;
static_assert(std::is_same<decltype(&Platform::getCharABCwide),
    void (Platform::*)(int, int&, int&, int&)>::value, "Actual private ABC member boundary");
static_assert(std::is_same<decltype(&Platform::getCharRGBA),
    void (Platform::*)(int, int, int, int, int, uchar*)>::value, "Actual private raster boundary");
static_assert(std::is_same<decltype(&Platform::getTall), int (Platform::*)()>::value,
    "Actual private height boundary");
static_assert(std::is_same<decltype(&Platform::getWide), int (Platform::*)()>::value,
    "Actual private maximum-width boundary");
static_assert(std::is_same<decltype(&Platform::drawSetTextFont),
    void (Platform::*)(vgui::SurfacePlat*)>::value, "Actual private surface identity boundary");
static_assert(std::is_same<decltype(&Platform::equals),
    bool (Platform::*)(const char*, int, int, float, int, bool, bool, bool, bool)>::value,
    "Actual private request identity boundary");
static_assert(std::has_virtual_destructor<Platform>::value && std::is_abstract<Platform>::value,
    "Actual virtual-destructor/abstract platform ownership");
static_assert(cse_vgui1_native::AtlasPageLimit == 8, "All intended page slots");

#if defined(CSE_PROVIDER_BOUNDARY)
#define CSE_BOUNDARY_NAME FixtureProviderPlatformFacts
#else
#define CSE_BOUNDARY_NAME FixtureConsumerPlatformFacts
#endif
extern "C" bool CSE_BOUNDARY_NAME(std::uint64_t* output, std::uint32_t count) {
    const std::uint64_t facts[]{sizeof(Platform), alignof(Platform), sizeof(int), sizeof(float), sizeof(uchar),
        sizeof(void*), sizeof(decltype(&Platform::getTall)), sizeof(decltype(&Platform::getCharRGBA)),
        sizeof(CseVgui1FontStatusV1), alignof(CseVgui1FontStatusV1)};
    if (!output || count != sizeof(facts) / sizeof(facts[0])) {
        return false;
    }
    for (std::size_t index = 0; index < count; ++index) {
        output[index] = facts[index];
    }
    return true;
}
#undef CSE_BOUNDARY_NAME
