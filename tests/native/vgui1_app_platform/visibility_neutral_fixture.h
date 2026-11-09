#ifndef CSE_TEST_VGUI1_VISIBILITY_NEUTRAL_FIXTURE_H
#define CSE_TEST_VGUI1_VISIBILITY_NEUTRAL_FIXTURE_H
#include "VGUI.h"
#if defined(_WIN32)
// Execution fixture ONLY. The separate original-profile compile retains real
// dllexport. Neutralize visibility here so this EXE need not supply every other
// VGUI class exported by the retained header. No type, access or slot is changed.
#undef VGUIAPI
#define VGUIAPI
#endif
#endif
