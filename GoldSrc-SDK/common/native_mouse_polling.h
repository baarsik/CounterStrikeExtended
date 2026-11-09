#pragma once

#include <stdint.h>

#if UINTPTR_MAX > UINT32_MAX
// Internal to each owned client DLL. Main-thread, edge-only and idempotent.
// The caller publishes effective combined camera intent before calling this
// hook, and completes its final recenter before releasing ownership.
// The hook preserves filter history and the existing button-state policy.
void IN_SetCameraMouseOwnership(bool owned);
#endif
