#include <cstdint>
#include <cstdio>
#include <new>
#include "../../../GoldSrc-SDK/common/checked_native_index.h"
#include "../../../GoldSrc-SDK/engine/world_leaf_index.h"
#include "../strings/high_offset_buffer.h"

struct Record {
    std::uint32_t identity;
    void* native;
};

#define REQUIRE(expression) do { if (!(expression)) { \
    std::fprintf(stderr, "Index check failed at line %d: %s\n", __LINE__, #expression); return 1; \
} } while (false)

int main() {
    HighOffsetBuffer mapping(nullptr);
    REQUIRE(mapping.get());
    constexpr std::size_t RecordCount = 3;
    auto* records = reinterpret_cast<Record*>(mapping.get());
    for (std::size_t index = 0; index < RecordCount; ++index) {
        new (records + index) Record{static_cast<std::uint32_t>(index), nullptr};
    }
    if (sizeof(void*) > sizeof(std::uint32_t)) {
        REQUIRE(reinterpret_cast<std::uintptr_t>(records) > UINT32_MAX);
    }
    int result = -1;
    for (int index = 0; index < static_cast<int>(RecordCount); ++index) {
        REQUIRE(cse::native::CheckedIndex(records + index, records, RecordCount, result));
        REQUIRE(result == index && records[result].identity == static_cast<std::uint32_t>(index));
    }
    constexpr int VisibleLeaves = static_cast<int>(RecordCount) - 1;
    REQUIRE(cse::bsp::CheckedWorldLeafIndex(records + VisibleLeaves, records, VisibleLeaves, result));
    REQUIRE(result == VisibleLeaves);
    REQUIRE(cse::bsp::CheckedWorldLeafIndex(records, records, 0, result) && result == 0);
    REQUIRE(!cse::bsp::CheckedWorldLeafIndex(records, records, -1, result));
    result = -1;
    REQUIRE(!cse::native::CheckedIndex(records + RecordCount, records, RecordCount, result) && result == -1);
    const auto base = reinterpret_cast<std::uintptr_t>(records);
    REQUIRE(!cse::native::CheckedIndexFromAddress(base - 1, base, sizeof(Record), RecordCount, result));
    REQUIRE(!cse::native::CheckedIndexFromAddress(base + 1, base, sizeof(Record), RecordCount, result));
    REQUIRE(!cse::native::CheckedIndexFromAddress(0, 0, sizeof(Record), RecordCount, result));
    REQUIRE(!cse::native::CheckedIndexFromAddress(base, base, 0, RecordCount, result));
    REQUIRE(!cse::native::CheckedIndexFromAddress(base, base, sizeof(Record), 0, result));
#if UINTPTR_MAX > UINT32_MAX
    // Synthetic arithmetic covers the signed index limit without claiming a multi-gigabyte object allocation.
    constexpr std::uintptr_t SyntheticBase = UINT64_C(0x10000000000);
    constexpr std::size_t LargestIndex = static_cast<std::size_t>(INT_MAX);
    REQUIRE(cse::native::CheckedIndexFromAddress(SyntheticBase + LargestIndex, SyntheticBase,
        1, LargestIndex + 1, result) && result == INT_MAX);
    REQUIRE(!cse::native::CheckedIndexFromAddress(SyntheticBase + LargestIndex + 1, SyntheticBase,
        1, LargestIndex + 2, result));
#endif
    for (std::size_t index = 0; index < RecordCount; ++index) {
        records[index].~Record();
    }
    return 0;
}
