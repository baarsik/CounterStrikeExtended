#pragma once

#include <stdint.h>

// Native engine string identities are relative addresses, not serialized integers.
// Zero keeps its existing identity: the engine's empty-string base.
typedef uintptr_t string_t;

#ifdef __cplusplus
inline string_t CseStringOffset(const char *base, const char *text) {
    return reinterpret_cast<uintptr_t>(text) - reinterpret_cast<uintptr_t>(base);
}

inline const char *CseStringAddress(const char *base, string_t offset) {
    return reinterpret_cast<const char *>(reinterpret_cast<uintptr_t>(base) + offset);
}

inline char *CseStringAddress(char *base, string_t offset) {
    return reinterpret_cast<char *>(reinterpret_cast<uintptr_t>(base) + offset);
}
#endif
