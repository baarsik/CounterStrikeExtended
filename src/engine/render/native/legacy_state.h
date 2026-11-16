#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace cse {
namespace render {

// Engine diffuse/lightmap/detail units. These are private CPU limits, not GL capability claims.
constexpr std::size_t kTextureUnits = 3;
constexpr std::size_t kModelViewStackDepth = 32;
constexpr std::size_t kProjectionStackDepth = 2;
constexpr std::size_t kTextureStackDepth = 2;

enum class Status { Ok, InvalidValue, InvalidOperation, StackOverflow, StackUnderflow,
    LimitExceeded, UnsupportedTopology };
enum class MatrixMode : std::uint32_t { ModelView, Projection, Texture };
enum class Shade : std::uint32_t { Flat, Smooth };
enum class Capability : std::uint32_t { Blend, DepthTest, AlphaTest, CullFace, ScissorTest, Fog,
    Texture2D, TextureRectangle, PolygonOffsetFill, SharedTexturePalette };
enum class Compare : std::uint32_t { Never, Less, Equal, LessEqual, Greater, NotEqual, GreaterEqual, Always };
enum class BlendFactor : std::uint32_t { Zero, One, SourceColor, InverseSourceColor, SourceAlpha,
    InverseSourceAlpha, DestinationAlpha, InverseDestinationAlpha, DestinationColor, InverseDestinationColor };
enum class Cull : std::uint32_t { Front, Back, FrontAndBack };
enum class EnvironmentMode : std::uint32_t { Modulate, Replace, Combine };
enum class EnvironmentParameter : std::uint32_t { Mode, CombineRGB, RGBScale };
enum class FogMode : std::uint32_t { Exponential, ExponentialSquared, Linear };
enum class PolygonMode : std::uint32_t { Point, Line, Fill };
enum class HintQuality : std::uint32_t { DontCare, Fastest, Nicest };

struct Matrix4 {
    std::array<float, 16> values{};
    static Matrix4 Identity();
};

Matrix4 Multiply(const Matrix4& left, const Matrix4& right);
std::array<float, 4> Transform(const Matrix4& matrix, const std::array<float, 4>& value);

struct Vertex {
    std::array<float, 4> position{{0, 0, 0, 1}};
    std::array<float, 4> color{{1, 1, 1, 1}};
    std::array<std::array<float, 4>, kTextureUnits> texcoords{};
    Vertex();
};

struct RasterState {
    std::array<bool, 7> enabled{};
    bool depthWrite = true;
    std::array<bool, 4> colorWrite{{true, true, true, true}};
    Compare depthFunction = Compare::Less;
    Compare alphaFunction = Compare::Always;
    float alphaReference = 0;
    BlendFactor blendSource = BlendFactor::One;
    BlendFactor blendDestination = BlendFactor::Zero;
    Cull cull = Cull::Back;
    Shade shade = Shade::Smooth;
    std::array<int, 4> viewport{};
    std::array<int, 4> scissor{};
    std::array<double, 2> depthRange{{0, 1}};
    FogMode fogMode = FogMode::Exponential;
    std::array<float, 4> fogColor{};
    float fogDensity = 1;
    float fogStart = 0;
    float fogEnd = 1;
    bool frontCounterClockwise = true;
    std::array<PolygonMode, 2> polygonMode{{PolygonMode::Fill, PolygonMode::Fill}};
    std::array<float, 2> polygonOffset{};
    HintQuality fogHint = HintQuality::DontCare;
    HintQuality perspectiveHint = HintQuality::DontCare;
};

struct TextureUnitState {
    bool enabled2D = false;
    bool enabledRectangle = false;
    std::uint32_t texture2D = 0;
    std::uint32_t textureRectangle = 0;
    EnvironmentMode environment = EnvironmentMode::Modulate;
    // Selected detail path uses the default texture*previous combiner and only changes RGB scale.
    std::uint32_t rgbScale = 1;
};

class LegacyState {
public:
    LegacyState();
    Status SelectTexture(std::uint32_t unit);
    Status SelectMatrix(MatrixMode mode);
    Status Push();
    Status Pop();
    void LoadIdentity();
    void LoadMatrix(const Matrix4& matrix);
    void MultiplyMatrix(const Matrix4& matrix);
    void Translate(float x, float y, float z);
    void Scale(float x, float y, float z);
    void Rotate(float degrees, float x, float y, float z);
    Status Ortho(double left, double right, double bottom, double top, double nearDepth, double farDepth);
    Status Frustum(double left, double right, double bottom, double top, double nearDepth, double farDepth);
    const Matrix4& Matrix(MatrixMode mode, std::size_t unit = 0) const;
    std::size_t Depth(MatrixMode mode, std::size_t unit = 0) const;
    MatrixMode Mode() const {
        return mode_;
    }
    std::size_t ActiveTexture() const {
        return activeTexture_;
    }
    void Color(float red, float green, float blue, float alpha);
    Status TexCoord(std::uint32_t unit, float s, float t);
    const Vertex& Current() const {
        return current_;
    }
    Status Enable(Capability capability, bool enabled);
    bool sharedTexturePalette = false;
    RasterState raster;
    std::array<TextureUnitState, kTextureUnits> textures{};

private:
    struct Stack {
        std::array<Matrix4, kModelViewStackDepth> entries{};
        std::size_t depth = 1;
        std::size_t limit = 1;
    };
    Stack& SelectedStack();
    const Stack& StackFor(MatrixMode mode, std::size_t unit) const;
    void Apply(const Matrix4& matrix);
    Stack modelView_;
    Stack projection_;
    std::array<Stack, kTextureUnits> texture_{};
    MatrixMode mode_ = MatrixMode::ModelView;
    std::size_t activeTexture_ = 0;
    Vertex current_;
};

}
}
