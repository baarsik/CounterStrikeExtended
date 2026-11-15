#pragma once

// Private BSP preflight; runtime structures and published model interfaces are unchanged.
// Range and iterative graph checks informed by CS16Reconstruction e5c25197f,
// engine/src/assets.cpp and engine/src/brush_models.cpp. Disk layouts remain owned by bspfile.h.
#include "bspfile.h"
#include "engine_studio.h"
#include "model_loading.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <iterator>
#include <vector>

namespace BrushModelLoading {
constexpr int Dimensions = 3;
constexpr int TextureAxes = 2;
constexpr int NodeChildren = 2;
constexpr int MinimumFaceEdges = 3;
constexpr int NoOffset = -1;
constexpr int LightmapSpacing = 16;
constexpr int MaximumPlaneType = PLANE_ANYZ;
constexpr int MaximumSignedChildren = static_cast<int>(SHRT_MAX) + 1;
constexpr int PlaneCopies = 2;
constexpr int StyleTerminator = 255;
constexpr int MipScale = 64 + 16 + 4 + 1;
constexpr int MipDivisor = 64;

inline bool Range(std::size_t capacity, std::int64_t first, std::int64_t count) {
    return first >= 0 && count >= 0 && static_cast<std::uint64_t>(first) <= capacity &&
           static_cast<std::uint64_t>(count) <= capacity - static_cast<std::size_t>(first);
}

template <class T> inline T Record(const byte* base, const lump_t& lump, int index) {
    return ModelLoading::Read<T>(base + lump.fileofs + static_cast<std::size_t>(index) * sizeof(T));
}

// Stock maps can wrap texture origins outside int16 (for example cs_havana).
// Preserve their low bits while rejecting unrepresentable extents before narrowing.
inline bool Extents(const float* minimum, const float* maximum, short* origins, short* extents) {
    for (int axis = 0; axis < TextureAxes; ++axis) {
        const double low = std::floor(minimum[axis] / LightmapSpacing);
        const double high = std::ceil(maximum[axis] / LightmapSpacing);
        const double extent = (high - low) * LightmapSpacing;
        if (!std::isfinite(low) || !std::isfinite(high) || low < INT_MIN || low > INT_MAX || high < INT_MIN ||
            high > INT_MAX || extent < 0 || extent > SHRT_MAX) {
            return false;
        }
        constexpr std::uint32_t OriginMask = UINT16_MAX;
        constexpr int OriginModulus = UINT16_MAX + 1;
        const auto bits = (static_cast<std::uint32_t>(static_cast<int>(low)) * LightmapSpacing) & OriginMask;
        origins[axis] =
            static_cast<short>(bits <= SHRT_MAX ? static_cast<int>(bits) : static_cast<int>(bits) - OriginModulus);
        extents[axis] = static_cast<short>(extent);
    }
    return true;
}

inline bool Miptex(const byte* bytes, std::size_t size) {
    if (size < sizeof(miptex_t)) {
        return false;
    }
    const miptex_t texture = ModelLoading::Read<miptex_t>(bytes);
    if (!std::memchr(texture.name, 0, sizeof(texture.name))) {
        return false;
    }
    if (!texture.offsets[0]) {
        return true; // An external WAD reference has no embedded pixels or palette.
    }
    const int width = LittleLong(texture.width);
    const int height = LittleLong(texture.height);
    std::size_t pixels;
    if (!ModelLoading::Pixels(width, height, pixels) || width % LightmapSpacing || height % LightmapSpacing) {
        return false;
    }
    const std::size_t mipBytes = pixels / MipDivisor * MipScale;
    for (int level = 0; level < MIPLEVELS; ++level) {
        const int offset = LittleLong(texture.offsets[level]);
        if (offset < static_cast<int>(sizeof(texture)) ||
            !Range(mipBytes, static_cast<std::int64_t>(offset) - sizeof(texture), pixels >> (level * 2))) {
            return false;
        }
    }
    const std::size_t paletteOffset = sizeof(texture) + mipBytes;
    if (!Range(size, paletteOffset, sizeof(unsigned short))) {
        return false;
    }
    const auto entries =
        static_cast<unsigned short>(LittleShort(ModelLoading::Read<unsigned short>(bytes + paletteOffset)));
    // Hardware palette conversion reads all 256 entries, even when the count is smaller.
    return entries >= ModelLoading::PaletteEntries &&
           Range(size, sizeof(texture), mipBytes + sizeof(entries) + entries * ModelLoading::PaletteChannels) &&
           ModelLoading::NativeAllocation(sizeof(texture_t) + mipBytes + sizeof(entries), entries,
                                          sizeof(PackedColorVec));
}

template <class Children> bool Graph(int count, Children children) {
    enum class Visit : byte { New, Active, Complete };
    struct Frame {
        int index;
        int child;
    };
    std::vector<Visit> visited(static_cast<std::size_t>(count), Visit::New);
    std::vector<Frame> stack;
    for (int root = 0; root < count; ++root) {
        if (visited[root] != Visit::New) {
            continue;
        }
        stack.push_back({root, 0});
        visited[root] = Visit::Active;
        while (!stack.empty()) {
            Frame& current = stack.back();
            if (current.child == NodeChildren) {
                visited[current.index] = Visit::Complete;
                stack.pop_back();
                continue;
            }
            const int child = children(current.index, current.child++);
            if (child < 0) {
                continue;
            }
            if (visited[child] == Visit::Active) {
                return false;
            }
            if (visited[child] == Visit::New) {
                visited[child] = Visit::Active;
                stack.push_back({child, 0});
            }
        }
    }
    return true;
}

inline const char* Check(const void* data, int fileBytes, bool blueShift) {
    if (!data || fileBytes < static_cast<int>(sizeof(dheader_t))) {
        return "truncated header";
    }
    const auto* base = static_cast<const byte*>(data);
    dheader_t header = ModelLoading::Read<dheader_t>(base);
    for (lump_t& lump : header.lumps) {
        lump.fileofs = LittleLong(lump.fileofs);
        lump.filelen = LittleLong(lump.filelen);
        if (!Range(fileBytes, lump.fileofs, lump.filelen)) {
            return "lump outside file";
        }
    }
    if (blueShift) {
        std::swap(header.lumps[LUMP_PLANES], header.lumps[LUMP_ENTITIES]);
    }
    constexpr std::array<std::size_t, HEADER_LUMPS> DiskSizes = {1,
                                                                 sizeof(dplane_t),
                                                                 1,
                                                                 sizeof(dvertex_t),
                                                                 1,
                                                                 sizeof(dnode_t),
                                                                 sizeof(texinfo_t),
                                                                 sizeof(dface_t),
                                                                 1,
                                                                 sizeof(dclipnode_t),
                                                                 sizeof(dleaf_t),
                                                                 sizeof(unsigned short),
                                                                 sizeof(dedge_t),
                                                                 sizeof(int),
                                                                 sizeof(dmodel_t)};
    constexpr std::array<std::size_t, HEADER_LUMPS> NativeSizes = {1,
                                                                   sizeof(mplane_t) * PlaneCopies,
                                                                   1,
                                                                   sizeof(mvertex_t),
                                                                   1,
                                                                   sizeof(mnode_t),
                                                                   sizeof(mtexinfo_t),
                                                                   sizeof(msurface_t),
                                                                   1,
                                                                   sizeof(dclipnode_t),
                                                                   sizeof(mleaf_t),
                                                                   sizeof(msurface_t*),
                                                                   sizeof(medge_t),
                                                                   sizeof(int),
                                                                   sizeof(dmodel_t)};
    std::array<int, HEADER_LUMPS> counts{};
    for (int index = 0; index < HEADER_LUMPS; ++index) {
        const auto length = static_cast<std::size_t>(header.lumps[index].filelen);
        counts[index] = static_cast<int>(length / DiskSizes[index]);
        const std::size_t extra = index == LUMP_EDGES ? sizeof(medge_t) : 0;
        if (length % DiskSizes[index] || !ModelLoading::NativeAllocation(extra, counts[index], NativeSizes[index])) {
            return "partial record or oversized allocation";
        }
    }
    const auto& lumps = header.lumps;
    const int nodes = counts[LUMP_NODES];
    const int leafs = counts[LUMP_LEAFS];
    if (!counts[LUMP_MODELS] || !nodes || !leafs || nodes > MaximumSignedChildren ||
        counts[LUMP_CLIPNODES] > MaximumSignedChildren || leafs > MaximumSignedChildren) {
        return "invalid model or graph count";
    }
    const lump_t& entities = lumps[LUMP_ENTITIES];
    if (entities.filelen && !std::memchr(base + entities.fileofs, 0, entities.filelen)) {
        return "unterminated entities";
    }
    int textures = 0;
    const lump_t& textureLump = lumps[LUMP_TEXTURES];
    if (textureLump.filelen) {
        if (textureLump.filelen < static_cast<int>(sizeof(int))) {
            return "truncated texture directory";
        }
        const byte* bytes = base + textureLump.fileofs;
        textures = LittleLong(ModelLoading::Read<int>(bytes));
        if (!Range(textureLump.filelen, sizeof(int), static_cast<std::int64_t>(textures) * sizeof(int)) ||
            !ModelLoading::NativeAllocation(0, textures, sizeof(texture_t*))) {
            return "invalid texture directory";
        }
        const std::size_t tableBytes = sizeof(int) + static_cast<std::size_t>(textures) * sizeof(int);
        for (int index = 0; index < textures; ++index) {
            const int offset = LittleLong(ModelLoading::Read<int>(bytes + sizeof(int) + index * sizeof(int)));
            if (offset == NoOffset) {
                continue;
            }
            if (offset < 0 || static_cast<std::size_t>(offset) < tableBytes ||
                !Range(textureLump.filelen, offset, sizeof(miptex_t)) ||
                !Miptex(bytes + offset, textureLump.filelen - offset)) {
                return "invalid embedded texture";
            }
        }
    }
    for (int index = 0; index < counts[LUMP_VERTEXES]; ++index) {
        const auto vertex = Record<dvertex_t>(base, lumps[LUMP_VERTEXES], index);
        for (float coordinate : vertex.point) {
            if (!std::isfinite(coordinate)) {
                return "nonfinite vertex";
            }
        }
    }
    for (int index = 0; index < counts[LUMP_PLANES]; ++index) {
        const auto plane = Record<dplane_t>(base, lumps[LUMP_PLANES], index);
        if (plane.type < PLANE_X || plane.type > MaximumPlaneType || !std::isfinite(plane.dist)) {
            return "invalid plane";
        }
        for (float component : plane.normal) {
            if (!std::isfinite(component)) {
                return "nonfinite plane";
            }
        }
    }
    for (int index = 0; index < counts[LUMP_EDGES]; ++index) {
        const auto edge = Record<dedge_t>(base, lumps[LUMP_EDGES], index);
        for (unsigned short vertex : edge.v) {
            if (vertex >= counts[LUMP_VERTEXES]) {
                return "invalid edge vertex";
            }
        }
    }
    for (int index = 0; index < counts[LUMP_SURFEDGES]; ++index) {
        const auto edge = static_cast<std::int64_t>(Record<int>(base, lumps[LUMP_SURFEDGES], index));
        if (!Range(counts[LUMP_EDGES], edge < 0 ? -edge : edge, 1)) {
            return "invalid surface edge";
        }
    }
    for (int index = 0; index < counts[LUMP_TEXINFO]; ++index) {
        const auto texture = Record<texinfo_t>(base, lumps[LUMP_TEXINFO], index);
        if (textureLump.filelen && !Range(textures, texture.miptex, 1)) {
            return "invalid texture reference";
        }
        for (const auto& axis : texture.vecs) {
            for (float component : axis) {
                if (!std::isfinite(component)) {
                    return "nonfinite texture projection";
                }
            }
        }
    }
    for (int index = 0; index < counts[LUMP_FACES]; ++index) {
        const auto face = Record<dface_t>(base, lumps[LUMP_FACES], index);
        if (!Range(counts[LUMP_PLANES], face.planenum, 1) || !Range(counts[LUMP_TEXINFO], face.texinfo, 1) ||
            face.numedges < MinimumFaceEdges || !Range(counts[LUMP_SURFEDGES], face.firstedge, face.numedges)) {
            return "invalid face reference";
        }
        const auto texture = Record<texinfo_t>(base, lumps[LUMP_TEXINFO], face.texinfo);
        std::array<float, TextureAxes> minimum;
        std::array<float, TextureAxes> maximum;
        // Keep CalcSurfaceExtents' historical initial values.
        constexpr float MinimumProjectionSeed = 999999.0f;
        constexpr float MaximumProjectionSeed = -99999.0f;
        minimum.fill(MinimumProjectionSeed);
        maximum.fill(MaximumProjectionSeed);
        for (int number = 0; number < face.numedges; ++number) {
            const int edgeIndex = Record<int>(base, lumps[LUMP_SURFEDGES], face.firstedge + number);
            const auto edge = Record<dedge_t>(base, lumps[LUMP_EDGES], edgeIndex < 0 ? -edgeIndex : edgeIndex);
            const auto vertex = Record<dvertex_t>(base, lumps[LUMP_VERTEXES], edge.v[edgeIndex < 0 ? 1 : 0]);
            for (int axis = 0; axis < TextureAxes; ++axis) {
                const auto* row = texture.vecs[axis];
                const float value = static_cast<float>(vertex.point[0] * static_cast<double>(row[0]) +
                                                       vertex.point[1] * static_cast<double>(row[1]) +
                                                       vertex.point[2] * static_cast<double>(row[2]) + row[3]);
                if (!std::isfinite(value)) {
                    return "texture projection overflow";
                }
                minimum[axis] = (std::min)(minimum[axis], value);
                maximum[axis] = (std::max)(maximum[axis], value);
            }
        }
        short origins[TextureAxes];
        short extents[TextureAxes];
        if (!Extents(minimum.data(), maximum.data(), origins, extents)) {
            return "unrepresentable surface extents";
        }
        if (face.lightofs != NoOffset) {
            std::size_t styles = 0;
            while (styles < std::size(face.styles) && face.styles[styles] != StyleTerminator) {
                ++styles;
            }
            const std::size_t samples = (extents[0] / LightmapSpacing + 1) * (extents[1] / LightmapSpacing + 1) *
                                        ModelLoading::PaletteChannels * styles;
            if (!Range(lumps[LUMP_LIGHTING].filelen, face.lightofs, (std::max)(samples, std::size_t{1}))) {
                return "light samples outside lump";
            }
        }
    }
    for (int index = 0; index < counts[LUMP_MARKSURFACES]; ++index) {
        if (Record<unsigned short>(base, lumps[LUMP_MARKSURFACES], index) >= counts[LUMP_FACES]) {
            return "invalid marked surface";
        }
    }
    for (int index = 0; index < leafs; ++index) {
        const auto leaf = Record<dleaf_t>(base, lumps[LUMP_LEAFS], index);
        if ((index == 0 && leaf.contents != CONTENTS_SOLID) || leaf.contents >= 0 || leaf.contents < SHRT_MIN ||
            !Range(counts[LUMP_MARKSURFACES], leaf.firstmarksurface, leaf.nummarksurfaces) ||
            (leaf.visofs != NoOffset && !Range(lumps[LUMP_VISIBILITY].filelen, leaf.visofs, 1))) {
            return "invalid leaf reference";
        }
    }
    std::vector<byte> parents(static_cast<std::size_t>(nodes), 0);
    for (int index = 0; index < nodes; ++index) {
        const auto node = Record<dnode_t>(base, lumps[LUMP_NODES], index);
        if (!Range(counts[LUMP_PLANES], node.planenum, 1) ||
            !Range(counts[LUMP_FACES], node.firstface, node.numfaces)) {
            return "invalid node reference";
        }
        for (short child : node.children) {
            if (child >= 0 ? child >= nodes : -1 - child >= leafs) {
                return "invalid node child";
            }
            if (child >= 0 && ++parents[child] > 1) {
                return "shared drawing node has ambiguous parent";
            }
        }
    }
    for (int index = 0; index < counts[LUMP_CLIPNODES]; ++index) {
        const auto node = Record<dclipnode_t>(base, lumps[LUMP_CLIPNODES], index);
        if (!Range(counts[LUMP_PLANES], node.planenum, 1)) {
            return "invalid clip plane";
        }
        for (short child : node.children) {
            if (child >= counts[LUMP_CLIPNODES]) {
                return "invalid clip child";
            }
        }
    }
    for (int index = 0; index < counts[LUMP_MODELS]; ++index) {
        const auto model = Record<dmodel_t>(base, lumps[LUMP_MODELS], index);
        if (!Range(nodes, model.headnode[0], 1) || !Range(counts[LUMP_FACES], model.firstface, model.numfaces) ||
            model.visleafs < 0 || model.visleafs > MAX_MAP_LEAFS || model.visleafs >= leafs) {
            return "invalid submodel reference";
        }
        for (int hull = 1; hull < MAX_MAP_HULLS; ++hull) {
            // Stock c3a2c uses the one-past root for empty inline hulls. No
            // clipnode is read while publishing these roots; retain that sentinel.
            if (model.headnode[hull] > counts[LUMP_CLIPNODES]) {
                return "invalid submodel clip root";
            }
        }
        for (int axis = 0; axis < Dimensions; ++axis) {
            if (!std::isfinite(model.mins[axis]) || !std::isfinite(model.maxs[axis]) ||
                !std::isfinite(model.origin[axis])) {
                return "nonfinite submodel bounds";
            }
        }
    }
    if (!Graph(nodes,
               [&](int node, int side) {
                   return Record<dnode_t>(base, lumps[LUMP_NODES], node).children[side];
               }) ||
        !Graph(counts[LUMP_CLIPNODES], [&](int node, int side) {
            return Record<dclipnode_t>(base, lumps[LUMP_CLIPNODES], node).children[side];
        })) {
        return "cyclic BSP graph";
    }
    // Inline models can add leaves after the world's PVS domain (stock datacore).
    // Only non-solid leaves reachable from the world root index that world's row.
    const auto world = Record<dmodel_t>(base, lumps[LUMP_MODELS], 0);
    std::vector<int> pending{0}; // Rendering/point queries start at node zero.
    if (world.headnode[0] != 0) {
        pending.push_back(world.headnode[0]); // Hull queries also consume the declared root.
    }
    while (!pending.empty()) {
        const int node = pending.back();
        pending.pop_back();
        if (node < 0) {
            const int index = -1 - node;
            const auto leaf = Record<dleaf_t>(base, lumps[LUMP_LEAFS], index);
            if (leaf.contents != CONTENTS_SOLID && index > world.visleafs) {
                return "world leaf outside visibility row";
            }
        } else {
            const auto entry = Record<dnode_t>(base, lumps[LUMP_NODES], node);
            pending.push_back(entry.children[1]);
            pending.push_back(entry.children[0]);
        }
    }
    return nullptr;
}
} // namespace BrushModelLoading
