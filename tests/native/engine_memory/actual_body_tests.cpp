#include "host.h"
#include "NativeEngineMemory.h"

#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <string>
#include <type_traits>
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

struct Services {
    memory::PhysicalMemory physical{memory::QueryStatus::InvalidPageCount, 0};
    std::string trace;
    std::string error;
    bool failAllocation = false;
    bool returnFromError = false;
    int allocations = 0;
    int frees = 0;
    int errorBytes = 0;
    double errorMebibytes = 0;
} services;
}

namespace cse_native_engine_memory {
PhysicalMemory ControlledQuery() noexcept {
    services.trace += 'Q';
    return services.physical;
}

void* ControlledAllocate(int bytes) noexcept {
    Check(bytes >= 14680064 && bytes <= 134217728, "actual source must not exceed allocation ceiling");
    services.trace += 'A';
    ++services.allocations;
    return services.failAllocation ? nullptr : AllocateHeap(bytes);
}

void ControlledRelease(void*& base, int& bytes) noexcept {
    if (base != nullptr) {
        services.trace += 'F';
        ++services.frees;
    }
    ReleaseHeap(base, bytes);
}

static_assert(std::is_same<decltype(&ControlledQuery), decltype(&QueryPhysicalMemory)>::value,
    "actual helper query boundary");
static_assert(std::is_same<decltype(&ControlledAllocate), decltype(&AllocateHeap)>::value,
    "actual helper allocation boundary");
static_assert(std::is_same<decltype(&ControlledRelease), decltype(&ReleaseHeap)>::value,
    "actual helper release boundary");
}

// These are the complete functions from the actual source, not reconstructed
// widget/engine types. Only external fatal/query/allocation boundaries are controlled.
#define QueryPhysicalMemory ControlledQuery
#define AllocateHeap ControlledAllocate
#define ReleaseHeap ControlledRelease
#define COM_CheckParm ActualCheckParm
namespace sw {
quakeparms_t host_parms{};
qboolean g_bIsDedicatedServer = 0;
int com_argc = 0;
char** com_argv = nullptr;
int Q_strcmp(const char* left, const char* right) {
    return std::strcmp(left, right);
}
#include "COM_CheckParm.inc"
}
namespace hw {
quakeparms_t host_parms{};
qboolean g_bIsDedicatedServer = 0;
int com_argc = 0;
char** com_argv = nullptr;
int Q_strcmp(const char* left, const char* right) {
    return std::strcmp(left, right);
}
#include "COM_CheckParm.inc"
}
#undef COM_CheckParm

#define CSE_MEMORY_FIXTURE_OWNER sw
#define CSE_MEMORY_FIXTURE_SW 1
#include "owner_context.inc"
#undef CSE_MEMORY_FIXTURE_SW
#undef CSE_MEMORY_FIXTURE_OWNER
#define CSE_MEMORY_FIXTURE_OWNER hw
#define CSE_MEMORY_FIXTURE_SW 0
#include "owner_context.inc"
#undef CSE_MEMORY_FIXTURE_SW
#undef CSE_MEMORY_FIXTURE_OWNER
#undef QueryPhysicalMemory
#undef AllocateHeap
#undef ReleaseHeap

namespace {
template<typename Owner>
void RunOwner(const char* name) {
    for (const Case& item : Cases) {
        services = {};
        services.physical = memory::ClassifyPhysicalMemory(static_cast<std::int64_t>(item.physical), 1);
        Owner::Set(item.initial, item.dedicated, item.arguments);
        Owner::Init();
        Check(Owner::Heap().memsize == ExpectedBytes[item.id], "actual body matches frozen Win32 requested bytes");
        Check(services.error.empty() && services.allocations == 1, "valid body allocates once without fatal");
        auto* payload = static_cast<unsigned char*>(Owner::Heap().membase);
        Check(payload != nullptr, "actual malloc obtains arena");
        payload[0] = 0x31;
        payload[Owner::Heap().memsize - 1] = 0xa7;
        Check(payload[0] == 0x31 && payload[Owner::Heap().memsize - 1] == 0xa7,
            "first/last requested bytes writable");
        Owner::Shutdown();
        Owner::Shutdown();
        Check(Owner::Heap().membase == nullptr && Owner::Heap().memsize == 0 && services.frees == 1,
            "actual shutdown clears and frees once, including repeat cleanup");
        int requested = item.initial;
        for (std::size_t index = 1; index + 1 < item.arguments.size(); ++index) {
            if (item.arguments[index] == "-heapsize") {
                Check(memory::ParseHeapArgument(item.arguments[index + 1].c_str(), requested), "valid reference domain");
                break;
            }
        }
        Check(services.trace == (requested < 14680064 ? "HQMAF" : "HMAF"), "actual query/min/alloc/free order");
    }
    const char* invalid[]{"-1", "2097152", "999999999999999999999999"};
    for (const char* text : invalid) {
        services = {};
        services.returnFromError = true;
        Owner::Set(67108864, false, {"engine", "-heapsize", text, "-minmemory"});
        Owner::Init();
        Check(Owner::Heap().memsize == 67108864 && Owner::Heap().membase == nullptr,
            "invalid heap request preserves prior host state");
        Check(!services.error.empty() && services.trace == "HE" && services.allocations == 0,
            "returning fatal handler cannot fall through invalid input to query/allocation");
        Owner::Shutdown();
    }
    for (const bool dedicated : {false, true}) {
        for (const std::int64_t physical : {1ll, 14680064ll, 15728639ll}) {
            for (const bool minimum : {false, true}) {
                for (const bool cli : {false, true}) {
                    services = {};
                    services.returnFromError = true;
                    services.physical = memory::ClassifyPhysicalMemory(physical, 1);
                    std::vector<std::string> arguments{"engine"};
                    if (cli) {
                        arguments.push_back("-heapsize");
                        arguments.push_back("1");
                    }
                    if (minimum) {
                        arguments.push_back("-minmemory");
                    }
                    Owner::Set(0, dedicated, arguments);
                    Owner::Init();
                    Check(Owner::Heap().memsize == (cli ? 1024 : 0) && services.errorBytes == (cli ? 1024 : 0),
                        "Win32 reference diagnostic bytes before low-RAM overrides");
                    Check(services.trace == "HQE" && services.allocations == 0 && Owner::Heap().membase == nullptr,
                        "known low RAM fails before minmemory/dedicated and cannot fall through");
                    Owner::Shutdown();
                }
            }
        }
        for (const memory::PhysicalMemory physical : {
                 memory::ClassifyPhysicalMemory(-1, 4096), memory::ClassifyPhysicalMemory(7, -1),
                 memory::ClassifyPhysicalMemory(INT64_MAX, 3), memory::ClassifyPhysicalMemory(4294971392ll, 4096)}) {
            services = {};
            services.physical = physical;
            Owner::Set(0, dedicated, {"engine"});
            Owner::Init();
            Check(Owner::Heap().memsize == (dedicated ? 41943040 : 134217728), "full-width/failure fallback body");
            Owner::Shutdown();
            Check(services.trace == "HQMAF", "unknown query follows healthy ordering");
        }
    }
    services = {};
    services.failAllocation = true;
    services.returnFromError = true;
    Owner::Set(67108864, false, {"engine"});
    Owner::Init();
    Check(services.trace == "HMAE" && Owner::Heap().membase == nullptr && Owner::Heap().memsize == 67108864,
        "allocation failure reports after single bounded attempt");
    Check(services.errorMebibytes == 64.0, "Win32 reference allocation-failure diagnostic value");
    Owner::Shutdown();
    Check(services.frees == 0 && Owner::Heap().memsize == 0, "partial allocation failure releases safely");
    std::printf("PASS actual %s: 116 Windows rows; 24 low-RAM rows; invalid/unknown/high-page/alloc/cleanup gates\n",
        name);
}
}

int main() {
    // Read-only real SDK query plus one serial <=128MiB live arena. This physical
    // capacity reserve is not a claim about current memory pressure/availability.
    const auto physical = memory::QueryPhysicalMemory();
    Check(physical.status == memory::QueryStatus::Known && physical.totalBytes > 1207959552ull,
        "real named SDK query and 1GiB capacity reserve beyond maximum payload");
    RunOwner<sw::Adapter>("SW");
    RunOwner<hw::Adapter>("HW");
    std::printf("real_query_bytes=%llu; max_live_payload=134217728; no game/provider/socket/UI calls\n",
        static_cast<unsigned long long>(physical.totalBytes));
}
