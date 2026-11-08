#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <type_traits>
#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable: 4267)
#endif
#include "extdll.h"
#if defined(M_PI)
#undef M_PI
#endif
#include "util.h"
#include "cbase.h"
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wswitch"
#endif
#include "nav.h"
#if defined(__clang__)
#pragma clang diagnostic pop
#endif
#include "nav_area.h"
#if defined(_MSC_VER)
#pragma warning(pop)
#endif

#define CHECK(condition) do { \
    if (!(condition)) { \
        std::fprintf(stderr, "Nav reference check failed at line %d: %s\n", __LINE__, #condition); \
        return 1; \
    } \
} while (false)

int main() {
    // Opaque, aligned storage tests native pointer identity without constructing a bot world.
    constexpr auto storageUnits = (sizeof(CNavArea) + sizeof(std::max_align_t) - 1) / sizeof(std::max_align_t);
    static_assert(alignof(CNavArea) <= alignof(std::max_align_t), "Fixture storage alignment is insufficient");
    auto storage = std::make_unique<std::max_align_t[]>(storageUnits);
    auto *area = reinterpret_cast<CNavArea *>(storage.get());
    if (sizeof(void *) > sizeof(std::uint32_t)) {
        CHECK(reinterpret_cast<std::uintptr_t>(area) > UINT32_MAX);
    }
    NavConnect connection{};
    connection.id = UINT32_C(0x12345678);
    CHECK(connection.Resolve([&](unsigned int id) { return id == UINT32_C(0x12345678) ? area : nullptr; }));
    CHECK(connection.area == area && connection.id == UINT32_C(0x12345678));
    CHECK(connection.Resolve([&](unsigned int id) { return id == UINT32_C(0x12345678) ? area : nullptr; }));
    connection.id = 77;
    CHECK(!connection.Resolve([](unsigned int) -> CNavArea * { return nullptr; }));
    CHECK(connection.id == 77 && connection.area == nullptr);
    connection.id = 0;
    CHECK(connection.Resolve([](unsigned int) -> CNavArea * { return nullptr; }));
    CHECK(connection.area == nullptr && connection.id == 0);
    // Runtime connections keep pointer comparison, including copied links.
    connection.area = area;
    NavConnect copied = connection;
    CHECK(copied == connection);
    copied.area = nullptr;
    CHECK(!(copied == connection));
    auto *spot = reinterpret_cast<HidingSpot *>(storage.get());
    SpotOrder order{};
    order.id = UINT32_C(0x87654321);
    order.t = 0.75f;
    CHECK(order.Resolve([&](unsigned int id) { return id == UINT32_C(0x87654321) ? spot : nullptr; }));
    CHECK(order.spot == spot && order.id == UINT32_C(0x87654321) && order.t == 0.75f);
    CHECK(!order.Resolve([](unsigned int) -> HidingSpot * { return nullptr; }));
    CHECK(order.spot == nullptr && order.id == UINT32_C(0x87654321));
    return 0;
}
