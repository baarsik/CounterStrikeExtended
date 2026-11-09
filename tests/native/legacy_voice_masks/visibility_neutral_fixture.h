#pragma once

#include "VGUI.h"

#if defined(_WIN32)
// Console execution fixture only: retain every actual class/type/virtual slot
// without requesting exports for unrelated VGUI classes in this standalone EXE.
// Separate whole-owner compiles retain the original production visibility.
#undef VGUIAPI
#define VGUIAPI
#endif
