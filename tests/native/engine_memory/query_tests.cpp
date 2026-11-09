#include <unistd.h>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <type_traits>
#include <vector>

namespace {
long pages = 1;
long pageBytes = 1;
std::vector<int> names;
void Check(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", message);
        std::exit(1);
    }
}
}

long ControlledSdkQuery(int name) {
    names.push_back(name);
    if (name == _SC_PHYS_PAGES) {
        return pages;
    }
    Check(name == _SC_PAGESIZE, "SDK query must use named page-size constant");
    return pageBytes;
}
static_assert(std::is_same<decltype(&ControlledSdkQuery), decltype(&::sysconf)>::value,
    "controlled query uses the real SDK signature, not invented Windows/native types");

#define sysconf ControlledSdkQuery
#include "NativeEngineMemory.h"
#undef sysconf

int main() {
    namespace memory = cse_native_engine_memory;
    static_assert(sizeof(long) == 8, "actual LP64 query return width");
    struct Case {
        long count;
        long size;
        memory::QueryStatus status;
        std::uint64_t bytes;
    };
    const Case cases[]{
        {4194304, 16384, memory::QueryStatus::Known, 68719476736ull},
        {4294971392l, 4096, memory::QueryStatus::Known, 17592202821632ull},
        {-1, 4096, memory::QueryStatus::InvalidPageCount, 0},
        {0, 4096, memory::QueryStatus::InvalidPageCount, 0},
        {1024, -1, memory::QueryStatus::InvalidPageSize, 0},
        {1024, 0, memory::QueryStatus::InvalidPageSize, 0},
        {(std::numeric_limits<long>::max)(), 3, memory::QueryStatus::ProductOverflow, 0}
    };
    for (const Case& item : cases) {
        pages = item.count;
        pageBytes = item.size;
        names.clear();
        const auto result = memory::QueryPhysicalMemory();
        Check(result.status == item.status && result.totalBytes == item.bytes,
            "actual helper preserves full-width results or explicit query failure reason");
        Check(names == std::vector<int>{_SC_PHYS_PAGES, _SC_PAGESIZE}, "both actual SDK queries in original order");
    }
    std::printf("PASS SDK query: named constants=%d/%d; long_bytes=%zu; high-count/nonpositive/overflow\n",
        _SC_PHYS_PAGES, _SC_PAGESIZE, sizeof(long));
}
