#include "studio_mesh_batch.h"

#include <cstdio>
#include <cstdlib>
#include <vector>

namespace {
void Check(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", message);
        std::exit(EXIT_FAILURE);
    }
}
}

int main() {
    using cse::studio::MeshCache;
    const std::int16_t strip[]{4, 0, 0, 0, 0, 1, 1, 1, 0, 2, 2, 1, 1, 3, 3, 0, 1, 0};
    MeshCache cache;
    const auto* mesh = cache.Get(strip, std::size(strip), 4, 4);
    Check(mesh != nullptr, "valid strip builds an indexed mesh");
    Check(mesh->indices == std::vector<std::uint32_t>{0, 1, 2, 2, 1, 3},
        "strip alternates winding and keeps newest provoking vertex last");
    Check(cache.Get(strip, std::size(strip), 4, 4) == mesh, "unchanged topology is reused");
    Check(mesh->vertices.size() == 4 && mesh->lastVertex == 3, "source terminal vertex is retained");

    const std::int16_t fan[]{-4, 0, 0, 0, 0, 1, 1, 1, 0, 2, 2, 1, 1, 3, 3, 0, 1, 0};
    mesh = cache.Get(fan, std::size(fan), 4, 4);
    Check(mesh && mesh->indices == std::vector<std::uint32_t>{0, 1, 2, 0, 2, 3}, "fan order");

    // One shared position has a distinct normal and UV; the last source tuple repeats the first.
    std::int16_t seams[]{3, 0, 0, 0, 0, 1, 1, 1, 0, 2, 2, 1, 1,
        3, 0, 1, 2, 3, 2, 2, 1, 1, 0, 0, 0, 0, 0};
    mesh = cache.Get(seams, std::size(seams), 3, 3);
    Check(mesh && mesh->vertices.size() == 4, "only complete tuples deduplicate");
    Check(mesh->indices == std::vector<std::uint32_t>{0, 1, 2, 3, 2, 0}, "UV seam indices");
    Check(mesh->lastVertex == 0, "terminal source tuple survives deduplication");
    Check(mesh->vertices[3].normal == 1 && mesh->vertices[3].s == 2, "normal and UV retained");
    seams[15] = 2;
    mesh = cache.Get(seams, std::size(seams), 3, 3);
    Check(mesh && mesh->vertices[3].normal == 2, "address reuse with changed bytes rebuilds topology");
    Check(!cache.Get(seams, std::size(seams), 2, 3), "changed bounds cannot reuse a cached mesh");
    Check(!cache.Get(seams, 1, 3, 3), "shorter allocation cannot reuse a cached mesh");

    const std::int16_t shortPrimitive[]{2, 0, 0, 0, 0, 1, 1, 3, 4, 0};
    mesh = cache.Get(shortPrimitive, std::size(shortPrimitive), 2, 2);
    Check(mesh && mesh->indices.empty() && mesh->lastVertex == 1, "short primitive retains terminal attributes");
    const std::int16_t empty[]{0};
    mesh = cache.Get(empty, std::size(empty), 1, 1);
    Check(mesh && mesh->vertices.empty(), "empty mesh");
    Check(!cache.Get(strip, std::size(strip) - 1, 4, 4), "missing terminator rejected");
    Check(!cache.Get(strip, 5, 4, 4), "truncated tuple stream rejected");
    Check(!cache.Get(strip, std::size(strip), 3, 4), "out-of-range position rejected");
    Check(!cache.Get(strip, std::size(strip), 4, 3), "out-of-range normal rejected");
    const std::int16_t negative[]{1, -1, 0, 0, 0, 0};
    Check(!cache.Get(negative, std::size(negative), 1, 1), "negative position rejected");
    const std::int16_t minimum[]{-32768, 0};
    Check(!cache.Get(minimum, std::size(minimum), 1, 1), "minimum signed count handled without overflow");
    Check(!cache.Get(nullptr, 0, 1, 1), "null stream rejected");
    MeshCache tiny(1);
    Check(!tiny.Get(strip, std::size(strip), 4, 4), "cache budget refuses oversized entry");
    cache.Clear();
    Check(cache.Get(strip, std::size(strip), 4, 4) != nullptr, "cache clear permits renderer restart");
    std::puts("studio topology passed");
}
