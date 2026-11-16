#pragma once

#include <algorithm>
#include <cstdio>
#include <cstring>
#include "legacy_text_encoding.h"

inline void NativeRadar_CopyString(char* out, const char* text, std::size_t capacity) {
    if (capacity > 0) {
        const std::size_t count = std::min(capacity - 1, std::strlen(text));
        std::memcpy(out, text, count);
        out[count] = '\0';
    }
}

template <std::size_t Capacity> inline void NativeRadar_CopyString(char (&out)[Capacity], const char* text) {
    NativeRadar_CopyString(out, text, Capacity);
}

inline void NativeRadar_TruncateUtf8(char* text, int characters) {
    auto* cursor = reinterpret_cast<const unsigned char*>(text);
    for (int i = 0; *cursor && i < characters; ++i) {
        cursor += cse::legacy_text::detail::DecodeUtf8(cursor).consumed;
    }
    text[cursor - reinterpret_cast<const unsigned char*>(text)] = '\0';
}
