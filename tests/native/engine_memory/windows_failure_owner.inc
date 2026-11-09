namespace CSE_FAILURE_OWNER {
// Exact SDK declarations and controlled bindings are shared with the frozen
// healthy reference; the unused healthy-only fatal binding remains unused here.
#define Sys_Error HealthyOnlyError
#include "reference_context.inc"
#undef Sys_Error

HGLOBAL WINAPI ControlledFailureAllocation(UINT flags, SIZE_T bytes) {
    Check(flags == GMEM_FIXED && bytes >= MinimumBytes && bytes <= MaximumBytes,
        "bounded original SDK allocation request");
    ++allocations;
    trace += 'A';
    return nullptr;
}
static_assert(std::is_same<decltype(&ControlledFailureAllocation), decltype(&::GlobalAlloc)>::value,
    "failure injection uses exact SDK allocator signature");

[[noreturn]] void Sys_Error(const char* format, ...) {
    trace += 'E';
    FatalCall call{format};
    va_list arguments;
    va_start(arguments, format);
    if (std::strcmp(format, "Available memory less than 15MB!!! %i\n") == 0) {
        call.bytes = va_arg(arguments, int);
    } else {
        Check(std::strcmp(format, "Unable to allocate %.2f MB\n") == 0, "only source-defined failure formats");
        call.mebibytes = va_arg(arguments, double);
    }
    va_end(arguments);
    throw call;
}

#define GlobalMemoryStatus ControlledGlobalMemoryStatus
#define GlobalAlloc ControlledFailureAllocation
#define GlobalFree ControlledGlobalFree
#if CSE_FAILURE_SW
#define MINIMUM_HEAP_SIZE 0xE00000
#define MAXIMUM_HEAP_SIZE 0x8000000
#include "original/sw-Sys_InitMemory.inc"
#include "original/sw-Sys_ShutdownMemory.inc"
#undef MINIMUM_HEAP_SIZE
#undef MAXIMUM_HEAP_SIZE
#else
constexpr int kFourteenMegabytes = 0xE00000;
constexpr int kOneHundredTwentyEightMegabytes = 0x8000000;
constexpr float kBytesToMegabytes = 0.00000095367431640625f;
#include "original/hw-Sys_InitMemory.inc"
#include "original/hw-Sys_ShutdownMemory.inc"
#endif
#undef GlobalMemoryStatus
#undef GlobalAlloc
#undef GlobalFree

struct Adapter {
    static void Set(int initial, unsigned long long physical, bool dedicated, const std::vector<std::string>& input) {
        Check(host_parms.membase == nullptr && owned == nullptr, "no live allocation in failure oracle");
        host_parms = {};
        host_parms.memsize = initial;
        physicalBytes = physical;
        g_bIsDedicatedServer = dedicated;
        queries = allocations = releases = 0;
        trace.clear();
        static std::vector<std::string> text;
        static std::vector<char*> arguments;
        text = input;
        arguments.clear();
        for (std::string& item : text) {
            arguments.push_back(&item[0]);
        }
        com_argc = static_cast<int>(arguments.size());
        com_argv = arguments.data();
    }
    static quakeparms_t& Heap() {
        return host_parms;
    }
    static std::string Trace() {
        return trace;
    }
    static unsigned int Allocations() {
        return allocations;
    }
    static void Init() {
        Sys_InitMemory();
    }
    static void Shutdown() {
        Sys_ShutdownMemory();
    }
};
}
