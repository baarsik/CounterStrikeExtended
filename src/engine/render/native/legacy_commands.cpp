#include "legacy_commands.h"
#include "surface_mesh.h"
#include "capture_allocation.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <tuple>
#include <cstring>

namespace cse {
namespace render {
namespace {

bool ValidPrimitive(std::uint32_t value) {
    return value <= static_cast<std::uint32_t>(Primitive::LineLoop);
}

std::size_t GeometryCost(const Command& command) {
    return std::max(command.vertices.size(), command.indices.size());
}

bool Integral(double value, int& result) {
    if (!std::isfinite(value) || value < std::numeric_limits<int>::min()
        || value > std::numeric_limits<int>::max() || value != std::floor(value)) {
        return false;
    }
    result = static_cast<int>(value);
    return true;
}

bool NonfiniteProjection(const ScalarCommand& command) {
    return (command.op == Op::Ortho || command.op == Op::Frustum) &&
        std::any_of(command.values.begin(), command.values.end(),
            [](double value) {
                return !std::isfinite(value);
            });
}

Status ValidateScalarArguments(const ScalarCommand& command) {
    std::size_t floats = 0;
    switch (command.op) {
    case Op::MatrixMode:
        return command.argument <= static_cast<std::uint32_t>(MatrixMode::Texture)
            ? Status::Ok : Status::InvalidValue;
    case Op::ActiveTexture:
    case Op::TexCoord:
        if (command.argument >= kTextureUnits) {
            return Status::InvalidValue;
        }
        floats = command.op == Op::TexCoord ? 2 : 0;
        break;
    case Op::Begin:
        return ValidPrimitive(command.argument) ? Status::Ok : Status::InvalidValue;
    case Op::Translate:
    case Op::Scale:
    case Op::Vertex:
        floats = 3;
        break;
    case Op::Color:
    case Op::Rotate:
        floats = 4;
        break;
    case Op::ShadeModel:
        return command.argument <= static_cast<std::uint32_t>(Shade::Smooth) ? Status::Ok : Status::InvalidValue;
    case Op::Enable:
        return command.argument <= static_cast<std::uint32_t>(Capability::SharedTexturePalette)
            ? Status::Ok : Status::InvalidValue;
    case Op::DepthFunction:
    case Op::AlphaFunction:
        if (command.argument > static_cast<std::uint32_t>(Compare::Always)) {
            return Status::InvalidValue;
        }
        floats = command.op == Op::AlphaFunction ? 1 : 0;
        break;
    case Op::BlendFunction:
        return command.argument <= static_cast<std::uint32_t>(BlendFactor::InverseDestinationColor)
            && command.second <= static_cast<std::uint32_t>(BlendFactor::InverseDestinationColor)
            ? Status::Ok : Status::InvalidValue;
    case Op::CullFace:
        return command.argument <= static_cast<std::uint32_t>(Cull::FrontAndBack) ? Status::Ok : Status::InvalidValue;
    case Op::BindTexture:
        return command.argument <= static_cast<std::uint32_t>(TextureTarget::Rectangle) ?
            Status::Ok : Status::InvalidValue;
    case Op::TextureParameter:
        if (command.argument > static_cast<std::uint32_t>(TextureTarget::Rectangle) ||
            command.second > static_cast<std::uint32_t>(TextureParameter::Anisotropy)) {
            return Status::InvalidValue;
        }
        break;
    case Op::TextureEnvironment:
        if ((command.argument == static_cast<std::uint32_t>(EnvironmentParameter::Mode) &&
            command.second <= static_cast<std::uint32_t>(EnvironmentMode::Combine)) ||
            (command.argument == static_cast<std::uint32_t>(EnvironmentParameter::CombineRGB) && command.second == 0) ||
            (command.argument == static_cast<std::uint32_t>(EnvironmentParameter::RGBScale) &&
            (command.second == 1 || command.second == 2 || command.second == 4))) {
            return Status::Ok;
        }
        return Status::InvalidValue;
    case Op::PixelStore: {
        int value = 0;
        if (command.argument > static_cast<std::uint32_t>(PixelStoreParameter::PackAlignment) ||
            !Integral(command.values[0], value)) {
            return Status::InvalidValue;
        }
        break;
    }
    case Op::FogMode:
        return command.argument <= static_cast<std::uint32_t>(FogMode::Linear) ? Status::Ok : Status::InvalidValue;
    case Op::FogHint:
    case Op::PerspectiveHint:
        return command.argument <= static_cast<std::uint32_t>(HintQuality::Nicest) ? Status::Ok : Status::InvalidValue;
    case Op::FogColor:
        floats = 4;
        break;
    case Op::FogDensity:
    case Op::FogStart:
    case Op::FogEnd:
        floats = 1;
        break;
    case Op::FrontFace:
        return command.argument <= 1 ? Status::Ok : Status::InvalidValue;
    case Op::PolygonMode:
        return command.argument <= static_cast<std::uint32_t>(Cull::FrontAndBack) &&
            command.second <= static_cast<std::uint32_t>(PolygonMode::Fill) ? Status::Ok : Status::InvalidValue;
    case Op::PolygonOffset:
        floats = 2;
        break;
    case Op::Viewport:
    case Op::Scissor:
        for (std::size_t index = 0; index < 4; ++index) {
            int value = 0;
            if (!Integral(command.values[index], value) || (index >= 2 && value < 0)) {
                return Status::InvalidValue;
            }
        }
        break;
    case Op::Ortho:
    case Op::Frustum:
        if (NonfiniteProjection(command)) {
            break;
        }
        if (command.values[0] == command.values[1] || command.values[2] == command.values[3]
            || command.values[4] == command.values[5]) {
            return Status::InvalidValue;
        }
        if (command.op == Op::Frustum && (command.values[4] <= 0 || command.values[5] <= 0)) {
            return Status::InvalidValue;
        }
        break;
    case Op::ColorWrite:
    case Op::Identity:
    case Op::Push:
    case Op::Pop:
    case Op::DepthWrite:
    case Op::DepthRange:
    case Op::End:
    case Op::CallList:
        break;
    default:
        return Status::InvalidValue;
    }
    for (std::size_t index = 0; index < floats; ++index) {
        const double value = command.values[index];
        if (std::isfinite(value) && std::fabs(value) > std::numeric_limits<float>::max()) {
            return Status::InvalidValue;
        }
    }
    return Status::Ok;
}

Status ValidateArguments(const Command& command) {
    switch (command.op) {
    case Op::TextureImage:
    case Op::TextureSubImage:
        if (command.argument > static_cast<std::uint32_t>(TextureTarget::Rectangle)) {
            return Status::InvalidValue;
        }
        return TextureStore::ValidateUpload(command.upload, command.unpack, command.op == Op::TextureSubImage);
    case Op::SharedPalette:
        return command.upload.bytes.size() == kPaletteBytes ? Status::Ok : Status::InvalidValue;
    case Op::DeleteTextures:
        return command.names.size() <= kTextureNames ? Status::Ok : Status::LimitExceeded;
    case Op::DrawArrays:
    case Op::DrawNative:
        return ValidPrimitive(command.argument) ? Status::Ok : Status::InvalidValue;
    case Op::LoadMatrix:
    case Op::MultiplyMatrix:
        // Retain nonfinite transforms for primitive-local filtering during draw preparation.
        return Status::Ok;
    default:
        return ValidateScalarArguments(command);
    }
}

bool AttributeOperation(Op operation) {
    return operation == Op::Color || operation == Op::TexCoord || operation == Op::Vertex
        || operation == Op::End || operation == Op::CallList;
}

}

Recorder::Recorder(CaptureLimits limits) : limits_(limits),
    textures_(limits.textureStorageBytes, limits.textureNames) {
}

bool Recorder::AtFrameBoundary() const {
    return !begin_ && !compiling_;
}

Status Recorder::TakeFrameDraws(std::vector<DrawPacket>& output) {
    EndQuadBatch();
    if (!output.empty() || !AtFrameBoundary()) {
        return Status::InvalidOperation;
    }
    output.swap(draws_);
    frameVertices_ = 0;
    return Status::Ok;
}

void Recorder::AbortPendingFrame() {
    EndQuadBatch();
    ClearDraws();
    // Destroy pending ownership without constructing a new recorder or allocating.
    std::vector<Command>().swap(pendingList_);
    pendingDraw_ = DrawPacket{};
    pendingVertices_ = 0;
    pendingUploadBytes_ = 0;
    compiling_ = 0;
    executeList_ = false;
    compileBegin_ = false;
    begin_ = false;
}

bool Recorder::MatchesQuadState(const DrawPacket& draw) const {
    // Compare floating representations, including signed zero. Do not compare structure padding.
    const auto sameScalarBits = [](const auto& first, const auto& second) {
        return std::memcmp(&first, &second, sizeof(first)) == 0;
    };
    const auto sameArrayBits = [](const auto& first, const auto& second) {
        return std::memcmp(first.data(), second.data(), first.size() * sizeof(first.front())) == 0;
    };
    const auto rasterKey = [](const RasterState& raster) {
        return std::tie(raster.enabled, raster.depthWrite, raster.colorWrite, raster.depthFunction,
            raster.alphaFunction, raster.blendSource, raster.blendDestination, raster.cull, raster.shade,
            raster.viewport, raster.scissor, raster.fogMode, raster.frontCounterClockwise, raster.polygonMode,
            raster.fogHint, raster.perspectiveHint);
    };
    const auto& raster = draw.raster;
    const auto& current = state_.raster;
    const bool sameFloatingRaster = sameScalarBits(raster.alphaReference, current.alphaReference) &&
        sameArrayBits(raster.depthRange, current.depthRange) && sameArrayBits(raster.fogColor, current.fogColor) &&
        sameScalarBits(raster.fogDensity, current.fogDensity) && sameScalarBits(raster.fogStart, current.fogStart) &&
        sameScalarBits(raster.fogEnd, current.fogEnd) && sameArrayBits(raster.polygonOffset, current.polygonOffset);
    if (!sameFloatingRaster || rasterKey(draw.raster) != rasterKey(state_.raster) ||
        !sameArrayBits(draw.modelView.values, state_.Matrix(MatrixMode::ModelView).values) ||
        !sameArrayBits(draw.projection.values, state_.Matrix(MatrixMode::Projection).values) ||
        draw.sharedTexturePalette != state_.sharedTexturePalette || draw.palette != textures_.PaletteSnapshot()) {
        return false;
    }
    const auto resources = textures_.Capture(state_);
    for (std::size_t unit = 0; unit < kTextureUnits; ++unit) {
        const auto textureKey = [](const TextureUnitState& texture) {
            return std::tie(texture.enabled2D, texture.enabledRectangle, texture.texture2D,
                texture.textureRectangle, texture.environment, texture.rgbScale);
        };
        if (textureKey(draw.textures[unit]) != textureKey(state_.textures[unit]) ||
            !sameArrayBits(draw.textureMatrices[unit].values, state_.Matrix(MatrixMode::Texture, unit).values) ||
            draw.resources[unit].texture2D != resources[unit].texture2D ||
            draw.resources[unit].rectangle != resources[unit].rectangle) {
            return false;
        }
    }
    return true;
}

DrawPacket Recorder::Snapshot(Primitive primitive) const {
    DrawPacket draw;
    draw.primitive = primitive;
    draw.shade = state_.raster.shade;
    draw.modelView = state_.Matrix(MatrixMode::ModelView);
    draw.projection = state_.Matrix(MatrixMode::Projection);
    for (std::size_t unit = 0; unit < kTextureUnits; ++unit) {
        draw.textureMatrices[unit] = state_.Matrix(MatrixMode::Texture, unit);
    }
    draw.raster = state_.raster;
    draw.textures = state_.textures;
    draw.resources = textures_.Capture(state_);
    draw.palette = textures_.PaletteSnapshot();
    draw.sharedTexturePalette = state_.sharedTexturePalette;
    return draw;
}

Status Recorder::GenerateLists(std::size_t count, std::uint32_t& first) {
    if (!AtFrameBoundary()) {
        return Status::InvalidOperation;
    }
    if (!count) {
        first = 0;
        return Status::Ok;
    }
    if (count > limits_.lists - lists_.size()) {
        return Status::LimitExceeded;
    }
    std::uint32_t candidate = 1;
    for (const auto& list : lists_) {
        if (list.first >= candidate && static_cast<std::uint64_t>(list.first) - candidate < count) {
            if (list.first == std::numeric_limits<std::uint32_t>::max()) {
                return Status::LimitExceeded;
            }
            candidate = list.first + 1;
        }
    }
    if (count - 1 > std::numeric_limits<std::uint32_t>::max() - candidate) {
        return Status::LimitExceeded;
    }
    // Reserve empty list definitions in the same namespace as explicit NewList identifiers.
    // Allocation failure is handled by the frame owner; remove only this transaction's additions.
    std::size_t reserved = 0;
    try {
        for (; reserved < count; ++reserved) {
            lists_.emplace(candidate + static_cast<std::uint32_t>(reserved), std::vector<Command>{});
        }
    } catch (...) {
        for (std::size_t index = 0; index < reserved; ++index) {
            lists_.erase(candidate + static_cast<std::uint32_t>(index));
        }
        throw;
    }
    first = candidate;
    return Status::Ok;
}

Status Recorder::BeginList(std::uint32_t id, bool execute) {
    if (compiling_ || begin_) {
        return Status::InvalidOperation;
    }
    if (!id) {
        return Status::InvalidValue;
    }
    if (lists_.find(id) == lists_.end() && lists_.size() >= limits_.lists) {
        return Status::LimitExceeded;
    }
    pendingList_.clear();
    pendingVertices_ = 0;
    pendingUploadBytes_ = 0;
    compiling_ = id;
    executeList_ = execute;
    compileBegin_ = false;
    return Status::Ok;
}

Status Recorder::EndList() {
    if (!compiling_ || compileBegin_ || begin_) {
        return Status::InvalidOperation;
    }
    const auto old = lists_.find(compiling_);
    std::size_t oldVertices = 0;
    std::size_t oldBytes = 0;
    const std::size_t oldCommands = old == lists_.end() ? 0 : old->second.size();
    if (old != lists_.end()) {
        for (const auto& command : old->second) {
            oldVertices += GeometryCost(command);
            oldVertices += command.op == Op::Vertex ? 1 : 0;
            oldBytes += command.upload.bytes.size();
        }
    }
    const auto remainingVertices = limits_.storedVertices - (storedVertices_ - oldVertices);
    const auto remainingCommands = limits_.listCommands - (storedCommands_ - oldCommands);
    if (pendingVertices_ > remainingVertices || pendingList_.size() > remainingCommands ||
        pendingUploadBytes_ > limits_.listUploadBytes - (storedUploadBytes_ - oldBytes)) {
        return Status::LimitExceeded;
    }
    if (old == lists_.end()) {
        BeforeCaptureAllocation(CaptureAllocationSite::PendingListStorage);
    }
    lists_[compiling_] = std::move(pendingList_);
    storedVertices_ = storedVertices_ - oldVertices + pendingVertices_;
    storedCommands_ = storedCommands_ - oldCommands + lists_[compiling_].size();
    storedUploadBytes_ = storedUploadBytes_ - oldBytes + pendingUploadBytes_;
    compiling_ = 0;
    return Status::Ok;
}

Status Recorder::DeleteList(std::uint32_t id) {
    if (begin_) {
        return Status::InvalidOperation;
    }
    const auto old = lists_.find(id);
    if (old != lists_.end()) {
        for (const auto& command : old->second) {
            storedVertices_ -= GeometryCost(command);
            storedVertices_ -= command.op == Op::Vertex ? 1 : 0;
            storedUploadBytes_ -= command.upload.bytes.size();
        }
        storedCommands_ -= old->second.size();
        lists_.erase(old);
    }
    return Status::Ok;
}

Status Recorder::Submit(const Command& input) {
    if (input.upload.bytes.size() > kTextureUploadBytes || input.names.size() > kTextureNames ||
        (compiling_ && input.upload.bytes.size() > limits_.listUploadBytes - pendingUploadBytes_)) {
        return Status::LimitExceeded;
    }
    if (!input.indices.empty()) {
        constexpr std::size_t kIndicesPerTriangle = 3;
        if (input.op != Op::DrawNative || input.argument != static_cast<std::uint32_t>(Primitive::Triangles) ||
            input.indices.size() % kIndicesPerTriangle != 0) {
            return Status::InvalidOperation;
        }
        if (input.indices.size() > limits_.verticesPerDraw) {
            return Status::LimitExceeded;
        }
        for (const auto index : input.indices) {
            if (index >= input.vertices.size()) {
                return Status::InvalidValue;
            }
        }
    }
    Command captured;
    const bool upload = input.op == Op::TextureImage || input.op == Op::TextureSubImage;
    if (upload) {
        if (compiling_ && !input.upload.bytes.empty()) {
            BeforeCaptureAllocation(CaptureAllocationSite::ListUploadCopy);
        }
        captured = input;
        captured.unpack = textures_.Unpack();
    }
    const auto& command = upload ? captured : input;
    const auto validation = ValidateArguments(command);
    if (validation != Status::Ok) {
        return validation;
    }
    if (command.op == Op::PixelStore || command.op == Op::DeleteTextures) {
        // OpenGL executes these immediately, even in GL_COMPILE lists; do not record them.
        if (compileBegin_) {
            return Status::InvalidOperation;
        }
        std::size_t budget = limits_.replayCommands;
        return Execute(command, 0, budget);
    }
    if (compiling_) {
        if (pendingList_.size() >= limits_.listCommands) {
            return Status::LimitExceeded;
        }
        if ((command.op == Op::Begin && compileBegin_)
            || (command.op == Op::End && !compileBegin_)
            || (command.op == Op::Vertex && !compileBegin_)
            || (compileBegin_ && !AttributeOperation(command.op))) {
            return Status::InvalidOperation;
        }
        if (command.op == Op::Begin && !ValidPrimitive(command.argument)) {
            return Status::InvalidValue;
        }
        const auto addition = GeometryCost(command) + (command.op == Op::Vertex ? 1 : 0);
        if (command.vertices.size() > limits_.verticesPerDraw
            || addition > limits_.storedVertices - pendingVertices_) {
            return Status::LimitExceeded;
        }
        if (!executeList_) {
            if (pendingList_.size() == pendingList_.capacity()) {
                BeforeCaptureAllocation(CaptureAllocationSite::PendingListStorage);
            }
            if (!command.upload.bytes.empty()) {
                BeforeCaptureAllocation(CaptureAllocationSite::ListUploadCopy);
            }
            pendingList_.push_back(command);
            pendingVertices_ += addition;
            pendingUploadBytes_ += command.upload.bytes.size();
            if (command.op == Op::Begin || command.op == Op::End) {
                compileBegin_ = command.op == Op::Begin;
            }
            return Status::Ok;
        }
    }
    std::size_t budget = limits_.replayCommands;
    const auto result = Execute(command, 0, budget);
    if (compiling_ && result == Status::Ok) {
        if (pendingList_.size() == pendingList_.capacity()) {
            BeforeCaptureAllocation(CaptureAllocationSite::PendingListStorage);
        }
        if (!command.upload.bytes.empty()) {
            BeforeCaptureAllocation(CaptureAllocationSite::ListUploadCopy);
        }
        pendingList_.push_back(command);
        pendingVertices_ += GeometryCost(command) + (command.op == Op::Vertex ? 1 : 0);
        pendingUploadBytes_ += command.upload.bytes.size();
        if (command.op == Op::Begin || command.op == Op::End) {
            compileBegin_ = command.op == Op::Begin;
        }
    }
    return result;
}

Status Recorder::SubmitScalar(const ScalarCommand& command) {
    const auto validation = ValidateScalarArguments(command);
    if (validation != Status::Ok) {
        return validation;
    }
    if (compiling_) {
        // Lists own complete commands and keep the same compile-only/immediate-operation rules.
        Command stored;
        static_cast<ScalarCommand&>(stored) = command;
        return Submit(stored);
    }
    std::size_t budget = limits_.replayCommands;
    return ExecuteScalar(command, 0, budget);
}

Status Recorder::SubmitQuad(const std::array<Vertex, 4>& vertices) {
    if (!AtFrameBoundary()) {
        return Status::InvalidOperation;
    }
    if (vertices.size() > limits_.verticesPerDraw) {
        return Status::LimitExceeded;
    }
    if (vertices.size() > limits_.frameVertices - frameVertices_) {
        return Status::LimitExceeded;
    }
    // Compare the current state before allocating/copying a full packet. Colors and current UVs
    // belong to individual vertices, so text with changing colors remains one ordered quad batch.
    const bool append = quadBatchOpen_ && !draws_.empty() &&
        draws_.back().vertices.size() <= limits_.verticesPerDraw - vertices.size() &&
        MatchesQuadState(draws_.back());
    std::array<Vertex, 4> captured = vertices;
    for (auto& vertex : captured) {
        vertex.color = state_.Current().color;
        for (std::size_t unit = 1; unit < kTextureUnits; ++unit) {
            vertex.texcoords[unit] = state_.Current().texcoords[unit];
        }
    }
    auto result = Status::Ok;
    if (append) {
        auto& target = draws_.back().vertices;
        target.insert(target.end(), captured.begin(), captured.end());
        frameVertices_ += captured.size();
    } else {
        auto draw = Snapshot(Primitive::Quads);
        draw.vertices.assign(captured.begin(), captured.end());
        result = AppendDraw(draw);
    }
    if (result == Status::Ok) {
        quadBatchOpen_ = true;
        state_.TexCoord(0, vertices.back().texcoords[0][0], vertices.back().texcoords[0][1]);
    }
    return result;
}

Status Recorder::SubmitIndexed(const std::vector<Vertex>& vertices,
    const std::vector<std::uint32_t>& indices, bool perVertexColor) {
    constexpr std::size_t kIndicesPerTriangle = 3;
    if (!AtFrameBoundary() || indices.size() % kIndicesPerTriangle != 0) {
        return Status::InvalidOperation;
    }
    if (vertices.size() > limits_.verticesPerDraw || indices.size() > limits_.verticesPerDraw) {
        return Status::LimitExceeded;
    }
    for (const auto index : indices) {
        if (index >= vertices.size()) {
            return Status::InvalidValue;
        }
    }
    if (indices.empty()) {
        return Status::Ok;
    }
    if (!limits_.replayCommands) {
        return Status::LimitExceeded;
    }
    return RecordArrayDraw(Primitive::Triangles, vertices, indices, perVertexColor);
}

Status Recorder::RecordArrayDraw(Primitive primitive, const std::vector<Vertex>& vertices,
    const std::vector<std::uint32_t>& indices, bool perVertexColor) {
    if (vertices.empty()) {
        return Status::Ok;
    }
    auto draw = Snapshot(primitive);
    BeforeCaptureAllocation(CaptureAllocationSite::DrawSnapshotCopy);
    draw.vertices = vertices;
    if (!indices.empty()) {
        BeforeCaptureAllocation(CaptureAllocationSite::DrawSnapshotCopy);
        draw.indices = indices;
    }
    for (auto& vertex : draw.vertices) {
        if (!perVertexColor) {
            vertex.color = state_.Current().color;
        }
        for (std::size_t unit = 1; unit < kTextureUnits; ++unit) {
            vertex.texcoords[unit] = state_.Current().texcoords[unit];
        }
    }
    return AppendDraw(draw);
}

Status Recorder::SubmitSurface(const std::shared_ptr<const SurfaceMesh>& mesh, float scroll, bool detail) {
    if (!AtFrameBoundary()) {
        return Status::InvalidOperation;
    }
    if (!mesh) {
        return Status::InvalidValue;
    }
    if (std::max(mesh->Vertices().size(), mesh->Indices().size()) > limits_.verticesPerDraw) {
        return Status::LimitExceeded;
    }
    auto draw = Snapshot(Primitive::Polygon);
    draw.mesh = mesh;
    draw.meshColor = state_.Current().color;
    draw.meshUvOffset[0][0] = scroll;
    draw.meshUvOffset[2][0] = detail ? scroll : 0;
    const auto result = AppendDraw(draw);
    if (result != Status::Ok) {
        return result;
    }
    const auto& last = mesh->Vertices().back();
    state_.TexCoord(0, last.texcoords[0][0] + scroll, last.texcoords[0][1]);
    state_.TexCoord(1, last.texcoords[1][0], last.texcoords[1][1]);
    if (detail) {
        state_.TexCoord(2, last.texcoords[2][0] + scroll, last.texcoords[2][1]);
    }
    return Status::Ok;
}

Status Recorder::AppendDraw(DrawPacket& draw) {
    EndQuadBatch();
    const auto vertices = draw.mesh ? std::max(draw.mesh->Vertices().size(), draw.mesh->Indices().size()) :
        std::max(draw.vertices.size(), draw.indices.size());
    if (draws_.size() >= limits_.drawPackets || vertices > limits_.frameVertices - frameVertices_) {
        return Status::LimitExceeded;
    }
    draws_.push_back(std::move(draw));
    frameVertices_ += vertices;
    return Status::Ok;
}

Status Recorder::Execute(const Command& command, std::size_t depth, std::size_t& budget) {
    switch (command.op) {
    case Op::LoadMatrix:
    case Op::MultiplyMatrix:
    case Op::DeleteTextures:
    case Op::TextureImage:
    case Op::TextureSubImage:
    case Op::SharedPalette:
    case Op::DrawArrays:
    case Op::DrawNative:
        break;
    default:
        return ExecuteScalar(command, depth, budget);
    }
    if (!budget || depth > limits_.replayDepth) {
        return Status::LimitExceeded;
    }
    --budget;
    if (begin_) {
        return Status::InvalidOperation;
    }
    switch (command.op) {
    case Op::LoadMatrix:
        state_.LoadMatrix(command.matrix);
        break;
    case Op::MultiplyMatrix:
        state_.MultiplyMatrix(command.matrix);
        break;
    case Op::DeleteTextures:
        textures_.Delete(state_, command.names);
        break;
    case Op::TextureImage:
    case Op::TextureSubImage:
        return textures_.Upload(state_, static_cast<TextureTarget>(command.argument), command.upload,
            command.unpack, command.op == Op::TextureSubImage);
    case Op::SharedPalette:
        return textures_.Palette(command.upload.bytes);
    case Op::DrawArrays:
    case Op::DrawNative: {
        if (!ValidPrimitive(command.argument)) {
            return Status::InvalidValue;
        }
        if (command.vertices.size() > limits_.verticesPerDraw) {
            return Status::LimitExceeded;
        }
        return RecordArrayDraw(static_cast<Primitive>(command.argument), command.vertices, command.indices,
            command.op == Op::DrawNative && command.second != 0);
    }
    default:
        return Status::InvalidValue;
    }
    return Status::Ok;
}

Status Recorder::ExecuteScalar(const ScalarCommand& command, std::size_t depth, std::size_t& budget) {
    if (!budget || depth > limits_.replayDepth) {
        return Status::LimitExceeded;
    }
    --budget;
    if (begin_ && !AttributeOperation(command.op)) {
        return Status::InvalidOperation;
    }
    const auto& v = command.values;
    const auto f = [&v](std::size_t index) {
        return static_cast<float>(v[index]);
    };
    switch (command.op) {
    case Op::MatrixMode:
        return state_.SelectMatrix(static_cast<MatrixMode>(command.argument));
    case Op::ActiveTexture:
        return state_.SelectTexture(command.argument);
    case Op::ColorWrite:
        for (std::size_t channel = 0; channel < state_.raster.colorWrite.size(); ++channel) {
            state_.raster.colorWrite[channel] = v[channel] != 0;
        }
        break;
    case Op::Identity:
        state_.LoadIdentity();
        break;
    case Op::Push:
        return state_.Push();
    case Op::Pop:
        return state_.Pop();
    case Op::Translate:
        state_.Translate(f(0), f(1), f(2));
        break;
    case Op::Scale:
        state_.Scale(f(0), f(1), f(2));
        break;
    case Op::Rotate:
        state_.Rotate(f(0), f(1), f(2), f(3));
        break;
    case Op::Ortho:
    case Op::Frustum:
        if (NonfiniteProjection(command)) {
            Matrix4 invalid;
            invalid.values.fill(std::numeric_limits<float>::quiet_NaN());
            state_.MultiplyMatrix(invalid);
            break;
        }
        return command.op == Op::Ortho ? state_.Ortho(v[0], v[1], v[2], v[3], v[4], v[5]) :
            state_.Frustum(v[0], v[1], v[2], v[3], v[4], v[5]);
    case Op::Color:
        state_.Color(f(0), f(1), f(2), f(3));
        break;
    case Op::TexCoord:
        return state_.TexCoord(command.argument, f(0), f(1));
    case Op::ShadeModel:
        if (command.argument > static_cast<std::uint32_t>(Shade::Smooth)) {
            return Status::InvalidValue;
        }
        state_.raster.shade = static_cast<Shade>(command.argument);
        break;
    case Op::Enable:
        return state_.Enable(static_cast<Capability>(command.argument), v[0] != 0);
    case Op::DepthWrite:
        state_.raster.depthWrite = v[0] != 0;
        break;
    case Op::DepthFunction:
    case Op::AlphaFunction:
        if (command.argument > static_cast<std::uint32_t>(Compare::Always)) {
            return Status::InvalidValue;
        }
        if (command.op == Op::DepthFunction) {
            state_.raster.depthFunction = static_cast<Compare>(command.argument);
        } else {
            state_.raster.alphaFunction = static_cast<Compare>(command.argument);
            state_.raster.alphaReference = std::max(0.0f, std::min(1.0f, f(0)));
        }
        break;
    case Op::BlendFunction:
        if (command.argument > static_cast<std::uint32_t>(BlendFactor::InverseDestinationColor)
            || command.second > static_cast<std::uint32_t>(BlendFactor::InverseDestinationColor)) {
            return Status::InvalidValue;
        }
        state_.raster.blendSource = static_cast<BlendFactor>(command.argument);
        state_.raster.blendDestination = static_cast<BlendFactor>(command.second);
        break;
    case Op::CullFace:
        if (command.argument > static_cast<std::uint32_t>(Cull::FrontAndBack)) {
            return Status::InvalidValue;
        }
        state_.raster.cull = static_cast<Cull>(command.argument);
        break;
    case Op::Viewport:
    case Op::Scissor: {
        std::array<int, 4> rectangle{};
        for (std::size_t index = 0; index < rectangle.size(); ++index) {
            if (!Integral(v[index], rectangle[index])) {
                return Status::InvalidValue;
            }
        }
        if (rectangle[2] < 0 || rectangle[3] < 0) {
            return Status::InvalidValue;
        }
        if (command.op == Op::Viewport) {
            state_.raster.viewport = rectangle;
        } else {
            state_.raster.scissor = rectangle;
        }
        break;
    }
    case Op::DepthRange:
        state_.raster.depthRange = {{std::max(0.0, std::min(1.0, v[0])), std::max(0.0, std::min(1.0, v[1]))}};
        break;
    case Op::BindTexture:
        return textures_.Bind(state_, static_cast<TextureTarget>(command.argument), command.second);
    case Op::PixelStore:
        return textures_.PixelStore(static_cast<PixelStoreParameter>(command.argument), static_cast<int>(v[0]));
    case Op::TextureParameter:
        return textures_.Parameter(state_, static_cast<TextureTarget>(command.argument),
            static_cast<TextureParameter>(command.second), v[0]);
    case Op::TextureEnvironment: {
        auto& texture = state_.textures[state_.ActiveTexture()];
        if (command.argument == static_cast<std::uint32_t>(EnvironmentParameter::Mode)) {
            texture.environment = static_cast<EnvironmentMode>(command.second);
        } else if (command.argument == static_cast<std::uint32_t>(EnvironmentParameter::RGBScale)) {
            texture.rgbScale = command.second;
        }
        break;
    }
    case Op::FogMode:
        state_.raster.fogMode = static_cast<FogMode>(command.argument);
        break;
    case Op::FogHint:
        state_.raster.fogHint = static_cast<HintQuality>(command.argument);
        break;
    case Op::PerspectiveHint:
        state_.raster.perspectiveHint = static_cast<HintQuality>(command.argument);
        break;
    case Op::FogColor:
        for (std::size_t index = 0; index < state_.raster.fogColor.size(); ++index) {
            state_.raster.fogColor[index] = std::max(0.0f, std::min(1.0f, f(index)));
        }
        break;
    case Op::FogDensity:
        if (v[0] < 0) {
            return Status::InvalidValue;
        }
        state_.raster.fogDensity = f(0);
        break;
    case Op::FogStart:
        state_.raster.fogStart = f(0);
        break;
    case Op::FogEnd:
        state_.raster.fogEnd = f(0);
        break;
    case Op::FrontFace:
        state_.raster.frontCounterClockwise = command.argument != 0;
        break;
    case Op::PolygonMode:
        if (command.argument != static_cast<std::uint32_t>(Cull::Back)) {
            state_.raster.polygonMode[0] = static_cast<PolygonMode>(command.second);
        }
        if (command.argument != static_cast<std::uint32_t>(Cull::Front)) {
            state_.raster.polygonMode[1] = static_cast<PolygonMode>(command.second);
        }
        break;
    case Op::PolygonOffset:
        state_.raster.polygonOffset = {{f(0), f(1)}};
        break;
    case Op::Begin:
        if (!ValidPrimitive(command.argument)) {
            return Status::InvalidValue;
        }
        pendingDraw_ = Snapshot(static_cast<Primitive>(command.argument));
        begin_ = true;
        break;
    case Op::Vertex: {
        if (!begin_) {
            return Status::InvalidOperation;
        }
        if (pendingDraw_.vertices.size() >= limits_.verticesPerDraw) {
            return Status::LimitExceeded;
        }
        auto vertex = state_.Current();
        vertex.position = {{f(0), f(1), f(2), 1}};
        pendingDraw_.vertices.push_back(vertex);
        break;
    }
    case Op::End:
        if (!begin_) {
            return Status::InvalidOperation;
        }
        if (!pendingDraw_.vertices.empty()) {
            const auto result = AppendDraw(pendingDraw_);
            if (result != Status::Ok) {
                return result;
            }
        }
        begin_ = false;
        break;
    case Op::CallList: {
        const auto list = lists_.find(command.argument);
        if (list == lists_.end()) {
            break;
        }
        for (const auto& item : list->second) {
            const auto result = Execute(item, depth + 1, budget);
            if (result != Status::Ok) {
                return result;
            }
        }
        break;
    }
    default:
        return Status::InvalidValue;
    }
    return Status::Ok;
}

Status ExpandTopology(const DrawPacket& draw, std::vector<Vertex>& output, Primitive& topology) {
    if (draw.primitive == Primitive::Quads || draw.primitive == Primitive::QuadStrip
        || draw.primitive == Primitive::Polygon) {
        return Status::UnsupportedTopology;
    }
    if (!ValidPrimitive(static_cast<std::uint32_t>(draw.primitive))) {
        return Status::InvalidValue;
    }
    if (draw.vertices.size() > kMaximumVertices) {
        return Status::LimitExceeded;
    }
    if (!draw.indices.empty()) {
        constexpr std::size_t kIndicesPerTriangle = 3;
        if (draw.primitive != Primitive::Triangles || draw.indices.size() % kIndicesPerTriangle != 0) {
            return Status::InvalidValue;
        }
        if (draw.indices.size() > kMaximumVertices) {
            return Status::LimitExceeded;
        }
        for (const auto index : draw.indices) {
            if (index >= draw.vertices.size()) {
                return Status::InvalidValue;
            }
        }
        std::vector<Vertex> expanded;
        expanded.reserve(draw.indices.size());
        for (std::size_t first = 0; first < draw.indices.size(); first += kIndicesPerTriangle) {
            const auto provoking = draw.indices[first + kIndicesPerTriangle - 1];
            for (std::size_t corner = 0; corner < kIndicesPerTriangle; ++corner) {
                auto vertex = draw.vertices[draw.indices[first + corner]];
                if (draw.shade == Shade::Flat) {
                    vertex.color = draw.vertices[provoking].color;
                }
                expanded.push_back(vertex);
            }
        }
        output = std::move(expanded);
        topology = Primitive::Triangles;
        return Status::Ok;
    }
    std::vector<Vertex> result;
    const auto emit = [&draw, &result](std::initializer_list<std::size_t> indices, std::size_t provoking) {
        for (auto index : indices) {
            auto vertex = draw.vertices[index];
            if (draw.shade == Shade::Flat) {
                vertex.color = draw.vertices[provoking].color;
            }
            result.push_back(vertex);
        }
    };
    const auto count = draw.vertices.size();
    switch (draw.primitive) {
    case Primitive::Triangles:
        for (std::size_t index = 0; index + 2 < count; index += 3) {
            emit({index, index + 1, index + 2}, index + 2);
        }
        break;
    case Primitive::TriangleFan:
        for (std::size_t index = 1; index + 1 < count; ++index) {
            emit({0, index, index + 1}, index + 1);
        }
        break;
    case Primitive::TriangleStrip:
        for (std::size_t index = 0; index + 2 < count; ++index) {
            emit({index + (index % 2), index + 1 - (index % 2), index + 2}, index + 2);
        }
        break;
    case Primitive::Lines:
        for (std::size_t index = 0; index + 1 < count; index += 2) {
            emit({index, index + 1}, index + 1);
        }
        break;
    case Primitive::LineLoop:
        if (count > 1) {
            for (std::size_t index = 0; index < count; ++index) {
                emit({index, (index + 1) % count}, (index + 1) % count);
            }
        }
        break;
    default:
        return Status::UnsupportedTopology;
    }
    topology = draw.primitive == Primitive::Lines || draw.primitive == Primitive::LineLoop
        ? Primitive::Lines : Primitive::Triangles;
    output = std::move(result);
    return Status::Ok;
}

}
}
