#ifndef CSE_LEGACY_GRAPH_RECORDS_H
#define CSE_LEGACY_GRAPH_RECORDS_H

#include <cstddef>
#include <cstdint>
#include <cstring>

#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ != __ORDER_LITTLE_ENDIAN__
#error The retained Win32 navigation codec requires a little-endian target
#endif

namespace cse {
namespace navigation {

constexpr std::int32_t LegacyGraphVersion = 16;
constexpr std::size_t GraphRangeCount = 256;
constexpr std::size_t GraphCacheCount = 128;
constexpr std::size_t GraphHashPrimeCount = 16;
constexpr std::size_t GraphHullCount = 4;

#pragma pack(push, 4)
struct LegacyGraphCache {
    float v[3];
    std::int16_t n;
    std::uint16_t padding;
};

struct LegacyGraphHeader {
    std::int32_t m_fGraphPresent;
    std::int32_t m_fGraphPointersSet;
    std::int32_t m_fRoutingComplete;
    std::uint32_t nodesPointerPlaceholder;
    std::uint32_t linksPointerPlaceholder;
    std::uint32_t routesPointerPlaceholder;
    std::int32_t m_cNodes;
    std::int32_t m_cLinks;
    std::int32_t m_nRouteInfo;
    std::uint32_t distancePointerPlaceholder;
    std::int32_t m_RangeStart[3][GraphRangeCount];
    std::int32_t m_RangeEnd[3][GraphRangeCount];
    float m_flShortest;
    std::int32_t m_iNearest;
    std::int32_t m_minX;
    std::int32_t m_minY;
    std::int32_t m_minZ;
    std::int32_t m_maxX;
    std::int32_t m_maxY;
    std::int32_t m_maxZ;
    std::int32_t m_minBoxX;
    std::int32_t m_minBoxY;
    std::int32_t m_minBoxZ;
    std::int32_t m_maxBoxX;
    std::int32_t m_maxBoxY;
    std::int32_t m_maxBoxZ;
    std::int32_t m_CheckedCounter;
    float m_RegionMin[3];
    float m_RegionMax[3];
    LegacyGraphCache m_Cache[GraphCacheCount];
    std::int32_t m_HashPrimes[GraphHashPrimeCount];
    std::uint32_t hashPointerPlaceholder;
    std::int32_t m_nHashLinks;
    std::int32_t m_iLastActiveIdleSearch;
    std::int32_t m_iLastCoverSearch;
};

struct LegacyGraphNode {
    float m_vecOrigin[3];
    float m_vecOriginPeek[3];
    std::uint8_t m_Region[3];
    std::uint8_t regionPadding;
    std::int32_t m_afNodeInfo;
    std::int32_t m_cNumLinks;
    std::int32_t m_iFirstLink;
    std::int32_t m_pNextBestNode[GraphHullCount][2];
    float m_flClosestSoFar;
    std::int32_t m_iPreviousNode;
    std::int16_t m_sHintType;
    std::int16_t m_sHintActivity;
    float m_flHintYaw;
};

struct LegacyGraphLink {
    std::int32_t m_iSrcNode;
    std::int32_t m_iDestNode;
    std::uint32_t entityPointerPlaceholder;
    char m_szLinkEntModelname[4];
    std::int32_t m_afLinkInfo;
    float m_flWeight;
};

struct LegacyGraphDistance {
    std::int32_t m_SortedBy[3];
    std::int32_t m_CheckedEvent;
};
#pragma pack(pop)

static_assert(sizeof(float) == 4 && sizeof(short) == 2, "Win32 graph value widths changed");
static_assert(sizeof(LegacyGraphHeader) == 8396, "Win32 CGraph file stride changed");
static_assert(sizeof(LegacyGraphNode) == 88, "Win32 CNode file stride changed");
static_assert(sizeof(LegacyGraphLink) == 24, "Win32 CLink file stride changed");
static_assert(sizeof(LegacyGraphDistance) == 16, "Win32 distance file stride changed");
static_assert(offsetof(LegacyGraphHeader, m_cNodes) == 24, "Win32 graph counts moved");

struct GraphFileView {
    const unsigned char *header = nullptr;
    const unsigned char *nodes = nullptr;
    const unsigned char *links = nullptr;
    const unsigned char *distances = nullptr;
    const unsigned char *routes = nullptr;
    const unsigned char *hashes = nullptr;
    std::int32_t nodeCount = 0;
    std::int32_t linkCount = 0;
    std::int32_t routeBytes = 0;
    std::int32_t hashCount = 0;
    std::size_t consumedBytes = 0;
};

namespace detail {
template<typename T>
inline T GraphValue(const void *bytes) {
    T value;
    std::memcpy(&value, bytes, sizeof(value));
    return value;
}

template<typename Destination, typename Source>
inline void CopyGraphBits(Destination &destination, const Source &source) {
    static_assert(sizeof(destination) == sizeof(source), "Native graph value width differs from Win32");
    std::memcpy(&destination, &source, sizeof(destination));
}

inline bool GraphSegment(const unsigned char *&cursor, std::size_t &remaining,
    std::int32_t count, std::size_t stride, const unsigned char *&segment) {
    if (count < 0 || static_cast<std::size_t>(count) > remaining / stride) {
        return false;
    }
    const auto bytes = static_cast<std::size_t>(count) * stride;
    segment = cursor;
    cursor += bytes;
    remaining -= bytes;
    return true;
}

template<typename NativeVector>
inline void DecodeGraphVector(const float (&disk)[3], NativeVector &native) {
    CopyGraphBits(native.x, disk[0]);
    CopyGraphBits(native.y, disk[1]);
    CopyGraphBits(native.z, disk[2]);
}

template<typename NativeVector>
inline void EncodeGraphVector(const NativeVector &native, float (&disk)[3]) {
    CopyGraphBits(disk[0], native.x);
    CopyGraphBits(disk[1], native.y);
    CopyGraphBits(disk[2], native.z);
}
} // namespace detail

inline bool ParseLegacyGraph(const void *bytes, std::size_t count, GraphFileView &view) {
    view = GraphFileView{};
    constexpr auto prefixBytes = sizeof(std::int32_t) + sizeof(LegacyGraphHeader);
    if (!bytes || count < prefixBytes || detail::GraphValue<std::int32_t>(bytes) != LegacyGraphVersion) {
        return false;
    }
    GraphFileView result;
    result.header = static_cast<const unsigned char *>(bytes) + sizeof(std::int32_t);
    result.nodeCount = detail::GraphValue<std::int32_t>(result.header + offsetof(LegacyGraphHeader, m_cNodes));
    result.linkCount = detail::GraphValue<std::int32_t>(result.header + offsetof(LegacyGraphHeader, m_cLinks));
    result.routeBytes = detail::GraphValue<std::int32_t>(result.header + offsetof(LegacyGraphHeader, m_nRouteInfo));
    result.hashCount = detail::GraphValue<std::int32_t>(result.header + offsetof(LegacyGraphHeader, m_nHashLinks));
    const unsigned char *cursor = static_cast<const unsigned char *>(bytes) + prefixBytes;
    std::size_t remaining = count - prefixBytes;
    if (!detail::GraphSegment(cursor, remaining, result.nodeCount, sizeof(LegacyGraphNode), result.nodes)
        || !detail::GraphSegment(cursor, remaining, result.linkCount, sizeof(LegacyGraphLink), result.links)
        || !detail::GraphSegment(cursor, remaining, result.nodeCount, sizeof(LegacyGraphDistance), result.distances)
        || !detail::GraphSegment(cursor, remaining, result.routeBytes, sizeof(char), result.routes)
        || !detail::GraphSegment(cursor, remaining, result.hashCount, sizeof(std::int16_t), result.hashes)) {
        return false;
    }
    result.consumedBytes = count - remaining;
    view = result;
    return true;
}

template<typename NativeGraph>
inline bool DecodeLegacyGraphHeader(const GraphFileView &view, NativeGraph &native) {
    if (!view.header) {
        return false;
    }
#define CSE_DECODE_GRAPH_VALUE(name) \
    static_assert(sizeof(native.name) == sizeof(LegacyGraphHeader::name), "Graph field width changed"); \
    std::memcpy(&native.name, view.header + offsetof(LegacyGraphHeader, name), sizeof(native.name))
    CSE_DECODE_GRAPH_VALUE(m_fGraphPresent);
    CSE_DECODE_GRAPH_VALUE(m_fGraphPointersSet);
    CSE_DECODE_GRAPH_VALUE(m_fRoutingComplete);
    CSE_DECODE_GRAPH_VALUE(m_cNodes);
    CSE_DECODE_GRAPH_VALUE(m_cLinks);
    CSE_DECODE_GRAPH_VALUE(m_nRouteInfo);
    CSE_DECODE_GRAPH_VALUE(m_RangeStart);
    CSE_DECODE_GRAPH_VALUE(m_RangeEnd);
    CSE_DECODE_GRAPH_VALUE(m_flShortest);
    CSE_DECODE_GRAPH_VALUE(m_iNearest);
    CSE_DECODE_GRAPH_VALUE(m_minX);
    CSE_DECODE_GRAPH_VALUE(m_minY);
    CSE_DECODE_GRAPH_VALUE(m_minZ);
    CSE_DECODE_GRAPH_VALUE(m_maxX);
    CSE_DECODE_GRAPH_VALUE(m_maxY);
    CSE_DECODE_GRAPH_VALUE(m_maxZ);
    CSE_DECODE_GRAPH_VALUE(m_minBoxX);
    CSE_DECODE_GRAPH_VALUE(m_minBoxY);
    CSE_DECODE_GRAPH_VALUE(m_minBoxZ);
    CSE_DECODE_GRAPH_VALUE(m_maxBoxX);
    CSE_DECODE_GRAPH_VALUE(m_maxBoxY);
    CSE_DECODE_GRAPH_VALUE(m_maxBoxZ);
    CSE_DECODE_GRAPH_VALUE(m_CheckedCounter);
    CSE_DECODE_GRAPH_VALUE(m_RegionMin);
    CSE_DECODE_GRAPH_VALUE(m_RegionMax);
    CSE_DECODE_GRAPH_VALUE(m_HashPrimes);
    CSE_DECODE_GRAPH_VALUE(m_nHashLinks);
    CSE_DECODE_GRAPH_VALUE(m_iLastActiveIdleSearch);
    CSE_DECODE_GRAPH_VALUE(m_iLastCoverSearch);
#undef CSE_DECODE_GRAPH_VALUE
    static_assert(sizeof(native.m_Cache) / sizeof(native.m_Cache[0]) == GraphCacheCount, "Graph cache count changed");
    for (std::size_t index = 0; index < GraphCacheCount; ++index) {
        LegacyGraphCache cache;
        std::memcpy(&cache, view.header + offsetof(LegacyGraphHeader, m_Cache) + index * sizeof(cache), sizeof(cache));
        detail::DecodeGraphVector(cache.v, native.m_Cache[index].v);
        detail::CopyGraphBits(native.m_Cache[index].n, cache.n);
    }
    native.m_pNodes = nullptr;
    native.m_pLinkPool = nullptr;
    native.m_pRouteInfo = nullptr;
    native.m_di = nullptr;
    native.m_pHashLinks = nullptr;
    return true;
}

template<typename NativeGraph>
inline LegacyGraphHeader EncodeLegacyGraphHeader(const NativeGraph &native) {
    LegacyGraphHeader disk{};
#define CSE_ENCODE_GRAPH_VALUE(name) detail::CopyGraphBits(disk.name, native.name)
    CSE_ENCODE_GRAPH_VALUE(m_fGraphPresent);
    CSE_ENCODE_GRAPH_VALUE(m_fGraphPointersSet);
    CSE_ENCODE_GRAPH_VALUE(m_fRoutingComplete);
    CSE_ENCODE_GRAPH_VALUE(m_cNodes);
    CSE_ENCODE_GRAPH_VALUE(m_cLinks);
    CSE_ENCODE_GRAPH_VALUE(m_nRouteInfo);
    CSE_ENCODE_GRAPH_VALUE(m_RangeStart);
    CSE_ENCODE_GRAPH_VALUE(m_RangeEnd);
    CSE_ENCODE_GRAPH_VALUE(m_flShortest);
    CSE_ENCODE_GRAPH_VALUE(m_iNearest);
    CSE_ENCODE_GRAPH_VALUE(m_minX);
    CSE_ENCODE_GRAPH_VALUE(m_minY);
    CSE_ENCODE_GRAPH_VALUE(m_minZ);
    CSE_ENCODE_GRAPH_VALUE(m_maxX);
    CSE_ENCODE_GRAPH_VALUE(m_maxY);
    CSE_ENCODE_GRAPH_VALUE(m_maxZ);
    CSE_ENCODE_GRAPH_VALUE(m_minBoxX);
    CSE_ENCODE_GRAPH_VALUE(m_minBoxY);
    CSE_ENCODE_GRAPH_VALUE(m_minBoxZ);
    CSE_ENCODE_GRAPH_VALUE(m_maxBoxX);
    CSE_ENCODE_GRAPH_VALUE(m_maxBoxY);
    CSE_ENCODE_GRAPH_VALUE(m_maxBoxZ);
    CSE_ENCODE_GRAPH_VALUE(m_CheckedCounter);
    CSE_ENCODE_GRAPH_VALUE(m_RegionMin);
    CSE_ENCODE_GRAPH_VALUE(m_RegionMax);
    CSE_ENCODE_GRAPH_VALUE(m_HashPrimes);
    CSE_ENCODE_GRAPH_VALUE(m_nHashLinks);
    CSE_ENCODE_GRAPH_VALUE(m_iLastActiveIdleSearch);
    CSE_ENCODE_GRAPH_VALUE(m_iLastCoverSearch);
#undef CSE_ENCODE_GRAPH_VALUE
    static_assert(sizeof(native.m_Cache) / sizeof(native.m_Cache[0]) == GraphCacheCount, "Graph cache count changed");
    for (std::size_t index = 0; index < GraphCacheCount; ++index) {
        detail::EncodeGraphVector(native.m_Cache[index].v, disk.m_Cache[index].v);
        detail::CopyGraphBits(disk.m_Cache[index].n, native.m_Cache[index].n);
    }
    return disk;
}

template<typename NativeNode>
inline bool DecodeLegacyGraphNode(const GraphFileView &view, std::size_t index, NativeNode &native) {
    if (!view.nodes || view.nodeCount < 0 || index >= static_cast<std::size_t>(view.nodeCount)) {
        return false;
    }
    LegacyGraphNode disk;
    std::memcpy(&disk, view.nodes + index * sizeof(disk), sizeof(disk));
    detail::DecodeGraphVector(disk.m_vecOrigin, native.m_vecOrigin);
    detail::DecodeGraphVector(disk.m_vecOriginPeek, native.m_vecOriginPeek);
#define CSE_COPY_GRAPH_NODE(name) detail::CopyGraphBits(native.name, disk.name)
    CSE_COPY_GRAPH_NODE(m_Region);
    CSE_COPY_GRAPH_NODE(m_afNodeInfo);
    CSE_COPY_GRAPH_NODE(m_cNumLinks);
    CSE_COPY_GRAPH_NODE(m_iFirstLink);
    CSE_COPY_GRAPH_NODE(m_pNextBestNode);
    CSE_COPY_GRAPH_NODE(m_flClosestSoFar);
    CSE_COPY_GRAPH_NODE(m_iPreviousNode);
    CSE_COPY_GRAPH_NODE(m_sHintType);
    CSE_COPY_GRAPH_NODE(m_sHintActivity);
    CSE_COPY_GRAPH_NODE(m_flHintYaw);
#undef CSE_COPY_GRAPH_NODE
    return true;
}

template<typename NativeNode>
inline LegacyGraphNode EncodeLegacyGraphNode(const NativeNode &native) {
    LegacyGraphNode disk{};
    detail::EncodeGraphVector(native.m_vecOrigin, disk.m_vecOrigin);
    detail::EncodeGraphVector(native.m_vecOriginPeek, disk.m_vecOriginPeek);
#define CSE_COPY_GRAPH_NODE(name) detail::CopyGraphBits(disk.name, native.name)
    CSE_COPY_GRAPH_NODE(m_Region);
    CSE_COPY_GRAPH_NODE(m_afNodeInfo);
    CSE_COPY_GRAPH_NODE(m_cNumLinks);
    CSE_COPY_GRAPH_NODE(m_iFirstLink);
    CSE_COPY_GRAPH_NODE(m_pNextBestNode);
    CSE_COPY_GRAPH_NODE(m_flClosestSoFar);
    CSE_COPY_GRAPH_NODE(m_iPreviousNode);
    CSE_COPY_GRAPH_NODE(m_sHintType);
    CSE_COPY_GRAPH_NODE(m_sHintActivity);
    CSE_COPY_GRAPH_NODE(m_flHintYaw);
#undef CSE_COPY_GRAPH_NODE
    return disk;
}

template<typename NativeLink, typename NativeEntity>
inline bool DecodeLegacyGraphLink(const GraphFileView &view, std::size_t index,
    NativeEntity *pendingEntity, NativeLink &native) {
    if (!view.links || view.linkCount < 0 || index >= static_cast<std::size_t>(view.linkCount)) {
        return false;
    }
    LegacyGraphLink disk;
    std::memcpy(&disk, view.links + index * sizeof(disk), sizeof(disk));
    if (disk.entityPointerPlaceholder && !pendingEntity) {
        return false;
    }
    detail::CopyGraphBits(native.m_iSrcNode, disk.m_iSrcNode);
    detail::CopyGraphBits(native.m_iDestNode, disk.m_iDestNode);
    detail::CopyGraphBits(native.m_szLinkEntModelname, disk.m_szLinkEntModelname);
    detail::CopyGraphBits(native.m_afLinkInfo, disk.m_afLinkInfo);
    detail::CopyGraphBits(native.m_flWeight, disk.m_flWeight);
    // A valid owned marker carries the pending lookup, never an address from the file.
    native.m_pLinkEnt = disk.entityPointerPlaceholder ? pendingEntity : nullptr;
    return true;
}

template<typename NativeLink>
inline LegacyGraphLink EncodeLegacyGraphLink(const NativeLink &native) {
    LegacyGraphLink disk{};
    detail::CopyGraphBits(disk.m_iSrcNode, native.m_iSrcNode);
    detail::CopyGraphBits(disk.m_iDestNode, native.m_iDestNode);
    detail::CopyGraphBits(disk.m_szLinkEntModelname, native.m_szLinkEntModelname);
    detail::CopyGraphBits(disk.m_afLinkInfo, native.m_afLinkInfo);
    detail::CopyGraphBits(disk.m_flWeight, native.m_flWeight);
    disk.entityPointerPlaceholder = native.m_pLinkEnt ? 1 : 0;
    return disk;
}

template<typename NativeDistance>
inline bool DecodeLegacyGraphDistance(const GraphFileView &view, std::size_t index, NativeDistance &native) {
    if (!view.distances || view.nodeCount < 0 || index >= static_cast<std::size_t>(view.nodeCount)) {
        return false;
    }
    LegacyGraphDistance disk;
    std::memcpy(&disk, view.distances + index * sizeof(disk), sizeof(disk));
    detail::CopyGraphBits(native.m_SortedBy, disk.m_SortedBy);
    detail::CopyGraphBits(native.m_CheckedEvent, disk.m_CheckedEvent);
    return true;
}

template<typename NativeDistance>
inline LegacyGraphDistance EncodeLegacyGraphDistance(const NativeDistance &native) {
    LegacyGraphDistance disk{};
    detail::CopyGraphBits(disk.m_SortedBy, native.m_SortedBy);
    detail::CopyGraphBits(disk.m_CheckedEvent, native.m_CheckedEvent);
    return disk;
}

} // namespace navigation
} // namespace cse

#endif
