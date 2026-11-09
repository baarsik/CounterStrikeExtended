#pragma once
#include "tier0/platform.h"
#include <cstdio>
#include <cstdlib>

// These existing POSIX exports have no declarations in the retained platform.h.
extern unsigned long Plat_PrimaryThreadID;
unsigned int Plat_RegisterThread(const char* name);
unsigned int Plat_RegisterPrimaryThread();
unsigned int Plat_GetCurrentThreadID();

inline void Require(bool condition, const char* description) {
    if (!condition) {
        std::fprintf(stderr, "FAILED: %s\n", description);
        std::abort();
    }
}
