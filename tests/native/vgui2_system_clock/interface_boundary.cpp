#include CSE_CLOCK_SYSTEM_HEADER
#include <cstdint>
#include <type_traits>

using NativeMillis = std::int32_t (vgui2::ISystem::*)();
using SelectedSourceMillis = long (vgui2::ISystem::*)();
#ifdef CSE_CLOCK_EXPECT_SELECTED_LONG
static_assert(std::is_same<decltype(&vgui2::ISystem::GetTimeMillis), SelectedSourceMillis>::value,
    "The selected System.cpp long definition disagrees with the native signed32 interface");
#else
static_assert(std::is_same<decltype(&vgui2::ISystem::GetTimeMillis), NativeMillis>::value,
    "The existing native interface remains signed32");
#endif
