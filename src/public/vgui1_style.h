#pragma once

#include "vgui_style_service.h"

#if defined(_WIN32)
#if defined(VGUI_EXPORTS)
#define CSE_VGUI1_STYLE_EXPORT __declspec(dllexport)
#else
#define CSE_VGUI1_STYLE_EXPORT __declspec(dllimport)
#endif
#else
#define CSE_VGUI1_STYLE_EXPORT __attribute__((visibility("default")))
#endif

// New optional binding entry point; existing VGUI1 classes, exports and virtual slots are unchanged.
extern "C" CSE_VGUI1_STYLE_EXPORT void CSE_ABI_CALL CSE_VGui1BindStyle(const cse::ui::StyleApi* api);
