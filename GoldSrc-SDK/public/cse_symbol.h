#pragma once

#include <stdint.h>

// Process-local code addresses never use a wire/save integer representation.
// Member-function pointers remain typed values in the saved-callback registry.
typedef uintptr_t CseSymbolAddress;
