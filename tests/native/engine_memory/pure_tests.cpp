#include "NativeEngineMemory.h"

#include <cerrno>
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <string>
#include <vector>

namespace memory = cse_native_engine_memory;

namespace {
void Check(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", message);
        std::exit(1);
    }
}

#include "cases.inc"
#include "golden.inc"

void ReferenceCases() {
    for (const Case& item : Cases) {
        int bytes = item.initial;
        for (std::size_t index = 1; index < item.arguments.size(); ++index) {
            if (item.arguments[index] == "-heapsize") {
                if (index + 1 < item.arguments.size()) {
                    Check(memory::ParseHeapArgument(item.arguments[index + 1].c_str(), bytes), "valid heap argument");
                }
                break;
            }
        }
        const memory::PhysicalMemory physical = item.physical == 0
            ? memory::ClassifyPhysicalMemory(0, 1)
            : memory::ClassifyPhysicalMemory(static_cast<std::int64_t>(item.physical), 1);
        memory::HeapPlan plan{-91, false};
        Check(memory::PlanHeap(bytes, physical, item.dedicated, plan) == memory::PlanStatus::Ready,
            "all valid Windows oracle cases admitted");
        for (const std::string& argument : item.arguments) {
            if (argument == "-minmemory") {
                plan.bytes = memory::kMinimumHeapBytes;
                break;
            }
        }
        Check(plan.bytes == ExpectedBytes[item.id], "measured Win32 exact requested heap bytes");
        Check(plan.usedUnknownFallback == (bytes < memory::kMinimumHeapBytes && item.physical == 0),
            "unknown fallback is distinguished from successful large-RAM query");
    }
}

void InvalidArguments() {
    const char* rejected[]{nullptr, "-1", "-2097152", "2097152", "2147483647", "999999999999999999999999"};
    for (const char* argument : rejected) {
        int unchanged = 731;
        errno = EDOM;
        Check(!memory::ParseHeapArgument(argument, unchanged), "negative or overflowing request rejected");
        Check(unchanged == 731 && errno == EDOM, "invalid request has no output or errno mutation");
    }
    int value = 0;
    Check(memory::ParseHeapArgument("2097151suffix", value) && value == 2147482624, "last safe KiB multiply");
    Check(memory::ParseHeapArgument("-0", value) && value == 0, "negative zero retains atoi behavior");
    Check(memory::ParseHeapArgument("nonnumeric", value) && value == 0, "nonnumeric retains automatic atoi zero");
    Check(memory::ParseHeapArgument("\t+14336tail", value) && value == 14680064, "ASCII prefix behavior");
}

void PhysicalFailures() {
    struct Case {
        std::int64_t pages;
        std::int64_t pageBytes;
        memory::QueryStatus status;
    };
    const Case cases[]{
        {-1, 4096, memory::QueryStatus::InvalidPageCount},
        {0, 4096, memory::QueryStatus::InvalidPageCount},
        {4096, -1, memory::QueryStatus::InvalidPageSize},
        {4096, 0, memory::QueryStatus::InvalidPageSize},
        {INT64_MAX, 3, memory::QueryStatus::ProductOverflow}
    };
    for (const Case& item : cases) {
        const memory::PhysicalMemory physical = memory::ClassifyPhysicalMemory(item.pages, item.pageBytes);
        Check(physical.status == item.status && physical.totalBytes == 0, "unknown query reason, no invented RAM");
        for (const bool dedicated : {false, true}) {
            memory::HeapPlan plan{-91, false};
            Check(memory::PlanHeap(0, physical, dedicated, plan) == memory::PlanStatus::Ready,
                "unknown query uses approved fallback");
            Check(plan.bytes == (dedicated ? 41943040 : 134217728) && plan.usedUnknownFallback,
                "unknown client128/dedicated40 fallback");
        }
    }
    const auto high = memory::ClassifyPhysicalMemory(4294971392ll, 4096);
    Check(high.status == memory::QueryStatus::Known && high.totalBytes == 17592202821632ull,
        "page count above uint32 remains full width");
    const auto largest = memory::ClassifyPhysicalMemory(INT64_MAX, 2);
    Check(largest.status == memory::QueryStatus::Known && largest.totalBytes == UINT64_MAX - 1,
        "checked product supports full unsigned result");
    memory::HeapPlan plan{-91, true};
    Check(memory::PlanHeap(0, high, false, plan) == memory::PlanStatus::Ready
        && plan.bytes == 134217728 && !plan.usedUnknownFallback, "known high memory keeps Windows cap");
    const auto low = memory::ClassifyPhysicalMemory(15728639, 1);
    plan = {-91, true};
    Check(memory::PlanHeap(0, low, true, plan) == memory::PlanStatus::PhysicalMemoryTooSmall,
        "known under15MiB remains fatal before dedicated/min overrides");
    Check(plan.bytes == -91 && plan.usedUnknownFallback, "rejected plan output unchanged");
    Check(memory::PlanHeap(67108864, low, true, plan) == memory::PlanStatus::Ready && plan.bytes == 67108864,
        "explicit configured heap bypasses physical policy");
}
}

int main() {
    static_assert(sizeof(int) == 4 && INT_MAX == 2147483647, "actual host heap field domain");
    ReferenceCases();
    InvalidArguments();
    PhysicalFailures();
    Check(memory::AllocateHeap(0) == nullptr && memory::AllocateHeap(134217729) == nullptr,
        "allocator refuses requests outside approved arena ceiling");
    void* empty = nullptr;
    int bytes = 14680064;
    memory::ReleaseHeap(empty, bytes);
    Check(empty == nullptr && bytes == 0, "partial-init release clears record");
    std::puts("PASS pure: 116 measured Windows cases; bounded CLI; full-width query and explicit unknown statuses");
}
