#pragma once

#include <alloca.h>
#include <cstdlib>
#define _alloca alloca
#define _HEAPOK 0

// File loading is outside the fixture; detect unintended entry into its CRT heap check.
inline int _heapchk() {
    std::abort();
}
