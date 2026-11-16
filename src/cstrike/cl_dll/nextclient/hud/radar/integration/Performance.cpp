#include "Performance.h"

namespace radar::integration {
std::uint32_t Performance::get_map_generation() const {
    return 0;
}

void Performance::ReportWorker(const performance::PerfWorkerReport&) {}

void Performance::AddCounter(performance::PerfCounterId, std::uint64_t) {}

ncl_gl::ProgramBuildCallbacks Performance::ProgramCallbacks() const {
    return {};
}
} // namespace radar::integration
