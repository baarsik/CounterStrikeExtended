#pragma once

#include "legacy_save_integer.h"

// Included with the complete game-side EHANDLE and entity declarations.
namespace cse { namespace save {

static_assert(sizeof(Vector) == sizeof(float) * CSE_SAVE_VECTOR_COMPONENTS,
    "Game vectors must retain the shared packed three-float representation");

inline bool ValidFieldStorage(const TYPEDESCRIPTION &field) {
    if (field.fieldType < FIELD_FLOAT || field.fieldType >= FIELD_TYPECOUNT || field.fieldOffset < 0
        || field.fieldSize < 0 || field.nativeStride == 0
        || static_cast<unsigned int>(field.fieldSize) > field.nativeBytes / field.nativeStride) {
        return false;
    }
    const auto bytes = static_cast<size_t>(field.fieldSize) * field.nativeStride;
    if (bytes > INT_MAX) {
        return false;
    }
    switch (field.fieldType) {
    case FIELD_BOOLEAN:
    case FIELD_INTEGER:
        return detail::ValidIntegerFlags(field.nativeStride, field.nativeFlags)
            && (field.nativeStride == sizeof(std::uint8_t) || field.nativeStride == sizeof(std::uint16_t)
                || field.nativeStride == sizeof(std::uint32_t) || field.nativeStride == sizeof(std::uint64_t));
    case FIELD_CLASSPTR:
    case FIELD_EVARS:
    case FIELD_EDICT:
        return field.nativeStride == sizeof(void *);
    case FIELD_POINTER:
        return false;
    case FIELD_EHANDLE:
        return field.nativeStride == sizeof(EHANDLE);
    case FIELD_STRING:
    case FIELD_MODELNAME:
    case FIELD_SOUNDNAME:
        return field.nativeStride == sizeof(string_t);
    case FIELD_VECTOR:
    case FIELD_POSITION_VECTOR:
        return field.nativeStride == sizeof(float) * CSE_SAVE_VECTOR_COMPONENTS;
    case FIELD_FLOAT:
    case FIELD_TIME:
        return field.nativeStride == sizeof(float);
    case FIELD_SHORT:
        return field.nativeStride == sizeof(short);
    case FIELD_CHARACTER:
        return field.nativeStride == sizeof(char);
    case FIELD_ENTITY:
        return field.nativeStride == sizeof(EOFFSET);
    case FIELD_FUNCTION:
        return true; // Its typed scalar category is validated separately.
    default:
        return false;
    }
}

inline bool NativeFieldEmpty(const TYPEDESCRIPTION &field, const void *storage) {
    if (field.fieldType == FIELD_EHANDLE) {
        const auto *handles = static_cast<const EHANDLE *>(storage);
        for (int index = 0; index < field.fieldSize; ++index) {
            if (handles[index].Get()) {
                return false;
            }
        }
        return true;
    }
    const auto *bytes = static_cast<const unsigned char *>(storage);
    const auto count = static_cast<size_t>(field.fieldSize) * field.nativeStride;
    for (size_t index = 0; index < count; ++index) {
        if (bytes[index]) {
            return false;
        }
    }
    return true;
}

inline void ClearNativeField(const TYPEDESCRIPTION &field, void *storage) {
    if (field.fieldType == FIELD_EHANDLE) {
        auto *handles = static_cast<EHANDLE *>(storage);
        for (int index = 0; index < field.fieldSize; ++index) {
            handles[index] = nullptr;
        }
        return;
    }
    // Only native storage spans are cleared. Callback members use typed null
    // operations, never this path; saved integer widths do not determine spans.
    std::memset(storage, 0, static_cast<size_t>(field.fieldSize) * field.nativeStride);
}

} } // namespace cse::save
