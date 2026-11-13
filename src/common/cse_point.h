#pragma once

#if !defined(_WIN32)
#include <stdint.h>

// The legacy engine mouse callback exchanges two Windows LONG-sized coordinates.
// Keep its tag and eight-byte record when native long is wider than 32 bits.
struct tagPOINT {
    int32_t x;
    int32_t y;
};
#endif
