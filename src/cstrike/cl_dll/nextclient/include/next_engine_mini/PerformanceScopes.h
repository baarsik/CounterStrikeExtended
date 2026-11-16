#pragma once

#include <cstdint>

namespace ncl_perf {
using PerfScopeId_t = std::uint32_t;
}

// NextClient's optional profiler is not a runtime dependency of the native port.
namespace performance {
class PerformanceMonitorInterface;
namespace PerfStage {
inline constexpr ncl_perf::PerfScopeId_t Radar = 0;
inline constexpr ncl_perf::PerfScopeId_t RadarBackdrop = 1;
inline constexpr ncl_perf::PerfScopeId_t RadarBackdropCapture = 2;
inline constexpr ncl_perf::PerfScopeId_t RadarBatch = 3;
inline constexpr ncl_perf::PerfScopeId_t RadarCapture = 4;
inline constexpr ncl_perf::PerfScopeId_t RadarDraw = 5;
inline constexpr ncl_perf::PerfScopeId_t RadarLayer = 6;
inline constexpr ncl_perf::PerfScopeId_t RadarLayout = 7;
inline constexpr ncl_perf::PerfScopeId_t RadarMapDraw = 8;
inline constexpr ncl_perf::PerfScopeId_t RadarObjectives = 9;
inline constexpr ncl_perf::PerfScopeId_t RadarPlayers = 10;
inline constexpr ncl_perf::PerfScopeId_t RadarPolicy = 11;
inline constexpr ncl_perf::PerfScopeId_t RadarPrepare = 12;
inline constexpr ncl_perf::PerfScopeId_t RadarText = 13;
inline constexpr ncl_perf::PerfScopeId_t RadarVisibility = 14;
} // namespace PerfStage
enum class PerfCounterId {
    RadarBatches,
    RadarEntitySweeps,
    RadarLayerCacheHits,
    RadarLayerImages,
    RadarQuads,
    RadarVisibilityTraces
};
class PerformanceMonitorInterface {
  public:
    void AddCounter(PerfCounterId, std::uint64_t) {}
};
enum class PerfWorkerStatus { Pending, Completed, Failed, Canceled };
struct PerfWorkerReport {
    std::uint32_t generation{};
    std::uint64_t duration_ns{};
    PerfWorkerStatus status{};
};
class PerformanceCpuScope {
  public:
    template <class... Args> explicit PerformanceCpuScope(Args&&...) {}
};
class PerformanceGpuScope {
  public:
    template <class... Args> explicit PerformanceGpuScope(Args&&...) {}
};
} // namespace performance
