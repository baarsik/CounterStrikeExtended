#pragma once

#include "legacy_commands.h"

namespace cse {
namespace render {

// Private capture bits, independent of GL clear enum values.
constexpr std::uint32_t kClearColor = 1;
constexpr std::uint32_t kClearDepth = 2;
constexpr std::uint32_t kClearStencil = 4;

enum class FrameEventKind { Draw, Clear, Readback };

struct ClearEvent {
    std::uint32_t mask = 0;
    std::array<float, 4> color{};
    double depth = 1;
    std::uint32_t stencil = 0;
    std::array<int, 4> scissor{};
    bool scissorEnabled = false;
    std::array<bool, 4> colorWrite{{true, true, true, true}};
    bool depthWrite = true;
    std::uint32_t stencilWriteMask = ~std::uint32_t{0};
};

enum class ReadbackStage { SceneColor, PresentedColor };

struct ReadbackEvent {
    std::uint64_t requestId = 0;
    ReadbackStage stage = ReadbackStage::SceneColor;
    // GL bottom-left pixel coordinates; delivery conversion belongs to a later backend.
    std::array<int, 4> rectangle{};
};

struct FrameEvent {
    FrameEventKind kind = FrameEventKind::Draw;
    std::size_t drawIndex = 0;
    ClearEvent clear;
    ReadbackEvent readback;
};

struct FrameInfo {
    std::uint64_t serial = 0;
    std::uint64_t displayEpoch = 0;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    // Nonzero when the engine holds every native world mesh until the next map cache reset.
    std::uint64_t surfaceMeshEpoch = 0;
};

struct FrameSnapshot {
    FrameInfo info;
    std::vector<FrameEvent> events;
    std::vector<DrawPacket> draws;
};

struct FrameLimits {
    CaptureLimits capture;
    std::size_t events = kMaximumDrawPackets * 2;
};

class FrameRecorder {
public:
    explicit FrameRecorder(FrameLimits limits = {}, TextureCapture capture = TextureCapture::AllBindings);
    Status BeginFrame(const FrameInfo& info);
    Status Submit(const Command& command);
    Status SubmitScalar(const ScalarCommand& command);
    Status SubmitQuad(const std::array<Vertex, 4>& vertices);
    Status SubmitIndexed(const std::vector<Vertex>& vertices, const std::vector<std::uint32_t>& indices,
        bool perVertexColor);
    Status SubmitSurface(const std::shared_ptr<const SurfaceMesh>& mesh, float scroll, bool detail);
    Status Refuse(Status reason);
    Status CaptureStatus() const;
    bool InPrimitive() const;
    bool AtFrameBoundary() const {
        return recorder_.AtFrameBoundary();
    }
    Status Clear(const ClearEvent& clear);
    Status Readback(const ReadbackEvent& readback);
    Status Seal(FrameSnapshot& output);
    // Return storage after the synchronous renderer releases its packet references.
    void Recycle(FrameSnapshot&& frame);
    void Abort();
    const FrameLimits& Limits() const {
        return limits_;
    }
    const LegacyState& State() const;
    const TextureStore& Textures() const;
    Status BeginList(std::uint32_t id, bool execute);
    Status EndList();
    Status DeleteList(std::uint32_t id);
    Status GenerateLists(std::size_t count, std::uint32_t& first);
    Status GenerateTextures(std::size_t count, std::vector<std::uint32_t>& names);

private:
    Status Gate() const;
    Status Fail(Status status);
    Status AppendEvent(const FrameEvent& event);
    Status AppendDrawEvents(std::size_t firstDraw, Status result);
    FrameLimits limits_;
    Recorder recorder_;
    FrameInfo info_;
    std::vector<FrameEvent> events_;
    std::uint64_t lastRequestId_ = 0;
    Status failure_ = Status::Ok;
    bool active_ = false;
};

}
}
