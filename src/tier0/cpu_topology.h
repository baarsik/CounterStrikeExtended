#ifndef CSE_CPU_TOPOLOGY_H
#define CSE_CPU_TOPOLOGY_H

#include "tier0/platform.h"
#if defined(_WIN32)
#include <windows.h>
#else
#include <unistd.h>
#if defined(__APPLE__)
#include <sys/sysctl.h>
#endif
#endif
#include <cstddef>
#include <cstdint>
#include <limits>

namespace cse_cpu {
struct Topology {
    uint8 logical;
    uint8 physical;
};

inline Topology BoundTopology(uint64 logical, uint64 physical) {
    constexpr uint64 MaximumProcessors = std::numeric_limits<uint8>::max();
    if (logical == 0) {
        logical = 1;
    }
    if (physical == 0) {
        physical = 1;
    }
    if (physical > logical) {
        physical = logical;
    }
    if (logical > MaximumProcessors) {
        logical = MaximumProcessors;
    }
    if (physical > MaximumProcessors) {
        physical = MaximumProcessors;
    }
    return {static_cast<uint8>(logical), static_cast<uint8>(physical)};
}

inline Topology ReadTopology() {
#if defined(_WIN32)
    const DWORD logical = GetActiveProcessorCount(ALL_PROCESSOR_GROUPS);
    DWORD bytes = 0;
    GetLogicalProcessorInformationEx(RelationProcessorCore, nullptr, &bytes);
    uint64 physical = 0;
    if (bytes != 0) {
        void* storage = HeapAlloc(GetProcessHeap(), 0, bytes);
        if (storage) {
            DWORD available = bytes;
            if (GetLogicalProcessorInformationEx(RelationProcessorCore,
                static_cast<PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX>(storage), &available) && available <= bytes) {
                const auto* cursor = static_cast<const unsigned char*>(storage);
                std::size_t remaining = available;
                constexpr std::size_t RecordHeaderBytes = offsetof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX, Processor);
                while (remaining >= RecordHeaderBytes) {
                    const auto* record = reinterpret_cast<const SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX*>(cursor);
                    if (record->Size < RecordHeaderBytes || record->Size > remaining) {
                        break;
                    }
                    if (record->Relationship == RelationProcessorCore) {
                        ++physical;
                    }
                    cursor += record->Size;
                    remaining -= record->Size;
                }
            }
            HeapFree(GetProcessHeap(), 0, storage);
        }
    }
    return BoundTopology(logical, physical);
#else
    const long online = sysconf(_SC_NPROCESSORS_ONLN);
    uint64 logical = online > 0 ? static_cast<uint64>(online) : 1;
    uint64 physical = logical;
#if defined(__APPLE__)
    uint32_t value = 0;
    std::size_t bytes = sizeof(value);
    if (sysctlbyname("hw.logicalcpu", &value, &bytes, nullptr, 0) == 0 && bytes == sizeof(value)) {
        logical = value;
    }
    value = 0;
    bytes = sizeof(value);
    if (sysctlbyname("hw.physicalcpu", &value, &bytes, nullptr, 0) == 0 && bytes == sizeof(value)) {
        physical = value;
    }
#endif
    return BoundTopology(logical, physical);
#endif
}
} // namespace cse_cpu
#endif
