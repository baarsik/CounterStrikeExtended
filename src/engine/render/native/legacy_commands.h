#pragma once

#include "legacy_state.h"
#include "legacy_texture.h"
#include <map>
#include <vector>

namespace cse {
namespace render {

class SurfaceMesh;

constexpr std::size_t kMaximumVertices = 1024 * 1024;
constexpr std::size_t kMaximumListCommands = 1024 * 1024;
constexpr std::size_t kMaximumReplayDepth = 64;
constexpr std::size_t kMaximumReplayCommands = 1024 * 1024;
constexpr std::size_t kMaximumLists = 65536;
constexpr std::size_t kMaximumDrawPackets = 65536;

struct CaptureLimits {
    std::size_t verticesPerDraw = kMaximumVertices;
    std::size_t listCommands = kMaximumListCommands;
    std::size_t lists = kMaximumLists;
    std::size_t drawPackets = kMaximumDrawPackets;
    std::size_t storedVertices = kMaximumVertices * 4;
    std::size_t frameVertices = kMaximumVertices * 4;
    std::size_t replayDepth = kMaximumReplayDepth;
    std::size_t replayCommands = kMaximumReplayCommands;
    std::size_t textureStorageBytes = kTextureStorageBytes;
    std::size_t textureNames = kTextureNames;
    std::size_t listUploadBytes = kTextureUploadBytes;
};

enum class Primitive : std::uint32_t { Triangles, TriangleFan, Quads, Polygon, Lines, TriangleStrip, QuadStrip,
    LineLoop };
enum class Op { MatrixMode, ActiveTexture, Identity, Push, Pop, Translate, Scale, Rotate, Ortho, Frustum,
    Color, TexCoord, ShadeModel, Enable, DepthWrite, DepthFunction, AlphaFunction, BlendFunction, CullFace,
    Viewport, Scissor, DepthRange, Begin, Vertex, End, DrawArrays, CallList, BindTexture, DeleteTextures,
    TextureImage, TextureSubImage, TextureParameter, TextureEnvironment, PixelStore, SharedPalette,
    FogMode, FogColor, FogDensity, FogStart, FogEnd, FrontFace, PolygonMode, PolygonOffset, FogHint, PerspectiveHint, LoadMatrix, MultiplyMatrix, ColorWrite, DrawNative };

// Scalar capture does not initialize owning geometry, image or matrix payloads.
struct ScalarCommand {
    Op op = Op::Identity;
    std::uint32_t argument = 0;
    std::uint32_t second = 0;
    std::array<double, 6> values{};
};

struct Command : ScalarCommand {
    Matrix4 matrix;
    // A DrawArrays command owns its resolved position/UV data. Current color is
    // applied at execution (the engine enables only position and UV arrays).
    std::vector<Vertex> vertices;
    std::vector<std::uint32_t> indices;
    std::vector<std::uint32_t> names;
    TextureUpload upload;
    // Recorder overwrites this at submission: lists retain capture-time unpack state.
    PixelStoreState unpack;
};

struct DrawPacket {
    std::shared_ptr<const SurfaceMesh> mesh;
    std::array<float, 4> meshColor{{1, 1, 1, 1}};
    std::array<std::array<float, 4>, kTextureUnits> meshUvOffset{};
    Primitive primitive = Primitive::Triangles;
    Shade shade = Shade::Smooth;
    Matrix4 modelView = Matrix4::Identity();
    Matrix4 projection = Matrix4::Identity();
    std::array<Matrix4, kTextureUnits> textureMatrices{};
    RasterState raster;
    std::array<TextureUnitState, kTextureUnits> textures{};
    std::vector<Vertex> vertices;
    std::vector<std::uint32_t> indices;
    std::array<TextureResources, kTextureUnits> resources;
    std::shared_ptr<const SharedPalette> palette;
    bool sharedTexturePalette = false;
};

// Retains original packets for unqualified quad/polygon lowering. Does not rasterize.
Status ExpandTopology(const DrawPacket& draw, std::vector<Vertex>& output, Primitive& topology);

class Recorder {
public:
    explicit Recorder(CaptureLimits limits = {}, TextureCapture capture = TextureCapture::AllBindings);
    // Value copies own geometry/list bytes; immutable texture versions use copy-on-write and a
    // shared physical-storage charge. No moved-from counters can outlive their containers.
    Recorder(const Recorder&) = default;
    Recorder& operator=(const Recorder&) = default;
    Status Submit(const Command& command);
    Status SubmitScalar(const ScalarCommand& command);
    void EndQuadBatch() {
        quadBatchOpen_ = false;
    }
    Status SubmitQuad(const std::array<Vertex, 4>& vertices);
    Status SubmitIndexed(const std::vector<Vertex>& vertices, const std::vector<std::uint32_t>& indices,
        bool perVertexColor);
    Status SubmitSurface(const std::shared_ptr<const SurfaceMesh>& mesh, float scroll, bool detail);
    Status BeginList(std::uint32_t id, bool execute);
    Status EndList();
    Status DeleteList(std::uint32_t id);
    Status GenerateLists(std::size_t count, std::uint32_t& first);
    Status GenerateTextures(std::size_t count, std::vector<std::uint32_t>& names) {
        return begin_ || compileBegin_ ? Status::InvalidOperation : textures_.Generate(count, names);
    }
    const TextureStore& Textures() const {
        return textures_;
    }
    const LegacyState& State() const {
        return state_;
    }
    const std::vector<DrawPacket>& Draws() const {
        return draws_;
    }
    bool InPrimitive() const {
        return begin_ || compileBegin_;
    }
    bool AtFrameBoundary() const;
    Status TakeFrameDraws(std::vector<DrawPacket>& output);
    void AbortPendingFrame();
    void ClearDraws() {
        EndQuadBatch();
        draws_.clear();
        frameVertices_ = 0;
    }

private:
    Status Execute(const Command& command, std::size_t depth, std::size_t& budget);
    Status ExecuteScalar(const ScalarCommand& command, std::size_t depth, std::size_t& budget);
    bool MatchesQuadState(const DrawPacket& draw) const;
    DrawPacket Snapshot(Primitive primitive) const;
    Status RecordArrayDraw(Primitive primitive,
        const std::vector<Vertex>& vertices, const std::vector<std::uint32_t>& indices, bool perVertexColor);
    Status AppendDraw(DrawPacket& draw);
    bool quadBatchOpen_ = false;
    CaptureLimits limits_;
    TextureCapture textureCapture_;
    std::size_t frameVertices_ = 0;
    std::size_t storedVertices_ = 0;
    std::size_t storedCommands_ = 0;
    std::size_t pendingVertices_ = 0;
    std::size_t storedUploadBytes_ = 0;
    std::size_t pendingUploadBytes_ = 0;
    LegacyState state_;
    TextureStore textures_;
    std::vector<DrawPacket> draws_;
    std::map<std::uint32_t, std::vector<Command>> lists_;
    std::vector<Command> pendingList_;
    std::uint32_t compiling_ = 0;
    bool executeList_ = false;
    bool compileBegin_ = false;
    bool begin_ = false;
    DrawPacket pendingDraw_;
};

}
}
