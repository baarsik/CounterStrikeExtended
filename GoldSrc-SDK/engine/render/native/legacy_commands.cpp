#include "legacy_commands.h"
#include <algorithm>
#include <cmath>
#include <limits>

namespace cse {
namespace render {
namespace {

bool ValidPrimitive(std::uint32_t value) {
    return value <= static_cast<std::uint32_t>(Primitive::LineLoop);
}

bool Integral(double value, int& result) {
    if (!std::isfinite(value) || value < std::numeric_limits<int>::min()
        || value > std::numeric_limits<int>::max() || value != std::floor(value)) {
        return false;
    }
    result = static_cast<int>(value);
    return true;
}

Status ValidateArguments(const Command& command) {
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
    case Op::DrawArrays:
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
        return command.argument <= static_cast<std::uint32_t>(Capability::PolygonOffsetFill)
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
    case Op::TextureImage:
    case Op::TextureSubImage:
        if (command.argument > static_cast<std::uint32_t>(TextureTarget::Rectangle)) {
            return Status::InvalidValue;
        }
        return TextureStore::ValidateUpload(command.upload, command.unpack, command.op == Op::TextureSubImage);
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
    case Op::SharedPalette:
        return command.upload.bytes.size() == kPaletteBytes ? Status::Ok : Status::InvalidValue;
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
    case Op::DeleteTextures:
        if (command.names.size() > kTextureNames) {
            return Status::LimitExceeded;
        }
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
        if (command.values[0] == command.values[1] || command.values[2] == command.values[3]
            || command.values[4] == command.values[5]) {
            return Status::InvalidValue;
        }
        if (command.op == Op::Frustum && (command.values[4] <= 0 || command.values[5] <= 0)) {
            return Status::InvalidValue;
        }
        break;
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

bool AttributeOperation(Op operation) {
    return operation == Op::Color || operation == Op::TexCoord || operation == Op::Vertex
        || operation == Op::End || operation == Op::CallList;
}

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
    return draw;
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
            oldVertices += command.vertices.size();
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
            storedVertices_ -= command.vertices.size();
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
    Command captured;
    const bool upload = input.op == Op::TextureImage || input.op == Op::TextureSubImage;
    if (upload) {
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
        const auto addition = command.vertices.size() + (command.op == Op::Vertex ? 1 : 0);
        if (command.vertices.size() > limits_.verticesPerDraw
            || addition > limits_.storedVertices - pendingVertices_) {
            return Status::LimitExceeded;
        }
        if (!executeList_) {
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
        pendingList_.push_back(command);
        pendingVertices_ += command.vertices.size() + (command.op == Op::Vertex ? 1 : 0);
        pendingUploadBytes_ += command.upload.bytes.size();
        if (command.op == Op::Begin || command.op == Op::End) {
            compileBegin_ = command.op == Op::Begin;
        }
    }
    return result;
}

Status Recorder::AppendDraw(DrawPacket& draw) {
    if (draws_.size() >= limits_.drawPackets || draw.vertices.size() > limits_.frameVertices - frameVertices_) {
        return Status::LimitExceeded;
    }
    const auto vertices = draw.vertices.size();
    draws_.push_back(std::move(draw));
    frameVertices_ += vertices;
    return Status::Ok;
}

Status Recorder::Execute(const Command& command, std::size_t depth, std::size_t& budget) {
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
        return state_.Ortho(v[0], v[1], v[2], v[3], v[4], v[5]);
    case Op::Frustum:
        return state_.Frustum(v[0], v[1], v[2], v[3], v[4], v[5]);
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
    case Op::DeleteTextures:
        textures_.Delete(state_, command.names);
        break;
    case Op::PixelStore:
        return textures_.PixelStore(static_cast<PixelStoreParameter>(command.argument), static_cast<int>(v[0]));
    case Op::TextureParameter:
        return textures_.Parameter(state_, static_cast<TextureTarget>(command.argument),
            static_cast<TextureParameter>(command.second), v[0]);
    case Op::TextureImage:
    case Op::TextureSubImage:
        return textures_.Upload(state_, static_cast<TextureTarget>(command.argument), command.upload,
            command.unpack, command.op == Op::TextureSubImage);
    case Op::SharedPalette:
        return textures_.Palette(command.upload.bytes);
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
    case Op::DrawArrays: {
        if (!ValidPrimitive(command.argument)) {
            return Status::InvalidValue;
        }
        if (command.vertices.size() > limits_.verticesPerDraw) {
            return Status::LimitExceeded;
        }
        auto draw = Snapshot(static_cast<Primitive>(command.argument));
        draw.vertices = command.vertices;
        for (auto& vertex : draw.vertices) {
            vertex.color = state_.Current().color;
            for (std::size_t unit = 1; unit < kTextureUnits; ++unit) {
                vertex.texcoords[unit] = state_.Current().texcoords[unit];
            }
        }
        if (!draw.vertices.empty()) {
            return AppendDraw(draw);
        }
        break;
    }
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
