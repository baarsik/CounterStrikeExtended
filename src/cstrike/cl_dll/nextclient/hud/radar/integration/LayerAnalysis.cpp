#include "hud/radar/integration/LayerAnalysis.h"

#include <utility>

#include "hud/radar/integration/Performance.h"

namespace radar::integration {
LayerAnalysis::LayerAnalysis() : job_([this](const RadarLayerAnalysisEvent& event) { Report(event); }) {}

LayerAnalysis::LayerAnalysis(Performance& performance) : LayerAnalysis() {
    performance_ = &performance;
}

LayerAnalysis::~LayerAnalysis() {
    Shutdown();
}

void LayerAnalysis::Start(RadarMapGeometry geometry, std::optional<RadarLayerOverrides> overrides) {
    RadarLayerAnalysisRequest_t request = ++next_request_;
    generations_.emplace(request, (performance_ ? performance_->get_map_generation() : 0));

    job_.Start(request, std::move(geometry), std::move(overrides));
}

void LayerAnalysis::Reset() {
    job_.Reset();
}

void LayerAnalysis::Shutdown() {
    job_.Shutdown();
}

std::unique_ptr<RadarLayerAnalysisResult> LayerAnalysis::Poll() {
    return job_.Poll();
}

bool LayerAnalysis::is_pending() const {
    return job_.is_pending();
}

void LayerAnalysis::Report(const RadarLayerAnalysisEvent& event) {
    performance::PerfWorkerStatus status{};

    switch (event.status) {
        case RadarLayerAnalysisStatus::Pending:
            status = performance::PerfWorkerStatus::Pending;
            break;
        case RadarLayerAnalysisStatus::Completed:
            status = performance::PerfWorkerStatus::Completed;
            break;
        case RadarLayerAnalysisStatus::Failed:
            status = performance::PerfWorkerStatus::Failed;
            break;
        case RadarLayerAnalysisStatus::Canceled:
            status = performance::PerfWorkerStatus::Canceled;
            break;
    }

    uint32_t generation = generations_.at(event.request);
    if (performance_) {
        performance_->ReportWorker({generation, event.duration_ns, status});
    }

    if (event.status != RadarLayerAnalysisStatus::Pending) {
        generations_.erase(event.request);
    }
}
} // namespace radar::integration
