#include "frame_stream.h"
#include "capture_allocation.h"
#include <algorithm>
#include <cmath>
#include <new>

namespace cse {
namespace render {

FrameRecorder::FrameRecorder(FrameLimits limits) : limits_(limits), recorder_(limits.capture) {
}

Status FrameRecorder::Gate() const {
    return active_ ? failure_ : Status::InvalidOperation;
}

Status FrameRecorder::CaptureStatus() const {
    return Gate();
}

bool FrameRecorder::InPrimitive() const {
    return recorder_.InPrimitive();
}

Status FrameRecorder::Fail(Status status) {
    if (failure_ == Status::Ok) {
        failure_ = status;
    }
    return failure_;
}

Status FrameRecorder::Refuse(Status reason) {
    const auto status = Gate();
    if (status != Status::Ok) {
        return status;
    }
    return Fail(reason == Status::Ok ? Status::InvalidValue : reason);
}

Status FrameRecorder::BeginFrame(const FrameInfo& info) {
    if (active_ || !recorder_.AtFrameBoundary()) {
        return Status::InvalidOperation;
    }
    if (!info.serial || !info.displayEpoch || !info.width || !info.height ||
        info.width > static_cast<std::uint32_t>(kTextureDimension) ||
        info.height > static_cast<std::uint32_t>(kTextureDimension)) {
        return Status::InvalidValue;
    }
    info_ = info;
    active_ = true;
    return Status::Ok;
}

Status FrameRecorder::Submit(const Command& command) {
    const auto status = Gate();
    if (status != Status::Ok) {
        return status;
    }
    try {
        const auto firstDraw = recorder_.Draws().size();
        return AppendDrawEvents(firstDraw, recorder_.Submit(command));
    } catch (const std::bad_alloc&) {
        return Fail(Status::LimitExceeded);
    }
}

Status FrameRecorder::SubmitScalar(const ScalarCommand& command) {
    const auto status = Gate();
    if (status != Status::Ok) {
        return status;
    }
    try {
        const auto firstDraw = recorder_.Draws().size();
        return AppendDrawEvents(firstDraw, recorder_.SubmitScalar(command));
    } catch (const std::bad_alloc&) {
        return Fail(Status::LimitExceeded);
    }
}

Status FrameRecorder::AppendDrawEvents(std::size_t firstDraw, Status result) {
    if (result != Status::Ok) {
        return Fail(result);
    }
    const auto drawCount = recorder_.Draws().size();
    if (drawCount - firstDraw > limits_.events - events_.size()) {
        return Fail(Status::LimitExceeded);
    }
    for (std::size_t index = firstDraw; index < drawCount; ++index) {
        FrameEvent event;
        event.kind = FrameEventKind::Draw;
        event.drawIndex = index;
        const auto appended = AppendEvent(event);
        if (appended != Status::Ok) {
            return appended;
        }
    }
    return Status::Ok;
}

Status FrameRecorder::SubmitQuad(const std::array<Vertex, 4>& vertices) {
    const auto status = Gate();
    if (status != Status::Ok) {
        return status;
    }
    try {
        const auto firstDraw = recorder_.Draws().size();
        const auto result = recorder_.SubmitQuad(vertices);
        if (result != Status::Ok) {
            return Fail(result);
        }
        if (recorder_.Draws().size() == firstDraw) {
            return Status::Ok;
        }
        FrameEvent event;
        event.drawIndex = firstDraw;
        return AppendEvent(event);
    } catch (const std::bad_alloc&) {
        return Fail(Status::LimitExceeded);
    }
}

Status FrameRecorder::SubmitSurface(const std::shared_ptr<const SurfaceMesh>& mesh, float scroll, bool detail) {
    const auto status = Gate();
    if (status != Status::Ok) {
        return status;
    }
    try {
        const auto firstDraw = recorder_.Draws().size();
        const auto result = recorder_.SubmitSurface(mesh, scroll, detail);
        if (result != Status::Ok) {
            return Fail(result);
        }
        FrameEvent event;
        event.drawIndex = firstDraw;
        return AppendEvent(event);
    } catch (const std::bad_alloc&) {
        return Fail(Status::LimitExceeded);
    }
}

Status FrameRecorder::AppendEvent(const FrameEvent& event) {
    if (event.kind != FrameEventKind::Draw) {
        recorder_.EndQuadBatch();
    }
    if (events_.size() >= limits_.events) {
        return Fail(Status::LimitExceeded);
    }
    try {
        if (events_.size() == events_.capacity()) {
            // Geometric growth stays inside the independent event quota, with no addition overflow.
            const auto growth = std::min(limits_.events - events_.size(),
                std::max(events_.size(), std::size_t{1}));
            if (event.kind == FrameEventKind::Draw) {
                // Recorder has already appended its owned draw; failure here poisons the partial frame.
                BeforeCaptureAllocation(CaptureAllocationSite::EventGrowthAfterDraw);
            }
            events_.reserve(events_.size() + growth);
        }
        events_.push_back(event);
    } catch (const std::bad_alloc&) {
        return Fail(Status::LimitExceeded);
    }
    return Status::Ok;
}

Status FrameRecorder::Clear(const ClearEvent& clear) {
    const auto status = Gate();
    if (status != Status::Ok) {
        return status;
    }
    if (!recorder_.AtFrameBoundary()) {
        return Fail(Status::InvalidOperation);
    }
    constexpr std::uint32_t kClearMask = kClearColor | kClearDepth | kClearStencil;
    if (!clear.mask || (clear.mask & ~kClearMask) != 0 || !std::isfinite(clear.depth) ||
        clear.depth < 0 || clear.depth > 1 || clear.scissor[2] < 0 || clear.scissor[3] < 0) {
        return Fail(Status::InvalidValue);
    }
    for (const auto component : clear.color) {
        if (!std::isfinite(component)) {
            return Fail(Status::InvalidValue);
        }
    }
    FrameEvent event;
    event.kind = FrameEventKind::Clear;
    event.clear = clear;
    return AppendEvent(event);
}

Status FrameRecorder::Readback(const ReadbackEvent& readback) {
    const auto status = Gate();
    if (status != Status::Ok) {
        return status;
    }
    if (!recorder_.AtFrameBoundary()) {
        return Fail(Status::InvalidOperation);
    }
    if (!readback.requestId || readback.requestId <= lastRequestId_ ||
        (readback.stage != ReadbackStage::SceneColor && readback.stage != ReadbackStage::PresentedColor)) {
        return Fail(Status::InvalidValue);
    }
    const auto& rectangle = readback.rectangle;
    if (rectangle[0] < 0 || rectangle[1] < 0 || rectangle[2] <= 0 || rectangle[3] <= 0) {
        return Fail(Status::InvalidValue);
    }
    const auto x = static_cast<std::uint32_t>(rectangle[0]);
    const auto y = static_cast<std::uint32_t>(rectangle[1]);
    // Subtract only after proving the origin is contained, avoiding signed extent addition overflow.
    if (x > info_.width || y > info_.height ||
        static_cast<std::uint32_t>(rectangle[2]) > info_.width - x ||
        static_cast<std::uint32_t>(rectangle[3]) > info_.height - y) {
        return Fail(Status::InvalidValue);
    }
    FrameEvent event;
    event.kind = FrameEventKind::Readback;
    event.readback = readback;
    const auto result = AppendEvent(event);
    if (result == Status::Ok) {
        lastRequestId_ = readback.requestId;
    }
    return result;
}

Status FrameRecorder::Seal(FrameSnapshot& output) {
    const auto status = Gate();
    if (status != Status::Ok) {
        return status;
    }
    if (!output.events.empty() || !output.draws.empty()) {
        return Status::InvalidOperation;
    }
    const auto result = recorder_.TakeFrameDraws(output.draws);
    if (result != Status::Ok) {
        return Fail(result);
    }
    output.events.swap(events_);
    output.info = info_;
    active_ = false;
    return Status::Ok;
}

void FrameRecorder::Abort() {
    recorder_.AbortPendingFrame();
    events_.clear();
    failure_ = Status::Ok;
    active_ = false;
}

const LegacyState& FrameRecorder::State() const {
    return recorder_.State();
}

const TextureStore& FrameRecorder::Textures() const {
    return recorder_.Textures();
}

Status FrameRecorder::BeginList(std::uint32_t id, bool execute) {
    const auto status = Gate();
    if (status != Status::Ok) {
        return status;
    }
    try {
        return Fail(recorder_.BeginList(id, execute));
    } catch (const std::bad_alloc&) {
        return Fail(Status::LimitExceeded);
    }
}

Status FrameRecorder::EndList() {
    const auto status = Gate();
    if (status != Status::Ok) {
        return status;
    }
    try {
        return Fail(recorder_.EndList());
    } catch (const std::bad_alloc&) {
        return Fail(Status::LimitExceeded);
    }
}

Status FrameRecorder::DeleteList(std::uint32_t id) {
    const auto status = Gate();
    if (status != Status::Ok) {
        return status;
    }
    return Fail(recorder_.DeleteList(id));
}

Status FrameRecorder::GenerateLists(std::size_t count, std::uint32_t& first) {
    const auto status = Gate();
    if (status != Status::Ok) {
        return status;
    }
    try {
        return Fail(recorder_.GenerateLists(count, first));
    } catch (const std::bad_alloc&) {
        return Fail(Status::LimitExceeded);
    }
}

Status FrameRecorder::GenerateTextures(std::size_t count, std::vector<std::uint32_t>& names) {
    const auto status = Gate();
    if (status != Status::Ok) {
        return status;
    }
    try {
        return Fail(recorder_.GenerateTextures(count, names));
    } catch (const std::bad_alloc&) {
        return Fail(Status::LimitExceeded);
    }
}

}
}
