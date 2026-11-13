#pragma once

namespace cse {
namespace runtime_modules {

// Select names before a VFS lookup. A foreign extension can fail resolution
// before the platform loader has an opportunity to adapt it.
#if defined(_WIN32)
constexpr char FileSystem[] = "filesystem_stdio.dll";
constexpr char SoftwareEngine[] = "swds.dll";
constexpr char Vgui1[] = "vgui.dll";
constexpr char Vgui2[] = "vgui2.dll";
constexpr char GameUI[] = "cl_dlls\\GameUI.dll";
#elif defined(__APPLE__)
constexpr char FileSystem[] = "filesystem_stdio.dylib";
constexpr char SoftwareEngine[] = "swds.dylib";
constexpr char Vgui1[] = "vgui.dylib";
constexpr char Vgui2[] = "vgui2.dylib";
constexpr char GameUI[] = "cl_dlls/GameUI.dylib";
#else
constexpr char FileSystem[] = "filesystem_stdio.so";
constexpr char SoftwareEngine[] = "swds.so";
constexpr char Vgui1[] = "vgui.so";
constexpr char Vgui2[] = "vgui2.so";
constexpr char GameUI[] = "cl_dlls/GameUI.so";
#endif

} // namespace runtime_modules
} // namespace cse
