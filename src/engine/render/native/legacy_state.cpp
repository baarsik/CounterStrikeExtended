#include "legacy_state.h"

#include <cmath>
#include <stdexcept>
#include <limits>

namespace cse {
namespace render {

namespace {

bool ProjectionMatrix(const std::array<double, 16>& values, Matrix4& matrix) {
    for (std::size_t index = 0; index < values.size(); ++index) {
        if (!std::isfinite(values[index]) || std::fabs(values[index]) > std::numeric_limits<float>::max()) {
            return false;
        }
        matrix.values[index] = static_cast<float>(values[index]);
    }
    return true;
}

}

Matrix4 Matrix4::Identity() {
    Matrix4 result;
    constexpr std::size_t dimension = 4;
    for (std::size_t index = 0; index < dimension; ++index) {
        result.values[index * dimension + index] = 1;
    }
    return result;
}

Matrix4 Multiply(const Matrix4& left, const Matrix4& right) {
    Matrix4 result;
    constexpr std::size_t dimension = 4;
    for (std::size_t column = 0; column < dimension; ++column) {
        for (std::size_t row = 0; row < dimension; ++row) {
            float value = 0;
            for (std::size_t index = 0; index < dimension; ++index) {
                value += left.values[index * dimension + row] * right.values[column * dimension + index];
            }
            result.values[column * dimension + row] = value;
        }
    }
    return result;
}

std::array<float, 4> Transform(const Matrix4& matrix, const std::array<float, 4>& value) {
    std::array<float, 4> result{};
    for (std::size_t row = 0; row < result.size(); ++row) {
        for (std::size_t column = 0; column < value.size(); ++column) {
            result[row] += matrix.values[column * result.size() + row] * value[column];
        }
    }
    return result;
}

Vertex::Vertex() {
    for (auto& coordinate : texcoords) {
        coordinate = {{0, 0, 0, 1}};
    }
}

LegacyState::LegacyState() {
    modelView_.limit = kModelViewStackDepth;
    projection_.limit = kProjectionStackDepth;
    modelView_.entries[0] = Matrix4::Identity();
    projection_.entries[0] = Matrix4::Identity();
    for (auto& stack : texture_) {
        stack.limit = kTextureStackDepth;
        stack.entries[0] = Matrix4::Identity();
    }
}

Status LegacyState::SelectTexture(std::uint32_t unit) {
    if (unit >= kTextureUnits) {
        return Status::InvalidValue;
    }
    activeTexture_ = unit;
    return Status::Ok;
}

Status LegacyState::SelectMatrix(MatrixMode mode) {
    if (mode != MatrixMode::ModelView && mode != MatrixMode::Projection && mode != MatrixMode::Texture) {
        return Status::InvalidValue;
    }
    mode_ = mode;
    return Status::Ok;
}

const LegacyState::Stack& LegacyState::StackFor(MatrixMode mode, std::size_t unit) const {
    if (mode == MatrixMode::ModelView) {
        return modelView_;
    }
    if (mode == MatrixMode::Projection) {
        return projection_;
    }
    if (mode != MatrixMode::Texture || unit >= kTextureUnits) {
        throw std::out_of_range("private legacy matrix query");
    }
    return texture_[unit];
}

LegacyState::Stack& LegacyState::SelectedStack() {
    if (mode_ == MatrixMode::ModelView) {
        return modelView_;
    }
    if (mode_ == MatrixMode::Projection) {
        return projection_;
    }
    return texture_[activeTexture_];
}

const Matrix4& LegacyState::Matrix(MatrixMode mode, std::size_t unit) const {
    const auto& stack = StackFor(mode, unit);
    return stack.entries[stack.depth - 1];
}

std::size_t LegacyState::Depth(MatrixMode mode, std::size_t unit) const {
    return StackFor(mode, unit).depth;
}

Status LegacyState::Push() {
    auto& stack = SelectedStack();
    if (stack.depth == stack.limit) {
        return Status::StackOverflow;
    }
    stack.entries[stack.depth] = stack.entries[stack.depth - 1];
    ++stack.depth;
    return Status::Ok;
}

Status LegacyState::Pop() {
    auto& stack = SelectedStack();
    if (stack.depth == 1) {
        return Status::StackUnderflow;
    }
    --stack.depth;
    return Status::Ok;
}

void LegacyState::LoadIdentity() {
    auto& stack = SelectedStack();
    stack.entries[stack.depth - 1] = Matrix4::Identity();
}

void LegacyState::LoadMatrix(const Matrix4& matrix) {
    auto& stack = SelectedStack();
    stack.entries[stack.depth - 1] = matrix;
}

void LegacyState::MultiplyMatrix(const Matrix4& matrix) {
    Apply(matrix);
}

void LegacyState::Apply(const Matrix4& matrix) {
    auto& stack = SelectedStack();
    stack.entries[stack.depth - 1] = Multiply(stack.entries[stack.depth - 1], matrix);
}

void LegacyState::Translate(float x, float y, float z) {
    auto matrix = Matrix4::Identity();
    matrix.values[12] = x;
    matrix.values[13] = y;
    matrix.values[14] = z;
    Apply(matrix);
}

void LegacyState::Scale(float x, float y, float z) {
    auto matrix = Matrix4::Identity();
    matrix.values[0] = x;
    matrix.values[5] = y;
    matrix.values[10] = z;
    Apply(matrix);
}

void LegacyState::Rotate(float degrees, float x, float y, float z) {
    const double length = std::sqrt(static_cast<double>(x) * x + static_cast<double>(y) * y
        + static_cast<double>(z) * z);
    if (length == 0) {
        return;
    }
    constexpr double radiansPerDegree = 3.14159265358979323846 / 180.0;
    const double cosine = std::cos(degrees * radiansPerDegree);
    const double sine = std::sin(degrees * radiansPerDegree);
    const double remainder = 1 - cosine;
    const double nx = x / length;
    const double ny = y / length;
    const double nz = z / length;
    auto matrix = Matrix4::Identity();
    matrix.values[0] = static_cast<float>(nx * nx * remainder + cosine);
    matrix.values[1] = static_cast<float>(ny * nx * remainder + nz * sine);
    matrix.values[2] = static_cast<float>(nz * nx * remainder - ny * sine);
    matrix.values[4] = static_cast<float>(nx * ny * remainder - nz * sine);
    matrix.values[5] = static_cast<float>(ny * ny * remainder + cosine);
    matrix.values[6] = static_cast<float>(nz * ny * remainder + nx * sine);
    matrix.values[8] = static_cast<float>(nx * nz * remainder + ny * sine);
    matrix.values[9] = static_cast<float>(ny * nz * remainder - nx * sine);
    matrix.values[10] = static_cast<float>(nz * nz * remainder + cosine);
    Apply(matrix);
}

Status LegacyState::Ortho(double left, double right, double bottom, double top, double nearDepth, double farDepth) {
    if (left == right || bottom == top || nearDepth == farDepth) {
        return Status::InvalidValue;
    }
    std::array<double, 16> values{};
    values[15] = 1;
    values[0] = 2 / (right - left);
    values[5] = 2 / (top - bottom);
    values[10] = -2 / (farDepth - nearDepth);
    values[12] = -(right + left) / (right - left);
    values[13] = -(top + bottom) / (top - bottom);
    values[14] = -(farDepth + nearDepth) / (farDepth - nearDepth);
    Matrix4 matrix;
    if (!ProjectionMatrix(values, matrix)) {
        return Status::InvalidValue;
    }
    Apply(matrix);
    return Status::Ok;
}

Status LegacyState::Frustum(double left, double right, double bottom, double top, double nearDepth, double farDepth) {
    if (left == right || bottom == top || nearDepth <= 0 || farDepth <= 0 || nearDepth == farDepth) {
        return Status::InvalidValue;
    }
    std::array<double, 16> values{};
    values[0] = 2 * nearDepth / (right - left);
    values[5] = 2 * nearDepth / (top - bottom);
    values[8] = (right + left) / (right - left);
    values[9] = (top + bottom) / (top - bottom);
    values[10] = -(farDepth + nearDepth) / (farDepth - nearDepth);
    values[11] = -1;
    values[14] = -2 * farDepth * nearDepth / (farDepth - nearDepth);
    Matrix4 matrix;
    if (!ProjectionMatrix(values, matrix)) {
        return Status::InvalidValue;
    }
    Apply(matrix);
    return Status::Ok;
}

void LegacyState::Color(float red, float green, float blue, float alpha) {
    current_.color = {{red, green, blue, alpha}};
}

Status LegacyState::TexCoord(std::uint32_t unit, float s, float t) {
    if (unit >= kTextureUnits) {
        return Status::InvalidValue;
    }
    current_.texcoords[unit] = {{s, t, 0, 1}};
    return Status::Ok;
}

Status LegacyState::Enable(Capability capability, bool enabled) {
    if (capability == Capability::SharedTexturePalette) {
        sharedTexturePalette = enabled;
    } else if (capability == Capability::Texture2D) {
        textures[activeTexture_].enabled2D = enabled;
    } else if (capability == Capability::TextureRectangle) {
        textures[activeTexture_].enabledRectangle = enabled;
    } else {
        const auto index = capability == Capability::PolygonOffsetFill ? raster.enabled.size() - 1 :
            static_cast<std::size_t>(capability);
        if (index >= raster.enabled.size()) {
            return Status::InvalidValue;
        }
        raster.enabled[index] = enabled;
    }
    return Status::Ok;
}

}
}
