#include "Test.h"

#include <chrono>
#include <thread>
#include <utility>
#include <vector>

#include "hud/radar/map/layers/RadarLayerAnalysisJob.h"
#include "hud/radar/integration/LayerAnalysis.h"
#include "hud/radar/integration/Performance.h"
#include "RadarMapLayerFixtures.h"

namespace {
RadarMapGeometry Floor(float height) {
    RadarMapLayerFixture fixture;
    fixture.AddFloor(height, {{0.0f, 0.0f}, {256.0f, 256.0f}});
    return std::move(fixture.geometry);
}

std::unique_ptr<RadarLayerAnalysisResult> Wait(auto& job) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (std::chrono::steady_clock::now() < deadline) {
        if (auto result = job.Poll()) {
            return result;
        }
        std::this_thread::yield();
    }
    return nullptr;
}
} // namespace

RADAR_TEST(NativeLayerWorker, PublishesResultAndEventsOnOwner) {
    const auto owner = std::this_thread::get_id();
    std::vector<RadarLayerAnalysisEvent> events;
    RadarLayerAnalysisJob job([&](const auto& event) {
        CHECK_EQ(std::this_thread::get_id(), owner);
        events.push_back(event);
    });
    job.Start(42, Floor(24.0f));
    CHECK_EQ(events.size(), 1u);
    CHECK_EQ(events.front().status, RadarLayerAnalysisStatus::Pending);
    auto result = Wait(job);
    CHECK_TRUE(result);
    CHECK_EQ(result->request, 42u);
    CHECK_EQ(result->status, RadarMapStatus::Ready);
    CHECK_EQ(result->map.get_layers().size(), 1u);
    CHECK_FLOAT_EQ(result->map.get_layers().front().height, 24.0f);
    CHECK_EQ(events.size(), 2u);
    CHECK_EQ(events.back().status, RadarLayerAnalysisStatus::Completed);
    CHECK_FALSE(job.is_pending());
    CHECK_FALSE(job.Poll());
}

RADAR_TEST(NativeLayerWorker, SupersededRequestsNeverPublishAndOnlyLatestPendingRuns) {
    std::vector<RadarLayerAnalysisEvent> events;
    RadarLayerAnalysisJob job([&](const auto& event) { events.push_back(event); });
    job.Start(1, Floor(24.0f));
    job.Start(2, Floor(48.0f));
    job.Start(3, Floor(96.0f));
    CHECK_EQ(events.size(), 3u);
    CHECK_EQ(events[1].request, 1u);
    CHECK_EQ(events[1].status, RadarLayerAnalysisStatus::Canceled);
    CHECK_EQ(events[2].request, 2u);
    CHECK_EQ(events[2].status, RadarLayerAnalysisStatus::Canceled);
    auto result = Wait(job);
    CHECK_TRUE(result);
    CHECK_EQ(result->request, 3u);
    CHECK_FLOAT_EQ(result->map.get_layers().front().height, 96.0f);
    CHECK_EQ(events.size(), 5u);
    CHECK_EQ(events[3].request, 3u);
    CHECK_EQ(events[3].status, RadarLayerAnalysisStatus::Pending);
    CHECK_EQ(events[4].status, RadarLayerAnalysisStatus::Completed);
    CHECK_FALSE(job.Poll());
}

RADAR_TEST(NativeLayerWorker, ResetDiscardsRunningAndPendingResults) {
    RadarLayerAnalysisJob job;
    job.Start(1, Floor(24.0f));
    job.Start(2, Floor(48.0f));
    job.Reset();
    CHECK_FALSE(job.is_pending());
    job.Shutdown();
    CHECK_FALSE(job.Poll());
    job.Start(3, Floor(96.0f));
    auto result = Wait(job);
    CHECK_TRUE(result);
    CHECK_EQ(result->request, 3u);
}

RADAR_TEST(NativeLayerWorker, DestructionJoinsWithoutPumpingFrames) {
    for (int iteration = 0; iteration < 30; ++iteration) {
        RadarLayerAnalysisJob job;
        job.Start(1, Floor(24.0f));
        job.Start(2, Floor(48.0f));
    }
}

RADAR_TEST(NativeLayerWorker, FailedAnalysisKeepsRequestAndEmitsFailure) {
    std::vector<RadarLayerAnalysisEvent> events;
    RadarLayerAnalysisJob job([&](const auto& event) { events.push_back(event); });
    job.Start(19, {});
    auto result = Wait(job);
    CHECK_TRUE(result);
    CHECK_EQ(result->request, 19u);
    CHECK_EQ(result->status, RadarMapStatus::Failed);
    CHECK_EQ(events.size(), 2u);
    CHECK_EQ(events.back().status, RadarLayerAnalysisStatus::Failed);
    CHECK_FALSE(job.is_pending());
}

RADAR_TEST(NativeLayerAnalysis, DefaultAndPerformanceOwnersCancelResetAndReuse) {
    radar::integration::Performance performance;
    radar::integration::LayerAnalysis with_performance(performance);
    radar::integration::LayerAnalysis without_performance;
    for (auto* analysis : {&with_performance, &without_performance}) {
        analysis->Start(Floor(24.0f));
        analysis->Start(Floor(48.0f));
        analysis->Reset();
        CHECK_FALSE(analysis->is_pending());
        analysis->Shutdown();
        CHECK_FALSE(analysis->Poll());
        analysis->Start(Floor(96.0f));
        auto result = Wait(*analysis);
        CHECK_TRUE(result);
        CHECK_EQ(result->request, 3u);
        CHECK_FLOAT_EQ(result->map.get_layers().front().height, 96.0f);
        analysis->Shutdown();
        analysis->Shutdown();
    }
}
