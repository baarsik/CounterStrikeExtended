#pragma once

// Private disk-input preflight. Runtime model structures and published interfaces stay unchanged.
#include "zone.h"

#include <climits>
#include <cmath>
#include <cstddef>
#include <cstring>

#if defined(SW_RENDERER)
extern int r_pixbytes;
#endif

namespace ModelLoading {
constexpr std::size_t PaletteEntries = 256;
constexpr std::size_t PaletteChannels = 3;
constexpr std::size_t PaletteBytes = PaletteEntries * PaletteChannels;
constexpr std::size_t ExpandedPaletteChannels = 4;
constexpr std::size_t ExpandedPaletteBytes = PaletteEntries * ExpandedPaletteChannels * sizeof(word);

template<class T> inline T Read(const void* data) {
    T value;
    std::memcpy(&value, data, sizeof(value));
    return value;
}

inline bool Range(std::size_t bytes, int offset, std::size_t length) {
    return offset >= 0 && static_cast<std::size_t>(offset) <= bytes && length <= bytes - offset;
}

inline bool NativeAllocation(std::size_t base, int count, std::size_t element,
                             std::size_t maximumPayload = cse::memory::MaximumHunkPayloadBytes) {
    return count >= 0 && base <= maximumPayload &&
        static_cast<std::size_t>(count) <= (maximumPayload - base) / element;
}

inline bool WritableModelFileSize(unsigned int fileBytes) {
    constexpr std::size_t TrailingByteReservation = 1;
    return fileBytes >= sizeof(int) &&
        fileBytes <= cse::memory::MaximumHunkPayloadBytes - TrailingByteReservation;
}

inline bool Pixels(int width, int height, std::size_t& pixels) {
    if (width <= 0 || height <= 0 || width > INT_MAX / height) {
        return false;
    }
    pixels = static_cast<std::size_t>(width) * height;
    return true;
}

inline bool Studio(const void* buffer, int fileBytes, studiohdr_t& header, int& allocationBytes) {
    if (!buffer || fileBytes < static_cast<int>(sizeof(header))) {
        return false;
    }
    header = Read<studiohdr_t>(buffer);
    if (LittleLong(header.version) != STUDIO_VERSION) {
        // A complete unsupported-version header retains the historical bogus-model fallback.
        header = {};
        std::memcpy(header.name, "bogus", sizeof("bogus"));
        header.length = sizeof(header);
        header.texturedataindex = sizeof(header);
        allocationBytes = sizeof(header);
        return NativeAllocation(0, allocationBytes, 1, cse::memory::MaximumCachePayloadBytes);
    }
    if (header.length < static_cast<int>(sizeof(header)) || header.length > fileBytes ||
        header.numtextures < 0 || header.textureindex < 0 || header.texturedataindex < 0 ||
        header.texturedataindex > header.length) {
        return false;
    }
    allocationBytes = header.length;
    if (!header.textureindex) {
        return header.numtextures == 0 &&
            NativeAllocation(0, allocationBytes, 1, cse::memory::MaximumCachePayloadBytes);
    }
    if (header.texturedataindex < static_cast<int>(sizeof(header)) ||
        header.texturedataindex > header.length || header.textureindex < static_cast<int>(sizeof(header)) ||
        !NativeAllocation(0, header.numtextures, sizeof(mstudiotexture_t)) ||
        !Range(header.texturedataindex, header.textureindex,
            static_cast<std::size_t>(header.numtextures) * sizeof(mstudiotexture_t))) {
        return false;
    }
    const byte* data = static_cast<const byte*>(buffer);
#if defined(SW_RENDERER)
    constexpr std::size_t PaletteExpansion = ExpandedPaletteBytes - PaletteBytes;
    if (!NativeAllocation(header.length, header.numtextures, PaletteExpansion, cse::memory::MaximumCachePayloadBytes)) {
        return false;
    }
    allocationBytes = static_cast<int>(header.length +
        static_cast<std::size_t>(header.numtextures) * PaletteExpansion);
    std::size_t sequentialOffset = header.texturedataindex;
#else
    allocationBytes = header.texturedataindex;
    if (!NativeAllocation(0, allocationBytes, 1, cse::memory::MaximumCachePayloadBytes)) {
        return false;
    }
#endif
    for (int i = 0; i < header.numtextures; ++i) {
        const mstudiotexture_t texture = Read<mstudiotexture_t>(data + header.textureindex +
            static_cast<std::size_t>(i) * sizeof(mstudiotexture_t));
        std::size_t pixels;
        if (!Pixels(texture.width, texture.height, pixels) ||
            !Range(header.length, texture.index, pixels + PaletteBytes)) {
            return false;
        }
#if defined(SW_RENDERER)
        // Software consumes blocks sequentially, independently of each disk texture's index.
        if (sequentialOffset > static_cast<std::size_t>(header.length) ||
            pixels + PaletteBytes > header.length - sequentialOffset) {
            return false;
        }
        sequentialOffset += pixels + PaletteBytes;
#else
        // Uploads mutate palettes in place, so texture data must not alias retained metadata.
        if (texture.index < header.texturedataindex) {
            return false;
        }
#endif
    }
    return true;
}

class SpriteCursor {
public:
    SpriteCursor(const void* data, int bytes) : data_(static_cast<const byte*>(data)), bytes_(bytes) {}

    template<class T> bool Take(T& record) {
        if (!Skip(sizeof(record))) {
            return false;
        }
        record = Read<T>(data_ + offset_ - sizeof(record));
        return true;
    }

    bool Skip(std::size_t count) {
        if (count > bytes_ - offset_) {
            return false;
        }
        offset_ += count;
        return true;
    }

    bool Frame() {
        dspriteframe_t frame;
        std::size_t pixels;
        if (!Take(frame) || !Pixels(LittleLong(frame.width), LittleLong(frame.height), pixels)) {
            return false;
        }
#if defined(SW_RENDERER)
        if ((r_pixbytes != 1 && r_pixbytes != 2) ||
            !NativeAllocation(sizeof(mspriteframe_t), static_cast<int>(pixels), r_pixbytes)) {
            return false;
        }
#endif
        return Skip(pixels);
    }

private:
    const byte* data_;
    std::size_t bytes_;
    std::size_t offset_ = 0;
};

inline bool Sprite(const void* buffer, int fileBytes) {
    if (!buffer || fileBytes < static_cast<int>(sizeof(dsprite_t))) {
        return false;
    }
    SpriteCursor cursor(buffer, fileBytes);
    dsprite_t header;
    unsigned short diskEntries;
    std::size_t pixels;
    if (!cursor.Take(header) || LittleLong(header.version) != SPRITE_VERSION ||
        !Pixels(LittleLong(header.width), LittleLong(header.height), pixels) ||
        LittleLong(header.numframes) <= 0 || !cursor.Take(diskEntries)) {
        return false;
    }
    // Sprite palettes have a little-endian 16-bit count. Supported targets are little-endian.
    const int entries = diskEntries;
#if defined(SW_RENDERER)
    constexpr int MinimumPaletteEntries = 1;
    constexpr std::size_t NativePaletteEntryBytes = sizeof(PackedColorVec);
#else
    // Hardware always reads a full 256-color upload palette.
    constexpr int MinimumPaletteEntries = PaletteEntries;
    constexpr std::size_t NativePaletteEntryBytes = PaletteChannels;
#endif
    if (entries < MinimumPaletteEntries || entries > SHRT_MAX ||
        !cursor.Skip(static_cast<std::size_t>(entries) * PaletteChannels)) {
        return false;
    }
    const int syncType = LittleLong(Read<int>(static_cast<const byte*>(buffer) + offsetof(dsprite_t, synctype)));
    if (syncType != ST_SYNC && syncType != ST_RAND) {
        return false;
    }
    const int count = LittleLong(header.numframes);
    const std::size_t paletteBytes = sizeof(diskEntries) + entries * NativePaletteEntryBytes;
    if (!NativeAllocation(sizeof(msprite_t) + paletteBytes, count - 1, sizeof(mspriteframedesc_t))) {
        return false;
    }
    for (int i = 0; i < count; ++i) {
        int type;
        if (!cursor.Take(type)) {
            return false;
        }
        if (LittleLong(type) == SPR_SINGLE) {
            if (!cursor.Frame()) {
                return false;
            }
        } else if (LittleLong(type) == SPR_GROUP) {
            dspritegroup_t group;
            if (!cursor.Take(group)) {
                return false;
            }
            const int frames = LittleLong(group.numframes);
            if (frames <= 0 || !NativeAllocation(sizeof(mspritegroup_t), frames - 1, sizeof(mspriteframe_t*)) ||
                !NativeAllocation(0, frames, sizeof(float))) {
                return false;
            }
#if !defined(SW_RENDERER)
            // Retain the historical group texture-number naming without signed arithmetic overflow.
            constexpr int GroupFrameScale = 100;
            if (i > (INT_MAX - (frames - 1)) / GroupFrameScale) {
                return false;
            }
#endif
            for (int j = 0; j < frames; ++j) {
                dspriteinterval_t interval;
                if (!cursor.Take(interval)) {
                    return false;
                }
                const float seconds = LittleFloat(interval.interval);
                if (!std::isfinite(seconds) || seconds <= 0.0f) {
                    return false;
                }
            }
            for (int j = 0; j < frames; ++j) {
                if (!cursor.Frame()) {
                    return false;
                }
            }
        } else {
            return false;
        }
    }
    return true;
}
} // namespace ModelLoading
