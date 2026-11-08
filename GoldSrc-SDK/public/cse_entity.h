#pragma once

#include <stdint.h>

// Byte offsets are native process storage. Wire/save entity indices remain fixed-width integers.
typedef intptr_t EOFFSET;

#ifdef __cplusplus
namespace cse { namespace entities {
// Legacy zero identifies the world in lookup APIs and the null value in stored offset fields.
constexpr EOFFSET WorldOffset = 0;
} }
#endif
