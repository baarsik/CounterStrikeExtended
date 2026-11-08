#pragma once

#include "save_field_storage.h"
#include <climits>
#include <cstdint>
#include <cstring>

namespace cse { namespace save {

constexpr size_t LegacyIntegerBytes = sizeof(std::uint32_t);
static_assert(CHAR_BIT == 8 && LegacyIntegerBytes == 4, "Legacy saves use four eight-bit bytes per integer");

namespace detail {

template<typename Type>
Type ReadNativeInteger(const void *storage) {
    Type value;
    std::memcpy(&value, storage, sizeof(value));
    return value;
}

template<typename Type>
void StoreNativeInteger(void *storage, Type value) {
    std::memcpy(storage, &value, sizeof(value));
}

inline bool ReadNativeUnsigned(const void *storage, size_t stride, std::uint64_t &value) {
    switch (stride) {
    case sizeof(std::uint8_t):
        value = ReadNativeInteger<std::uint8_t>(storage);
        return true;
    case sizeof(std::uint16_t):
        value = ReadNativeInteger<std::uint16_t>(storage);
        return true;
    case sizeof(std::uint32_t):
        value = ReadNativeInteger<std::uint32_t>(storage);
        return true;
    case sizeof(std::uint64_t):
        value = ReadNativeInteger<std::uint64_t>(storage);
        return true;
    default:
        return false;
    }
}

inline bool StoreNativeUnsigned(void *storage, size_t stride, std::uint64_t value) {
    switch (stride) {
    case sizeof(std::uint8_t):
        StoreNativeInteger(storage, static_cast<std::uint8_t>(value));
        return true;
    case sizeof(std::uint16_t):
        StoreNativeInteger(storage, static_cast<std::uint16_t>(value));
        return true;
    case sizeof(std::uint32_t):
        StoreNativeInteger(storage, static_cast<std::uint32_t>(value));
        return true;
    case sizeof(std::uint64_t):
        StoreNativeInteger(storage, value);
        return true;
    default:
        return false;
    }
}

inline bool ValidIntegerFlags(size_t stride, unsigned int flags) {
    if (flags & ~(NativeSigned | NativeBoolean)) {
        return false;
    }
    return !(flags & NativeBoolean) || (flags == NativeBoolean && stride == sizeof(bool));
}

} // namespace detail

inline bool EncodeLegacyInteger(const void *storage, size_t stride, unsigned int flags, unsigned char *wire) {
    if (!detail::ValidIntegerFlags(stride, flags)) {
        return false;
    }
    std::uint64_t value = 0;
    if (flags & NativeBoolean) {
        value = detail::ReadNativeInteger<bool>(storage) ? 1 : 0;
    } else {
        if (!detail::ReadNativeUnsigned(storage, stride, value)) {
            return false;
        }
        if ((flags & NativeSigned) && stride < sizeof(value)) {
            const unsigned int bits = static_cast<unsigned int>(stride * CHAR_BIT);
            if (value & (UINT64_C(1) << (bits - 1))) {
                value |= ~UINT64_C(0) << bits;
            }
        }
        if (flags & NativeSigned) {
            constexpr std::int64_t legacyMinimum = -INT64_C(2147483648);
            constexpr std::int64_t legacyMaximum = INT64_C(2147483647);
            std::int64_t signedValue;
            std::memcpy(&signedValue, &value, sizeof(signedValue));
            if (signedValue < legacyMinimum || signedValue > legacyMaximum) {
                return false;
            }
        } else if (value > UINT32_MAX) {
            return false;
        }
    }
    for (size_t byte = 0; byte < LegacyIntegerBytes; ++byte) {
        wire[byte] = static_cast<unsigned char>(value & UINT8_MAX);
        value >>= CHAR_BIT;
    }
    return true;
}

inline bool DecodeLegacyInteger(const unsigned char *wire, void *storage, size_t stride, unsigned int flags) {
    if (!detail::ValidIntegerFlags(stride, flags)) {
        return false;
    }
    if (flags & NativeBoolean) {
        // Legacy bool metadata wrote adjacent bytes too. Only the low byte was
        // the original bool; upper bytes must not turn false into true.
        const bool value = wire[0] != 0;
        std::memcpy(storage, &value, sizeof(value));
        return true;
    }
    std::uint64_t value = 0;
    for (size_t byte = 0; byte < LegacyIntegerBytes; ++byte) {
        value |= static_cast<std::uint64_t>(wire[byte]) << (byte * CHAR_BIT);
    }
    if ((flags & NativeSigned) && stride > LegacyIntegerBytes && (value & UINT32_C(0x80000000))) {
        value |= ~UINT64_C(0) << (LegacyIntegerBytes * CHAR_BIT);
    }
    // Narrow fields retain their original low bytes, ignoring the unrelated
    // high bytes legacy FIELD_INTEGER captured next to a short/byte member.
    return detail::StoreNativeUnsigned(storage, stride, value);
}

} } // namespace cse::save
