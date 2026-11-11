#include "frame_stream.h"
#include "capture_allocation.h"
#include <algorithm>
#include <limits>
#include <new>
#include <string>
#include <cstdio>
#include <cstdlib>
#include <cstring>

using namespace cse::render;

namespace {

constexpr std::size_t kTriangleVertices = 3;
constexpr float kRetainedPosition = 19.0f;
constexpr std::uint32_t kPendingList = 7;

void Check(bool value, const char* message) {
    if (!value) {
        std::fprintf(stderr, "FAIL %s\n", message);
        std::exit(EXIT_FAILURE);
    }
}

Command Make(Op op, std::uint32_t argument = 0) {
    Command command;
    command.op = op;
    command.argument = argument;
    return command;
}

// Catches destructive rejection, copying instead of ownership transfer, stale
// frame charges, and an abort that leaves a primitive or list compilation open.
void RecorderBoundaryTransfer() {
    CaptureLimits limits;
    limits.frameVertices = kTriangleVertices;
    Recorder recorder(limits);
    Command triangle = Make(Op::DrawArrays, static_cast<std::uint32_t>(Primitive::Triangles));
    triangle.vertices.resize(kTriangleVertices);
    triangle.vertices.front().position[0] = kRetainedPosition;
    Check(recorder.AtFrameBoundary(), "fresh recorder is at frame boundary");
    Check(recorder.Submit(triangle) == Status::Ok, "record owned triangle");
    const Vertex* ownedVertices = recorder.Draws().front().vertices.data();
    std::vector<DrawPacket> output(1);
    Check(recorder.TakeFrameDraws(output) == Status::InvalidOperation, "nonempty output rejected");
    Check(output.size() == 1 && output.front().vertices.empty(), "rejected output unchanged");
    Check(recorder.Draws().size() == 1, "rejected transfer retains recorder draw");
    output.clear();
    Check(recorder.TakeFrameDraws(output) == Status::Ok, "boundary transfers full draw ownership");
    Check(output.size() == 1 && recorder.Draws().empty(), "transfer empties recorder only");
    Check(output.front().vertices.data() == ownedVertices, "transfer preserves owned allocation");
    triangle.vertices.front().position[0] = -kRetainedPosition;
    Check(output.front().vertices.front().position[0] == kRetainedPosition, "caller mutation is isolated");
    Check(recorder.Submit(triangle) == Status::Ok, "transfer resets frame vertex budget");
    Check(recorder.Submit(Make(Op::Begin, static_cast<std::uint32_t>(Primitive::Triangles))) == Status::Ok,
        "open primitive");
    std::vector<DrawPacket> rejected;
    Check(!recorder.AtFrameBoundary(), "primitive is not boundary");
    Check(recorder.TakeFrameDraws(rejected) == Status::InvalidOperation, "primitive transfer rejected");
    Check(rejected.empty() && recorder.Draws().size() == 1, "boundary rejection changes neither object");
    recorder.AbortPendingFrame();
    Check(recorder.AtFrameBoundary() && recorder.Draws().empty(), "abort drops primitive and current draws");
    Check(recorder.BeginList(kPendingList, false) == Status::Ok, "open list compilation");
    Check(!recorder.AtFrameBoundary(), "list compilation is not boundary");
    Check(recorder.TakeFrameDraws(rejected) == Status::InvalidOperation, "list transfer rejected");
    recorder.AbortPendingFrame();
    Check(recorder.AtFrameBoundary(), "abort abandons list compilation");
    Check(recorder.Submit(triangle) == Status::Ok, "next frame accepts geometry after abort");
    Check(output.front().vertices.front().position[0] == kRetainedPosition, "abort preserves older owned output");
}

constexpr std::uint32_t kFrameExtent = 64;
constexpr std::uint32_t kTextureName = 11;
constexpr std::size_t kImageBytes = 16;
constexpr std::uint64_t kSentinelSerial = 999;

FrameInfo Info(std::uint64_t serial = 1) {
    return {serial, 1, kFrameExtent, kFrameExtent};
}

Command Geometry(float position = kRetainedPosition) {
    Command command = Make(Op::DrawArrays, static_cast<std::uint32_t>(Primitive::Triangles));
    command.vertices.resize(kTriangleVertices);
    command.vertices[0].position = {{position, 0, 0, 1}};
    command.vertices[1].position = {{0, position, 0, 1}};
    command.vertices[2].position = {{0, 0, position, 1}};
    return command;
}

Command Image() {
    Command command = Make(Op::TextureImage);
    constexpr int kImageExtent = 2;
    command.upload.width = kImageExtent;
    command.upload.height = kImageExtent;
    command.upload.bytes = {255, 0, 0, 255, 0, 255, 0, 255, 0, 0, 255, 255, 255, 255, 0, 255};
    return command;
}

Command Binding() {
    Command command = Make(Op::BindTexture);
    command.second = kTextureName;
    return command;
}

ClearEvent DepthClear() {
    ClearEvent clear{};
    clear.mask = kClearDepth;
    clear.color = {{0.25f, 0.5f, 0.75f, 1.0f}};
    clear.depth = 0.625;
    clear.stencil = 9;
    clear.scissor = {{-2, 3, 17, 19}};
    clear.scissorEnabled = true;
    clear.colorWrite = {{true, false, true, false}};
    clear.depthWrite = false;
    clear.stencilWriteMask = 0x35;
    return clear;
}

ReadbackEvent Read(std::uint64_t id = 1) {
    return {id, ReadbackStage::SceneColor, {{0, 0, 8, 9}}};
}

void Send(FrameRecorder& recorder, const Command& command) {
    Check(recorder.Submit(command) == Status::Ok, "valid frame command accepted");
}

void Begin(FrameRecorder& recorder, std::uint64_t serial = 1) {
    Check(recorder.BeginFrame(Info(serial)) == Status::Ok, "begin valid frame");
}

void CheckFailedSeal(FrameRecorder& recorder, Status failure) {
    FrameSnapshot output;
    output.info.serial = kSentinelSerial;
    Check(recorder.Seal(output) == failure, "seal returns first failure");
    Check(output.info.serial == kSentinelSerial && output.events.empty() && output.draws.empty(),
        "failed seal preserves sentinel output without partial admission");
}

void Order() {
    FrameRecorder recorder;
    Begin(recorder);
    Send(recorder, Geometry());
    const auto clear = DepthClear();
    Check(recorder.Clear(clear) == Status::Ok, "ordered depth clear");
    Check(recorder.BeginList(kPendingList, false) == Status::Ok, "compile replay list");
    Send(recorder, Geometry(2));
    Send(recorder, Geometry(3));
    Check(recorder.EndList() == Status::Ok, "commit two-draw list");
    Send(recorder, Make(Op::CallList, kPendingList));
    Check(recorder.Readback(Read()) == Status::Ok, "ordered readback");
    FrameSnapshot output;
    Check(recorder.Seal(output) == Status::Ok, "seal ordered frame");
    Check(output.events.size() == 5 && output.draws.size() == 3, "exact event and draw counts");
    Check(output.events[0].kind == FrameEventKind::Draw && output.events[0].drawIndex == 0 &&
        output.events[1].kind == FrameEventKind::Clear && output.events[2].kind == FrameEventKind::Draw &&
        output.events[2].drawIndex == 1 && output.events[3].kind == FrameEventKind::Draw &&
        output.events[3].drawIndex == 2 && output.events[4].kind == FrameEventKind::Readback, "exact ordered stream");
    const auto& retained = output.events[1].clear;
    Check(retained.mask == kClearDepth && retained.color == clear.color && retained.depth == 0.625 &&
        retained.stencil == 9 && retained.scissor == clear.scissor && retained.scissorEnabled &&
        retained.colorWrite == clear.colorWrite && !retained.depthWrite && retained.stencilWriteMask == 0x35,
        "all explicit clear masks and state retained");
    Check(output.events[4].readback.requestId == 1 &&
        output.events[4].readback.stage == ReadbackStage::SceneColor &&
        output.events[4].readback.rectangle == Read().rectangle, "readback identity and bottom-left rectangle retained");
    Check(output.draws[1].vertices[0].position[0] == 2 && output.draws[2].vertices[0].position[0] == 3,
        "list replay preserves draw identity and ordering");
    Check(output.info.serial == 1 && output.info.displayEpoch == 1 && output.info.width == kFrameExtent &&
        output.info.height == kFrameExtent, "frame envelope retained");
}

const std::vector<std::uint8_t>& Pixels(const FrameSnapshot& frame) {
    return frame.draws.at(0).resources[0].texture2D->levels.at(0)->pixels;
}

void RequireHigh(const void* address, const char* label) {
#if UINTPTR_MAX > UINT32_MAX
    Check(reinterpret_cast<std::uintptr_t>(address) > UINT32_MAX,
        "UNQUALIFIED high-address allocation; placement fallback required, never skip");
    std::printf("HIGH_ADDRESS %s %p\n", label, address);
#else
    (void)address;
    (void)label;
#endif
}

void Owned() {
    FrameSnapshot first;
    FrameSnapshot second;
    const auto expectedFirst = Image().upload.bytes;
    auto expectedSecond = expectedFirst;
    std::reverse(expectedSecond.begin(), expectedSecond.end());
    {
        FrameRecorder recorder;
        auto image = Image();
        auto geometry = Geometry();
        RequireHigh(image.upload.bytes.data(), "source-pixels-A");
        RequireHigh(geometry.vertices.data(), "source-vertices-A");
        Begin(recorder);
        Send(recorder, Binding());
        Send(recorder, image);
        Send(recorder, geometry);
        Check(recorder.Seal(first) == Status::Ok, "seal A while recorder remains alive");
        Begin(recorder, 2);
        image.upload.bytes = expectedSecond;
        RequireHigh(image.upload.bytes.data(), "source-pixels-B");
        Send(recorder, image);
        auto deletion = Make(Op::DeleteTextures);
        deletion.names = {kTextureName};
        Send(recorder, deletion);
        Send(recorder, Binding());
        Send(recorder, image);
        geometry.vertices[0].position[0] = -kRetainedPosition;
        RequireHigh(geometry.vertices.data(), "source-vertices-B");
        Send(recorder, geometry);
        Check(recorder.Seal(second) == Status::Ok, "seal B after replace/delete/rebind");
        image.upload.bytes.assign(kImageBytes, 0);
        geometry.vertices.assign(kTriangleVertices, Vertex{});
        Check(Pixels(first) == expectedFirst && Pixels(second) == expectedSecond, "exact independent live versions");
    }
    Check(Pixels(first) == expectedFirst && Pixels(second) == expectedSecond, "versions survive recorder destruction");
    Check(first.draws[0].vertices[0].position[0] == kRetainedPosition &&
        second.draws[0].vertices[0].position[0] == -kRetainedPosition, "full-width owned vertex reads after destruction");
    const std::array<std::array<float, 4>, kTriangleVertices> expectedPositions = {{{{19, 0, 0, 1}},
        {{0, 19, 0, 1}}, {{0, 0, 19, 1}}}};
    for (std::size_t index = 0; index < expectedPositions.size(); ++index) {
        Check(first.draws[0].vertices[index].position == expectedPositions[index], "exact A positions after destruction");
        auto expected = expectedPositions[index];
        if (index == 0) {
            expected[0] = -kRetainedPosition;
        }
        Check(second.draws[0].vertices[index].position == expected, "exact B positions after destruction");
    }
    Check(first.draws[0].resources[0].texture2D->generation != second.draws[0].resources[0].texture2D->generation,
        "name reuse retains distinct immutable generations");
}

void Boundaries() {
    RecorderBoundaryTransfer();
    FrameRecorder recorder;
    Check(recorder.Submit(Geometry()) == Status::InvalidOperation &&
        recorder.Clear(DepthClear()) == Status::InvalidOperation &&
        recorder.Readback(Read()) == Status::InvalidOperation, "out-of-frame semantic operations rejected");
    std::vector<std::uint32_t> names;
    Check(recorder.GenerateTextures(1, names) == Status::InvalidOperation && names.empty(), "name generation gated");
    Check(recorder.BeginList(1, false) == Status::InvalidOperation &&
        recorder.EndList() == Status::InvalidOperation && recorder.DeleteList(1) == Status::InvalidOperation,
        "list operations gated");
    for (int field = 0; field < 6; ++field) {
        auto info = Info();
        if (field == 0) {
            info.width = 0;
        }
        if (field == 1) {
            info.height = 0;
        }
        if (field == 2) {
            info.width = static_cast<std::uint32_t>(kTextureDimension) + 1;
        }
        if (field == 3) {
            info.height = static_cast<std::uint32_t>(kTextureDimension) + 1;
        }
        if (field == 4) {
            info.serial = 0;
        }
        if (field == 5) {
            info.displayEpoch = 0;
        }
        Check(recorder.BeginFrame(info) == Status::InvalidValue, "invalid frame envelope rejected unchanged");
    }
    Begin(recorder);
    Check(recorder.BeginFrame(Info(2)) == Status::InvalidOperation, "duplicate begin rejected unchanged");
    FrameSnapshot empty;
    Check(recorder.Seal(empty) == Status::Ok && empty.info.serial == 1 && empty.events.empty(), "empty frame valid");
    for (bool list : {false, true}) {
        Begin(recorder);
        if (list) {
            Check(recorder.BeginList(kPendingList, false) == Status::Ok, "unfinished list");
        } else {
            Send(recorder, Make(Op::Begin));
        }
        CheckFailedSeal(recorder, Status::InvalidOperation);
        Check(recorder.Clear(DepthClear()) == Status::InvalidOperation, "clear inside primitive/list rejected");
        Check(recorder.Readback(Read()) == Status::InvalidOperation, "readback inside primitive/list rejected");
        recorder.Abort();
    }
    for (bool list : {false, true}) {
        for (bool readback : {false, true}) {
            Begin(recorder);
            if (list) {
                Check(recorder.BeginList(kPendingList, false) == Status::Ok, "direct unfinished list boundary");
            } else {
                Send(recorder, Make(Op::Begin));
            }
            const auto result = readback ? recorder.Readback(Read()) : recorder.Clear(DepthClear());
            Check(result == Status::InvalidOperation, "direct clear/readback boundary rejection");
            CheckFailedSeal(recorder, Status::InvalidOperation);
            recorder.Abort();
        }
    }
    const std::array<std::array<int, 4>, 8> invalidRectangles = {{{{-1, 0, 1, 1}}, {{0, -1, 1, 1}},
        {{0, 0, 0, 1}}, {{0, 0, 1, 0}}, {{0, 0, -1, 1}}, {{63, 0, 2, 1}},
        {{1, 1, std::numeric_limits<int>::max(), 1}}, {{0, 64, 1, 1}}}};
    for (const auto& rectangle : invalidRectangles) {
        Begin(recorder);
        auto readback = Read();
        readback.rectangle = rectangle;
        Check(recorder.Readback(readback) == Status::InvalidValue, "invalid readback rectangle");
        CheckFailedSeal(recorder, Status::InvalidValue);
        recorder.Abort();
    }
    for (int invalid = 0; invalid < 2; ++invalid) {
        Begin(recorder);
        auto readback = Read();
        if (invalid == 0) {
            readback.requestId = 0;
        } else {
            readback.stage = static_cast<ReadbackStage>(UINT32_MAX);
        }
        Check(recorder.Readback(readback) == Status::InvalidValue, "invalid ID/stage rejected");
        CheckFailedSeal(recorder, Status::InvalidValue);
        recorder.Abort();
    }
    for (int invalid = 0; invalid < 8; ++invalid) {
        Begin(recorder);
        auto clear = DepthClear();
        if (invalid == 0) {
            clear.mask = 0;
        }
        if (invalid == 1) {
            clear.mask = 8;
        }
        if (invalid == 2) {
            clear.color[0] = std::numeric_limits<float>::infinity();
        }
        if (invalid == 3) {
            clear.depth = std::numeric_limits<double>::quiet_NaN();
        }
        if (invalid == 4) {
            clear.depth = -1;
        }
        if (invalid == 5) {
            clear.scissor[2] = -1;
        }
        if (invalid == 6) {
            clear.depth = 2;
        }
        if (invalid == 7) {
            clear.scissor[3] = -1;
        }
        Check(recorder.Clear(clear) == Status::InvalidValue, "invalid clear rejected");
        CheckFailedSeal(recorder, Status::InvalidValue);
        recorder.Abort();
    }
    Begin(recorder);
    Check(recorder.Readback(Read(3)) == Status::Ok, "accept initial request");
    Check(recorder.Readback(Read(2)) == Status::InvalidValue, "decreasing request within same frame rejected");
    CheckFailedSeal(recorder, Status::InvalidValue);
    recorder.Abort();
    Begin(recorder);
    Check(recorder.Readback(Read(5)) == Status::Ok, "accept request watermark");
    Check(recorder.Readback(Read(5)) == Status::InvalidValue, "duplicate in frame");
    CheckFailedSeal(recorder, Status::InvalidValue);
    recorder.Abort();
    for (std::uint64_t id : {4u, 5u}) {
        Begin(recorder);
        Check(recorder.Readback(Read(id)) == Status::InvalidValue, "accepted ID survives abort");
        recorder.Abort();
    }
    Begin(recorder);
    Check(recorder.Readback(Read(6)) == Status::Ok, "next ID accepted");
    FrameSnapshot sealed;
    Check(recorder.Seal(sealed) == Status::Ok, "seal accepted request");
    for (std::uint64_t id : {5u, 6u}) {
        Begin(recorder);
        Check(recorder.Readback(Read(id)) == Status::InvalidValue, "duplicate/decreasing ID survives seal");
        recorder.Abort();
    }
    Begin(recorder);
    auto presented = Read(std::numeric_limits<std::uint64_t>::max());
    presented.stage = ReadbackStage::PresentedColor;
    Check(recorder.Readback(presented) == Status::Ok, "max ID accepted without wrap");
    recorder.Abort();
    Begin(recorder);
    Check(recorder.Readback(Read(1)) == Status::InvalidValue, "exhausted watermark cannot wrap");
    recorder.Abort();
    Begin(recorder);
    Send(recorder, Geometry());
    Check(recorder.Seal(sealed) == Status::InvalidOperation && sealed.events.size() == 1 &&
        sealed.events[0].readback.requestId == 6, "nonempty output rejected unchanged");
    FrameSnapshot valid;
    Check(recorder.Seal(valid) == Status::Ok && valid.draws.size() == 1, "nonempty output rejection retains frame");
}

void Limits() {
    FrameLimits limits;
    limits.events = 2;
    FrameRecorder recorder(limits);
    Begin(recorder);
    Send(recorder, Geometry());
    Check(recorder.Clear(DepthClear()) == Status::Ok, "second event slot");
    Check(recorder.Readback(Read(7)) == Status::LimitExceeded, "third event bounded independently");
    Check(recorder.Submit(Geometry()) == Status::LimitExceeded, "event failure sticky");
    CheckFailedSeal(recorder, Status::LimitExceeded);
    recorder.Abort();
    Begin(recorder);
    Check(recorder.Readback(Read(7)) == Status::Ok, "failed event append does not consume ID");
    FrameSnapshot output;
    Check(recorder.Seal(output) == Status::Ok && output.events.size() == 1, "no partial prior events survive abort");
    limits.capture.drawPackets = 1;
    limits.events = 8;
    FrameRecorder listRecorder(limits);
    Begin(listRecorder);
    Check(listRecorder.BeginList(kPendingList, false) == Status::Ok, "compile list exceeding draw bound");
    Send(listRecorder, Geometry());
    Send(listRecorder, Geometry());
    Check(listRecorder.EndList() == Status::Ok, "list retained independently of frame draws");
    Check(listRecorder.Submit(Make(Op::CallList, kPendingList)) == Status::LimitExceeded,
        "replay cannot exceed draw limit despite event space");
    CheckFailedSeal(listRecorder, Status::LimitExceeded);
}

void Persistent() {
    FrameRecorder recorder;
    Begin(recorder);
    std::vector<std::uint32_t> generated;
    Check(recorder.GenerateTextures(1, generated) == Status::Ok && generated.size() == 1, "forward generated names");
    auto translation = Make(Op::Translate);
    translation.values = {{2, 3, 4, 0, 0, 0}};
    Send(recorder, translation);
    auto color = Make(Op::Color);
    color.values = {{0.25, 0.5, 0.75, 1, 0, 0}};
    Send(recorder, color);
    Send(recorder, Binding());
    Send(recorder, Image());
    Check(recorder.BeginList(kPendingList, false) == Status::Ok, "persistent list begin");
    Send(recorder, Geometry());
    Check(recorder.EndList() == Status::Ok, "persistent list end");
    FrameSnapshot first;
    Check(recorder.Seal(first) == Status::Ok && first.draws.empty(), "setup-only frame");
    Begin(recorder, 2);
    Send(recorder, Make(Op::CallList, kPendingList));
    FrameSnapshot second;
    Check(recorder.Seal(second) == Status::Ok && second.draws.size() == 1, "list survives seal");
    const auto& draw = second.draws[0];
    Check(draw.modelView.values[12] == 2 && draw.modelView.values[13] == 3 && draw.modelView.values[14] == 4 &&
        draw.vertices[0].color == std::array<float, 4>{{0.25f, 0.5f, 0.75f, 1}}, "matrix/current state survive seal");
    Check(Pixels(second) == Image().upload.bytes && recorder.Textures().IsTexture(kTextureName),
        "texture and binding survive seal");
    Begin(recorder, 3);
    Check(recorder.DeleteList(kPendingList) == Status::Ok, "delete list forwarded");
    Send(recorder, Make(Op::CallList, kPendingList));
    FrameSnapshot third;
    Check(recorder.Seal(third) == Status::Ok && third.draws.empty(), "deleted list no longer replays");
}

void Abort() {
    FrameLimits limits;
    limits.capture.listUploadBytes = kImageBytes;
    limits.capture.storedVertices = kTriangleVertices;
    FrameRecorder recorder(limits);
    Begin(recorder);
    Send(recorder, Binding());
    Send(recorder, Image());
    Check(recorder.BeginList(kPendingList, false) == Status::Ok, "prior completed list");
    Send(recorder, Geometry());
    Check(recorder.EndList() == Status::Ok, "completed list retained");
    Send(recorder, Make(Op::CallList, kPendingList));
    FrameSnapshot older;
    Check(recorder.Seal(older) == Status::Ok, "older snapshot");
    for (bool list : {false, true}) {
        Begin(recorder);
        auto color = Make(Op::Color);
        color.values = {{0.5, 0.25, 1, 1, 0, 0}};
        Send(recorder, color);
        Send(recorder, Image());
        const auto retainedBytes = recorder.Textures().StorageBytes();
        if (list) {
            Check(recorder.BeginList(kPendingList + 1, false) == Status::Ok, "pending upload list");
            Send(recorder, Image());
            Send(recorder, Geometry());
        } else {
            Send(recorder, Make(Op::Begin));
            Send(recorder, Make(Op::Vertex));
        }
        recorder.Abort();
        Check(recorder.Textures().StorageBytes() == retainedBytes, "abort keeps committed texture bytes only");
        Begin(recorder);
        Send(recorder, Make(Op::CallList, kPendingList));
        Check(recorder.BeginList(kPendingList + 1, false) == Status::Ok, "aborted list charges released");
        Send(recorder, Image());
        Check(recorder.EndList() == Status::Ok, "full upload budget reusable after abort");
        Check(recorder.DeleteList(kPendingList + 1) == Status::Ok, "release recovery upload list");
        FrameSnapshot next;
        Check(recorder.Seal(next) == Status::Ok && next.draws.size() == 1, "valid next frame after abort");
        Check(next.draws[0].vertices[0].color[0] == 0.5f && Pixels(older) == Image().upload.bytes,
            "state mutations persist and older snapshot is intact");
    }
}

class ScopedFault {
public:
    explicit ScopedFault(CaptureAllocationSite site, std::size_t failOn = 1) : site_(site), failOn_(failOn),
        previous_(ActiveCaptureAllocationHook()), previousOwner_(owner_) {
        owner_ = this;
        ActiveCaptureAllocationHook() = &Hit;
    }
    ~ScopedFault() {
        ActiveCaptureAllocationHook() = previous_;
        owner_ = previousOwner_;
    }
    std::size_t Hits() const {
        return hits_;
    }
    std::size_t AllHits() const {
        return allHits_;
    }
private:
    static void Hit(CaptureAllocationSite site) {
        ++owner_->allHits_;
        if (site == owner_->site_ && ++owner_->hits_ == owner_->failOn_) {
            throw std::bad_alloc();
        }
    }
    CaptureAllocationSite site_;
    std::size_t failOn_;
    std::size_t hits_ = 0;
    std::size_t allHits_ = 0;
    CaptureAllocationHook previous_;
    ScopedFault* previousOwner_;
    static thread_local ScopedFault* owner_;
};
thread_local ScopedFault* ScopedFault::owner_ = nullptr;

void LaterAllocationHits() {
    struct FailureCase {
        CaptureAllocationSite site;
        const char* name;
        std::size_t nth;
    };
    const std::array<FailureCase, 4> cases = {{{CaptureAllocationSite::ListUploadCopy, "ListUploadCopy", 2},
        {CaptureAllocationSite::TextureImageStorage, "TextureImageStorage", 2},
        {CaptureAllocationSite::TextureImageStorage, "TextureImageStorage", 3},
        {CaptureAllocationSite::EventGrowthAfterDraw, "EventGrowthAfterDraw", 2}}};
    for (const auto& failure : cases) {
        FrameRecorder recorder;
        Begin(recorder);
        Send(recorder, Binding());
        Send(recorder, Image());
        Send(recorder, Geometry());
        FrameSnapshot older;
        Check(recorder.Seal(older) == Status::Ok, "later-hit snapshot");
        const auto baselineBytes = recorder.Textures().StorageBytes();
        Begin(recorder, 2);
        if (failure.site == CaptureAllocationSite::ListUploadCopy) {
            Check(recorder.BeginList(kPendingList, false) == Status::Ok, "later upload-list copy");
        }
        const bool eventGrowth = failure.site == CaptureAllocationSite::EventGrowthAfterDraw;
        const auto command = eventGrowth ? Geometry() : Image();
        {
            ScopedFault fault(failure.site, failure.nth);
            if (eventGrowth) {
                Send(recorder, command);
            }
            Check(recorder.Submit(command) == Status::LimitExceeded, "later ownership allocation fails deterministically");
            Check(fault.Hits() == failure.nth, "requested Nth allocation actually hit");
            CheckFailedSeal(recorder, Status::LimitExceeded);
            const auto hits = fault.AllHits();
            recorder.Abort();
            Check(fault.AllHits() == hits, "later-hit abort is allocation-free");
            std::printf("ALLOCATION_NTH_HIT %s %zu\n", failure.name, fault.Hits());
        }
        Check(recorder.Textures().StorageBytes() == baselineBytes && Pixels(older) == Image().upload.bytes,
            "partial ownership allocation unwinds charges and preserves older image");
        Begin(recorder, 3);
        Send(recorder, Geometry());
        FrameSnapshot next;
        Check(recorder.Seal(next) == Status::Ok, "later-hit recorder recovers after abort");
    }
}

void AllocationFailure() {
    const std::array<CaptureAllocationSite, 6> sites = {{CaptureAllocationSite::EventGrowthAfterDraw,
        CaptureAllocationSite::PendingListStorage, CaptureAllocationSite::ListUploadCopy,
        CaptureAllocationSite::TextureImageStorage, CaptureAllocationSite::TextureObjectVersion,
        CaptureAllocationSite::DrawSnapshotCopy}};
    const std::array<const char*, 6> names = {{"EventGrowthAfterDraw", "PendingListStorage", "ListUploadCopy",
        "TextureImageStorage", "TextureObjectVersion", "DrawSnapshotCopy"}};
    for (std::size_t index = 0; index < sites.size(); ++index) {
        FrameLimits limits;
        limits.capture.listUploadBytes = kImageBytes;
        FrameRecorder recorder(limits);
        Begin(recorder);
        Send(recorder, Binding());
        Send(recorder, Image());
        Send(recorder, Geometry());
        FrameSnapshot older;
        Check(recorder.Seal(older) == Status::Ok, "allocation case older snapshot");
        const auto retainedBytes = recorder.Textures().StorageBytes();
        Begin(recorder, 2);
        if (sites[index] == CaptureAllocationSite::PendingListStorage ||
            sites[index] == CaptureAllocationSite::ListUploadCopy) {
            Check(recorder.BeginList(kPendingList, false) == Status::Ok, "fault pending list");
        }
        const bool upload = sites[index] == CaptureAllocationSite::ListUploadCopy ||
            sites[index] == CaptureAllocationSite::TextureImageStorage ||
            sites[index] == CaptureAllocationSite::TextureObjectVersion;
        const auto command = upload ? Image() : Geometry();
        {
            ScopedFault fault(sites[index]);
            Check(recorder.Submit(command) == Status::LimitExceeded, "allocation converted to sticky status");
            Check(fault.Hits() == 1, "named allocation seam actually hit");
            Check(recorder.Submit(Make(Op::End)) == Status::LimitExceeded, "first failure remains sticky");
            CheckFailedSeal(recorder, Status::LimitExceeded);
            FrameSnapshot sentinel;
            sentinel.info.serial = kSentinelSerial;
            sentinel.events.resize(1);
            Check(recorder.Seal(sentinel) == Status::LimitExceeded && sentinel.info.serial == kSentinelSerial &&
                sentinel.events.size() == 1, "first failure wins over nonempty sentinel output");
            const auto checkpointHits = fault.AllHits();
            recorder.Abort();
            Check(fault.AllHits() == checkpointHits, "abort invokes no allocation checkpoints");
            std::printf("ALLOCATION_HIT %s %zu\n", names[index], fault.Hits());
        }
        Check(recorder.Textures().StorageBytes() == retainedBytes, "failed allocation releases transient image charge");
        Check(Pixels(older) == Image().upload.bytes, "allocation failure leaves older snapshot intact");
        Begin(recorder, 3);
        Check(recorder.BeginList(kPendingList, false) == Status::Ok, "retry list after allocation abort");
        Send(recorder, Image());
        Send(recorder, Geometry());
        Check(recorder.EndList() == Status::Ok, "list accounting restored");
        Send(recorder, Make(Op::CallList, kPendingList));
        FrameSnapshot next;
        Check(recorder.Seal(next) == Status::Ok && next.draws.size() == 1, "valid next frame after disabled fault");
    }
    {
        ScopedFault fault(CaptureAllocationSite::RecorderConstruction);
        bool threw = false;
        try {
            FrameRecorder recorder;
        } catch (const std::bad_alloc&) {
            threw = true;
        }
        Check(threw && fault.Hits() == 1, "constructor allocation propagates to owner");
        std::printf("ALLOCATION_HIT RecorderConstruction %zu\n", fault.Hits());
    }
    LaterAllocationHits();
    FrameRecorder fresh;
    Begin(fresh);
    Send(fresh, Geometry());
    FrameSnapshot next;
    Check(fresh.Seal(next) == Status::Ok, "construction fault leaves next owner usable");
}

}

int main(int argc, char** argv) {
    Check(argc == 2, "one frame case required");
    const std::string name = argv[1];
    if (name == "order") {
        Order();
    } else if (name == "owned") {
        Owned();
    } else if (name == "boundaries") {
        Boundaries();
    } else if (name == "limits") {
        Limits();
    } else if (name == "abort") {
        Abort();
    } else if (name == "persistent") {
        Persistent();
    } else if (name == "allocation-failure") {
        AllocationFailure();
    } else {
        Check(false, "unknown frame case");
    }
    std::printf("PASS frame-%s\n", name.c_str());
    return EXIT_SUCCESS;
}
