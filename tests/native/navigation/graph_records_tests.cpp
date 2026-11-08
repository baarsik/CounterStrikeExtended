#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <memory>
#include <vector>
#if defined(_MSC_VER)
#pragma warning(push)
// Existing SDK private allocation takes int32; this fixture does not allocate entities.
#pragma warning(disable: 4267)
#endif
#include "extdll.h"
#if defined(M_PI)
#undef M_PI
#endif
#include "util.h"
#include "cbase.h"
#include "nodes.h"
#if defined(_MSC_VER)
#pragma warning(pop)
#endif
#include "legacy_graph_records.h"

#define CHECK(condition) do { if (!(condition)) { \
    std::fprintf(stderr, "Graph check failed at line %d: %s\n", __LINE__, #condition); \
    return 1; \
} } while (false)

template<typename T>
void Append(std::vector<unsigned char> &bytes, const T &value) {
    const auto *begin = reinterpret_cast<const unsigned char *>(&value);
    bytes.insert(bytes.end(), begin, begin + sizeof(value));
}

int main(int argc, char **argv) {
    using namespace cse::navigation;
    LegacyGraphHeader header{};
    header.m_fGraphPresent = 1;
    header.m_fGraphPointersSet = 1;
    header.m_fRoutingComplete = 1;
    header.nodesPointerPlaceholder = UINT32_C(0xfedcba98);
    header.linksPointerPlaceholder = 1;
    header.routesPointerPlaceholder = UINT32_MAX;
    header.distancePointerPlaceholder = 1;
    header.hashPointerPlaceholder = UINT32_MAX;
    header.m_cNodes = 2;
    header.m_cLinks = 1;
    header.m_nRouteInfo = 2;
    header.m_nHashLinks = 2;
    header.m_RegionMin[1] = -123.5f;
    header.m_RangeEnd[2][255] = 1;
    header.m_Cache[127].v[2] = 10.5f;
    header.m_Cache[127].n = -1;
    header.m_iLastCoverSearch = 1;
    LegacyGraphNode source{};
    source.m_vecOrigin[0] = 12.5f;
    source.m_vecOriginPeek[2] = -14.25f;
    source.m_cNumLinks = 1;
    LegacyGraphNode destination{};
    destination.m_vecOrigin[0] = 20.0f;
    destination.m_iFirstLink = 1;
    LegacyGraphLink link{};
    link.m_iDestNode = 1;
    link.entityPointerPlaceholder = UINT32_C(0xffffff01);
    std::memcpy(link.m_szLinkEntModelname, "*123", sizeof(link.m_szLinkEntModelname));
    link.m_flWeight = 7.5f;
    LegacyGraphDistance distance{};
    distance.m_SortedBy[0] = 1;
    std::vector<unsigned char> bytes;
    Append(bytes, std::int32_t{16});
    Append(bytes, header);
    Append(bytes, source);
    Append(bytes, destination);
    Append(bytes, link);
    Append(bytes, distance);
    Append(bytes, distance);
    Append(bytes, std::uint8_t{0});
    Append(bytes, std::uint8_t{127});
    Append(bytes, std::int16_t{0});
    Append(bytes, std::int16_t{-1});
    // Hand-derived Win32 layout, independent of the native SDK class sizes.
    CHECK(bytes.size() == 8638);
    GraphFileView view{};
    CHECK(ParseLegacyGraph(bytes.data(), bytes.size(), view));
    CHECK(view.nodeCount == 2 && view.linkCount == 1 && view.routeBytes == 2 && view.hashCount == 2);
    CHECK(view.consumedBytes == bytes.size());
    auto native = std::make_unique<CGraph>();
    CHECK(DecodeLegacyGraphHeader(view, *native));
    CHECK(!native->m_pNodes && !native->m_pLinkPool && !native->m_pRouteInfo && !native->m_di
        && !native->m_pHashLinks);
    CHECK(native->m_RegionMin[1] == -123.5f && native->m_RangeEnd[2][255] == 1);
    CHECK(native->m_Cache[127].v.z == 10.5f && native->m_Cache[127].n == -1);
    CHECK(native->m_iLastCoverSearch == 1);
    if (sizeof(void *) > sizeof(std::uint32_t)) {
        CHECK(reinterpret_cast<std::uintptr_t>(native.get()) > UINT32_MAX);
        CHECK(sizeof(CGraph) != 8396 && sizeof(CLink) != 24);
    } else {
        CHECK(sizeof(CGraph) == 8396 && sizeof(CLink) == 24);
    }
    header.nodesPointerPlaceholder = 0;
    header.linksPointerPlaceholder = 0;
    header.routesPointerPlaceholder = 0;
    header.distancePointerPlaceholder = 0;
    header.hashPointerPlaceholder = 0;
    const auto encodedHeader = EncodeLegacyGraphHeader(*native);
    CHECK(std::memcmp(&encodedHeader, &header, sizeof(header)) == 0);
    CNode node{};
    CHECK(DecodeLegacyGraphNode(view, 0, node));
    CHECK(node.m_vecOrigin.x == 12.5f && node.m_vecOriginPeek.z == -14.25f && node.m_cNumLinks == 1);
    const auto encodedNode = EncodeLegacyGraphNode(node);
    CHECK(std::memcmp(&encodedNode, &source, sizeof(source)) == 0);
    CHECK(!DecodeLegacyGraphNode(view, 2, node));
    entvars_t pendingEntity{};
    CLink nativeLink{};
    CHECK(DecodeLegacyGraphLink(view, 0, &pendingEntity, nativeLink));
    CHECK(nativeLink.m_pLinkEnt == &pendingEntity && nativeLink.m_iDestNode == 1);
    CHECK(nativeLink.m_flWeight == 7.5f && std::memcmp(nativeLink.m_szLinkEntModelname, "*123", 4) == 0);
    const auto encodedLink = EncodeLegacyGraphLink(nativeLink);
    link.entityPointerPlaceholder = 1;
    CHECK(std::memcmp(&encodedLink, &link, sizeof(link)) == 0);
    DIST_INFO nativeDistance{};
    CHECK(DecodeLegacyGraphDistance(view, 0, nativeDistance));
    CHECK(nativeDistance.m_SortedBy[0] == 1 && nativeDistance.m_CheckedEvent == 0);
    const auto encodedDistance = EncodeLegacyGraphDistance(nativeDistance);
    CHECK(std::memcmp(&encodedDistance, &distance, sizeof(distance)) == 0);
    CHECK(!DecodeLegacyGraphLink(view, 0, static_cast<entvars_t *>(nullptr), nativeLink));
    auto noEntityBytes = bytes;
    const std::uint32_t noEntity = 0;
    std::memcpy(noEntityBytes.data() + 8584, &noEntity, sizeof(noEntity));
    GraphFileView noEntityView;
    CHECK(ParseLegacyGraph(noEntityBytes.data(), noEntityBytes.size(), noEntityView));
    CHECK(DecodeLegacyGraphLink(noEntityView, 0, static_cast<entvars_t *>(nullptr), nativeLink));
    CHECK(nativeLink.m_pLinkEnt == nullptr);
    CHECK(!ParseLegacyGraph(bytes.data(), bytes.size() - 1, view));
    CHECK(!ParseLegacyGraph(nullptr, bytes.size(), view));
    CHECK(!ParseLegacyGraph(bytes.data(), 4, view));
    for (const auto offset : {28, 32, 36, 8388}) {
        for (const auto count : {std::int32_t{-1}, INT32_MAX}) {
            auto invalid = bytes;
            std::memcpy(invalid.data() + offset, &count, sizeof(count));
            CHECK(!ParseLegacyGraph(invalid.data(), invalid.size(), view));
        }
    }
    for (const std::size_t count : {0, 4, 8400, 8490, 8576, 8600, 8632, 8634, 8637}) {
        CHECK(!ParseLegacyGraph(bytes.data(), count, view));
    }
    auto wrongVersion = bytes;
    const std::int32_t unsupportedVersion = 17;
    std::memcpy(wrongVersion.data(), &unsupportedVersion, sizeof(unsupportedVersion));
    CHECK(!ParseLegacyGraph(wrongVersion.data(), wrongVersion.size(), view));
    LegacyGraphHeader emptyHeader{};
    std::vector<unsigned char> empty;
    Append(empty, std::int32_t{16});
    Append(empty, emptyHeader);
    CHECK(ParseLegacyGraph(empty.data(), empty.size(), view));
    CHECK(view.consumedBytes == 8400 && view.nodeCount == 0 && view.linkCount == 0);
    CHECK(DecodeLegacyGraphHeader(view, *native));
    CHECK(!native->m_fGraphPresent && !native->m_pNodes && !native->m_di);
    CHECK(!DecodeLegacyGraphNode(view, 0, node));
    if (argc > 1) {
        CHECK(argc <= 3);
        std::ifstream input(argv[1], std::ios::binary);
        CHECK(input);
        const std::vector<unsigned char> captured((std::istreambuf_iterator<char>(input)), {});
        CHECK(ParseLegacyGraph(captured.data(), captured.size(), view));
        CHECK(view.nodeCount == 32 && view.linkCount == 102 && view.routeBytes == 1853 && view.hashCount == 156);
        CHECK(view.consumedBytes == 16341 && captured.size() == 16341);
        CHECK(DecodeLegacyGraphHeader(view, *native));
        auto actual = captured;
        const auto capturedHeader = EncodeLegacyGraphHeader(*native);
        std::memcpy(actual.data() + 4, &capturedHeader, sizeof(capturedHeader));
        for (std::int32_t index = 0; index < view.nodeCount; ++index) {
            CHECK(DecodeLegacyGraphNode(view, index, node));
            const auto record = EncodeLegacyGraphNode(node);
            std::memcpy(actual.data() + 8400 + index * 88, &record, sizeof(record));
            CHECK(DecodeLegacyGraphDistance(view, index, nativeDistance));
            const auto distanceRecord = EncodeLegacyGraphDistance(nativeDistance);
            std::memcpy(actual.data() + 13664 + index * 16, &distanceRecord, sizeof(distanceRecord));
        }
        for (std::int32_t index = 0; index < view.linkCount; ++index) {
            CHECK(DecodeLegacyGraphLink(view, index, &pendingEntity, nativeLink));
            const auto record = EncodeLegacyGraphLink(nativeLink);
            std::memcpy(actual.data() + 11216 + index * 24, &record, sizeof(record));
        }
        auto expected = captured;
        // Hand-derived Win32 c1a0 extents, including ignored pointer and alignment slots.
        for (const auto offset : {16, 20, 24, 40, 8384}) {
            std::memset(expected.data() + offset, 0, 4);
        }
        for (std::size_t index = 0; index < 128; ++index) {
            std::memset(expected.data() + 6286 + index * 16, 0, 2);
        }
        for (std::size_t index = 0; index < 32; ++index) {
            expected[8400 + index * 88 + 27] = 0;
        }
        for (std::size_t index = 0; index < 102; ++index) {
            std::uint32_t presence;
            const auto offset = 11216 + index * 24 + 8;
            std::memcpy(&presence, expected.data() + offset, sizeof(presence));
            presence = presence ? 1 : 0;
            std::memcpy(expected.data() + offset, &presence, sizeof(presence));
        }
        CHECK(actual == expected);
        if (argc == 3) {
            std::ofstream output(argv[2], std::ios::binary);
            CHECK(output.write(reinterpret_cast<const char *>(actual.data()), actual.size()));
        }
    }
    return 0;
}
