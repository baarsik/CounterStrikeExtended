#include "VGUI.h"
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <new>

void operator delete(void* address, std::size_t size) noexcept;

namespace {
volatile std::size_t requestedBytes = 0;
void* allocatedAddress = nullptr;
int releases = 0;

void Require(bool condition, const char* description) {
    if (!condition) {
        std::fprintf(stderr, "VGUI allocator failure: %s\n", description);
        std::exit(EXIT_FAILURE);
    }
}

void* RecordAllocation(std::size_t size) {
    requestedBytes = size;
#if SIZE_MAX > UINT32_MAX
    if (size > UINT32_MAX) {
        throw std::bad_alloc();
    }
#endif
    allocatedAddress = std::malloc(size);
    if (!allocatedAddress) {
        throw std::bad_alloc();
    }
    return allocatedAddress;
}

void RecordRelease(void* address) {
    Require(address == allocatedAddress, "free callback receives the unchanged native address");
    ++releases;
    std::free(address);
    allocatedAddress = nullptr;
}
}

int main() {
    vgui_setMalloc(RecordAllocation);
    vgui_setFree(RecordRelease);
    constexpr std::size_t SmallRequestBytes = 37;
    void* small = ::operator new(SmallRequestBytes);
    Require(small == allocatedAddress && requestedBytes == SmallRequestBytes, "small request and returned identity");
    ::operator delete(small);
#if SIZE_MAX > UINT32_MAX
    // A throwing callback records the full request without claiming oversized storage.
    constexpr std::size_t HighRequestBytes = (std::size_t{1} << 32) + SmallRequestBytes;
    bool rejected = false;
    try {
        void* high = ::operator new(HighRequestBytes);
        ::operator delete(high);
    } catch (const std::bad_alloc&) {
        rejected = true;
    }
    Require(rejected && requestedBytes == HighRequestBytes, "request above 4 GiB reaches the throwing callback intact");
#endif
    Require(releases == 1 && allocatedAddress == nullptr, "callback-owned allocation released exactly once");
    vgui_setMalloc(nullptr);
    vgui_setFree(nullptr);
    void* ordinary = ::operator new(SmallRequestBytes);
    Require(ordinary != nullptr && allocatedAddress == nullptr, "default malloc callback restored");
    const int priorReleases = releases;
    ::operator delete(ordinary);
    Require(releases == priorReleases, "default free bypasses the former callback");
    vgui_setMalloc(RecordAllocation);
    vgui_setFree(RecordRelease);
    void* restarted = ::operator new(SmallRequestBytes);
    Require(restarted == allocatedAddress && requestedBytes == SmallRequestBytes,
        "callbacks can restart after default reset");
    ::operator delete(restarted, SmallRequestBytes);
    Require(releases == priorReleases + 1 && allocatedAddress == nullptr,
        "sized delete returns storage through the active free callback");
    vgui_setMalloc(nullptr);
    vgui_setFree(nullptr);
    std::puts("Native VGUI allocation callback width passed");
    return EXIT_SUCCESS;
}
