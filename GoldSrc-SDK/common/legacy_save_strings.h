#pragma once

#include <cstddef>
#include <cstring>

namespace cse { namespace save {

inline bool ValidLegacyStringPayload(const void *bytes, size_t size, int count) {
    if (count < 0 || static_cast<size_t>(count) > size || (size != 0 && !bytes)) {
        return false;
    }
    if (count == 0) {
        return size == 0;
    }
    const auto *cursor = static_cast<const char *>(bytes);
    size_t remaining = size;
    for (int index = 0; index < count; ++index) {
        const auto *terminator = static_cast<const char *>(std::memchr(cursor, '\0', remaining));
        if (!terminator) {
            return false;
        }
        const auto consumed = static_cast<size_t>(terminator - cursor) + 1;
        cursor += consumed;
        remaining -= consumed;
    }
    return remaining == 0;
}

} } // namespace cse::save
