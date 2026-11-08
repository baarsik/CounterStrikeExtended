#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <new>
#include "extdll.h"
#include "../strings/high_offset_buffer.h"

struct EntityPool {
    edict_t *edicts;
    int max_edicts;
    int num_edicts;
} sv{};
struct InvalidEntity {};
[[noreturn]] void Sys_Error(const char *, ...) {
    throw InvalidEntity{};
}
#include "../../../GoldSrc-SDK/engine/entity_offsets_impl.h"

#define CHECK(condition) do { \
    if (!(condition)) { \
        std::fprintf(stderr, "Engine entity check failed at line %d: %s\n", __LINE__, #condition); \
        return 1; \
    } \
} while (false)

template<typename Call>
bool Rejected(Call call) {
    try {
        call();
    } catch (const InvalidEntity &) {
        return true;
    }
    return false;
}

int main() {
    constexpr int Capacity = 3;
    constexpr int Allocated = 2;
    constexpr EOFFSET EntityStride = static_cast<EOFFSET>(sizeof(edict_t));
    HighOffsetBuffer storage(nullptr);
    CHECK(storage.get());
    sv.edicts = reinterpret_cast<edict_t *>(storage.get());
    sv.max_edicts = Capacity;
    sv.num_edicts = Allocated;
    for (int index = 0; index < Capacity; ++index) {
        new (sv.edicts + index) edict_t{};
    }
    if (sizeof(void *) > sizeof(std::uint32_t)) {
        CHECK(reinterpret_cast<std::uintptr_t>(sv.edicts) > UINT32_MAX);
    }
    CHECK(PEntityOfEntOffset(0) == sv.edicts);
    CHECK(EntOffsetOfPEntity(sv.edicts + 1) == EntityStride);
    CHECK(IndexOfEdict(nullptr) == 0);
    CHECK(IndexOfEdict(sv.edicts + Capacity - 1) == Capacity - 1);
    CHECK(NUM_FOR_EDICT(sv.edicts + Allocated - 1) == Allocated - 1);
    CHECK(PEntityOfEntOffset(-1) == nullptr);
    CHECK(PEntityOfEntOffset(1) == nullptr);
    CHECK(PEntityOfEntOffset(Capacity * EntityStride) == nullptr);
    CHECK(Rejected([&]() { IndexOfEdict(sv.edicts + Capacity); }));
    CHECK(Rejected([&]() { NUM_FOR_EDICT(sv.edicts + Allocated); }));
    auto *unaligned = reinterpret_cast<edict_t *>(reinterpret_cast<std::uintptr_t>(sv.edicts) + 1);
    CHECK(Rejected([&]() { EntOffsetOfPEntity(unaligned); }));
    CHECK(Rejected([&]() { IndexOfEdict(unaligned); }));
    CHECK(Rejected([&]() { NUM_FOR_EDICT(unaligned); }));
#if UINTPTR_MAX > UINT32_MAX
    constexpr auto wrapDistance = UINT64_C(1) << 32;
    auto *foreign = reinterpret_cast<edict_t *>(reinterpret_cast<std::uintptr_t>(sv.edicts) + wrapDistance);
    CHECK(Rejected([&]() { EntOffsetOfPEntity(foreign); }));
    CHECK(Rejected([&]() { IndexOfEdict(foreign); }));
    constexpr auto wrappedIndexDistance = wrapDistance * sizeof(edict_t);
    auto *wrappedIndex = reinterpret_cast<edict_t *>(reinterpret_cast<std::uintptr_t>(sv.edicts)
        + wrappedIndexDistance);
    CHECK(Rejected([&]() { NUM_FOR_EDICT(wrappedIndex); }));
#endif
    CHECK(Rejected([&]() { EDICT_NUM(Capacity); }));
    CHECK(Rejected([&]() { EDICT_NUM(-1); }));
    CHECK(EDICT_NUM(Capacity - 1) == sv.edicts + Capacity - 1);
    EOFFSET offsetGuard = 77;
    std::int32_t indexGuard = 88;
    CHECK(!cse::entities::OffsetFromEntity(sv.edicts, 0, sv.edicts, offsetGuard));
    CHECK(offsetGuard == 77);
    CHECK(!cse::entities::IndexFromOffset(sv.edicts, Capacity, -1, indexGuard));
    CHECK(indexGuard == 88);
    CHECK(!cse::entities::ValidPool(sv.edicts, static_cast<std::size_t>(-1)));
    auto *overflowBase = reinterpret_cast<edict_t *>(UINTPTR_MAX - sizeof(edict_t) / 2);
    CHECK(!cse::entities::ValidPool(overflowBase, Capacity));
    sv.num_edicts = Capacity + 1;
    CHECK(Rejected([&]() { NUM_FOR_EDICT(sv.edicts); }));
    sv.num_edicts = Allocated;
    for (int index = 0; index < Capacity; ++index) {
        sv.edicts[index].~edict_t();
    }
    std::puts("Native engine entity offsets, exact bounds and foreign pointer rejection passed");
    return 0;
}
