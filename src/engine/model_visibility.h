#pragma once

// Private brush visibility extents; model_t and the published engine interfaces remain unchanged.
#include "gl_model.h"
#include "zone.h"

#include <cstddef>
#include <cstdint>
#include <cstring>

struct ModelVisibilityInfo {
    std::size_t visBytes = 0;
    int leafCount = 0;
};

ModelVisibilityInfo Mod_GetVisibilityInfo(const model_t* model);

namespace ModelVisibility {
constexpr int BitsPerByte = 8;
constexpr byte AllVisible = 0xff;
constexpr int NoVisibilityOffset = -1;
constexpr std::size_t MaximumRowBytes = MAX_MAP_LEAFS / BitsPerByte;

inline bool RowBytes(int visibleLeafCount, std::size_t& bytes) {
    if (visibleLeafCount < 0 || visibleLeafCount > MAX_MAP_LEAFS) {
        return false;
    }
    bytes = (static_cast<std::size_t>(visibleLeafCount) + BitsPerByte - 1) / BitsPerByte;
    return bytes <= MaximumRowBytes;
}

inline bool LumpRange(int fileBytes, const lump_t& lump) {
    return fileBytes >= 0 && lump.fileofs >= 0 && lump.filelen >= 0 && lump.fileofs <= fileBytes &&
        lump.filelen <= fileBytes - lump.fileofs;
}

inline bool BrushLumps(const void* buffer, int fileBytes) {
    if (!buffer || fileBytes < static_cast<int>(sizeof(dheader_t))) {
        return false;
    }
    dheader_t header;
    std::memcpy(&header, buffer, sizeof(header));
    lump_t visibility = header.lumps[LUMP_VISIBILITY];
    visibility.fileofs = LittleLong(visibility.fileofs);
    visibility.filelen = LittleLong(visibility.filelen);
    lump_t leafs = header.lumps[LUMP_LEAFS];
    leafs.fileofs = LittleLong(leafs.fileofs);
    leafs.filelen = LittleLong(leafs.filelen);
    return LumpRange(fileBytes, visibility) && LumpRange(fileBytes, leafs) &&
        static_cast<std::size_t>(visibility.filelen) <= cse::memory::MaximumHunkPayloadBytes &&
        leafs.filelen % sizeof(dleaf_t) == 0 && leafs.filelen / sizeof(dleaf_t) > 0 &&
        leafs.filelen / sizeof(dleaf_t) <= cse::memory::MaximumHunkPayloadBytes / sizeof(mleaf_t);
}

inline bool SourceBytes(const model_t* model, const byte* input, const ModelVisibilityInfo& info,
                        std::size_t& bytes) {
    bytes = 0;
    if (!input) {
        return true;
    }
    const std::uintptr_t start = reinterpret_cast<std::uintptr_t>(model->visdata);
    const std::uintptr_t address = reinterpret_cast<std::uintptr_t>(input);
    if (!model->visdata || address < start || address - start >= info.visBytes) {
        return false;
    }
    bytes = info.visBytes - (address - start);
    return true;
}

inline bool LeafIndex(const model_t* model, const mleaf_t* leaf, const ModelVisibilityInfo& info, int& index) {
    const std::uintptr_t start = reinterpret_cast<std::uintptr_t>(model->leafs);
    const std::uintptr_t address = reinterpret_cast<std::uintptr_t>(leaf);
    if (!model->leafs || info.leafCount <= 0 || address < start ||
        (address - start) % sizeof(mleaf_t) != 0 || (address - start) / sizeof(mleaf_t) >=
            static_cast<std::size_t>(info.leafCount)) {
        return false;
    }
    index = static_cast<int>((address - start) / sizeof(mleaf_t));
    return true;
}
} // namespace ModelVisibility
