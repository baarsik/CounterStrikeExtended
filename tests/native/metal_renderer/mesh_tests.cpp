#include "metal_session.h"
#include "surface_mesh.h"
#include <SDL3/SDL.h>
#include <cmath>
#include <cstdlib>
#include <dlfcn.h>
#include <iostream>
#include <map>

using namespace cse::render;

namespace {
std::string error;
std::map<SDL_GPUBuffer*, std::size_t> buffers;
std::map<SDL_GPUTransferBuffer*, std::size_t> transfers;
unsigned uploads = 0;
constexpr unsigned kTargetSize = 16;
constexpr std::size_t kRgbaComponents = 4;

void Check(bool condition, const char* message) {
    if (!condition) {
        std::cerr << message << ": " << error << '\n';
        std::exit(1);
    }
}

template<typename Function> Function Original(const char* name) {
    auto function = reinterpret_cast<Function>(dlsym(RTLD_NEXT, name));
    Check(function != nullptr, "Resolve real SDL function");
    return function;
}

FrameSnapshot Frame(const std::shared_ptr<const SurfaceMesh>& mesh) {
    FrameSnapshot frame;
    frame.info.width = kTargetSize;
    frame.info.height = kTargetSize;
    frame.draws.resize(1);
    auto& draw = frame.draws[0];
    draw.mesh = mesh;
    draw.meshColor = {{1, 0, 0, 1}};
    draw.raster.viewport = {{0, 0, kTargetSize, kTargetSize}};
    for (auto& matrix : draw.textureMatrices) {
        matrix = Matrix4::Identity();
    }
    frame.events.resize(1);
    return frame;
}

std::vector<Vertex> Polygon(std::size_t count) {
    constexpr double kFullCircle = 6.28318530717958647692;
    constexpr double kRadius = 0.9;
    std::vector<Vertex> vertices(count);
    for (std::size_t index = 0; index < count; ++index) {
        const double angle = index * kFullCircle / count;
        vertices[index].position = {{static_cast<float>(kRadius * std::cos(angle)),
            static_cast<float>(kRadius * std::sin(angle)), 0, 1}};
    }
    return vertices;
}

void Pixel(MetalSession& session) {
    std::array<unsigned char, kRgbaComponents> pixel{};
    Check(session.ReadPixels(kTargetSize / 2, kTargetSize / 2, 1, 1, kRgbaComponents, 1, pixel.data(), error),
        "Read queued mesh result");
    Check(pixel[0] == 255 && pixel[1] == 0 && pixel[2] == 0, "Retired or streamed mesh retains exact color");
}
}

extern "C" SDL_GPUBuffer* SDL_CreateGPUBuffer(SDL_GPUDevice* device, const SDL_GPUBufferCreateInfo* info) {
    auto* buffer = Original<decltype(&SDL_CreateGPUBuffer)>("SDL_CreateGPUBuffer")(device, info);
    if (buffer) {
        buffers.emplace(buffer, info->size);
    }
    return buffer;
}

extern "C" void SDL_ReleaseGPUBuffer(SDL_GPUDevice* device, SDL_GPUBuffer* buffer) {
    Check(buffers.erase(buffer) == 1, "Release an owned GPU buffer exactly once");
    Original<decltype(&SDL_ReleaseGPUBuffer)>("SDL_ReleaseGPUBuffer")(device, buffer);
}

extern "C" SDL_GPUTransferBuffer* SDL_CreateGPUTransferBuffer(SDL_GPUDevice* device,
    const SDL_GPUTransferBufferCreateInfo* info) {
    auto* buffer = Original<decltype(&SDL_CreateGPUTransferBuffer)>("SDL_CreateGPUTransferBuffer")(device, info);
    if (buffer) {
        transfers.emplace(buffer, info->size);
    }
    return buffer;
}

extern "C" void SDL_ReleaseGPUTransferBuffer(SDL_GPUDevice* device, SDL_GPUTransferBuffer* buffer) {
    Check(transfers.erase(buffer) == 1, "Release an owned transfer exactly once");
    Original<decltype(&SDL_ReleaseGPUTransferBuffer)>("SDL_ReleaseGPUTransferBuffer")(device, buffer);
}

extern "C" void SDL_UploadToGPUBuffer(SDL_GPUCopyPass* pass, const SDL_GPUTransferBufferLocation* source,
    const SDL_GPUBufferRegion* destination, bool cycle) {
    ++uploads;
    Original<decltype(&SDL_UploadToGPUBuffer)>("SDL_UploadToGPUBuffer")(pass, source, destination, cycle);
}

int main() {
    Check(SDL_Init(SDL_INIT_VIDEO), "SDL video");
    MetalSession session;
    Check(session.Initialize(nullptr, true, error), "Metal initialization");
    Check(session.Resize(kTargetSize, kTargetSize, 1, error), "Metal target");
    FrameSnapshot empty;
    empty.info.width = kTargetSize;
    empty.info.height = kTargetSize;
    // Retire the sole owner before reading back: SDL must keep submitted GPU references alive.
    constexpr unsigned kRetirementCycles = 6;
    for (unsigned cycle = 0; cycle < kRetirementCycles; ++cycle) {
        auto frame = Frame(SurfaceMesh::Create(Polygon(4), error));
        Check(session.Render(frame, error), "Submit owned resident mesh");
        Check(buffers.size() == 2, "One shared vertex/index page pair");
        frame.draws.clear();
        Check(session.Render(empty, error), "Retire page during pending GPU work");
        Check(buffers.empty(), "Map retirement releases the entire arena page");
        Pixel(session);
    }
    // Each 65,536-vertex polygon exceeds a normal vertex page and forces rollover. Twenty-four
    // owners exceed the 128 MiB resident GPU budget, exercising the feature-preserving stream path.
    constexpr std::size_t kRolloverVertices = 64 * 1024;
    constexpr unsigned kPressureMeshes = 24;
    constexpr std::size_t kMaximumResidentAndStreams = 160 * 1024 * 1024;
    const auto vertices = Polygon(kRolloverVertices);
    std::vector<std::shared_ptr<const SurfaceMesh>> owners;
    FrameSnapshot frame;
    for (unsigned index = 0; index < kPressureMeshes; ++index) {
        owners.push_back(SurfaceMesh::Create(vertices, error));
        Check(owners.back() != nullptr, "Create rollover polygon");
        frame = Frame(owners.back());
        Check(session.Render(frame, error), "Render across mesh page rollover/cache pressure");
    }
    std::size_t bytes = 0;
    for (const auto& buffer : buffers) {
        bytes += buffer.second;
    }
    Check(bytes <= kMaximumResidentAndStreams, "Resident arena and three streams remain bounded");
    uploads = 0;
    Check(session.Render(frame, error), "Render cache-pressure fallback again");
    Check(uploads == 2, "Uncached mesh streams both vertices and indices without losing geometry");
    Pixel(session);
    owners.clear();
    frame.draws.clear();
    Check(session.Render(empty, error), "Retire every pressure page while retaining stream fences");
    bytes = 0;
    for (const auto& buffer : buffers) {
        bytes += buffer.second;
    }
    for (const auto& transfer : transfers) {
        bytes += transfer.second;
    }
    constexpr std::size_t kStreamingBudget = 128 * 1024 * 1024;
    Check(bytes <= kStreamingBudget, "Only bounded reusable frame streaming storage remains");
    // This recording's ordinary scene/HUD segments need more than 32 MiB across three
    // in-flight slots. Once warm, stable geometry must reuse its buffers without allocation churn.
    frame = Frame(nullptr);
    constexpr std::size_t kReuseVertices = 48 * 1024;
    frame.draws[0].vertices = Polygon(kReuseVertices);
    session.EnableDiagnostics(true);
    std::uint64_t warmedAllocations = 0;
    for (unsigned iteration = 0; iteration < 8; ++iteration) {
        Check(session.Render(frame, error) && session.Render(frame, error), "Encode stable scene and HUD segments");
        Check(session.Finish(error), "Retire one complete frame");
        if (iteration == 3) {
            warmedAllocations = session.Diagnostics().streamAllocations;
        }
    }
    Check(session.Diagnostics().streamAllocations == warmedAllocations,
        "Steady three-frame scene/HUD storage must not allocate each frame");
    // Large changing images have tiny geometry but large transfer capacity. Their retained
    // transfers must count toward the same cache bound, including upload-pressure submissions.
    frame = Frame(nullptr);
    frame.draws[0].vertices = Polygon(3);
    for (auto& vertex : frame.draws[0].vertices) {
        vertex.color = {{1, 0, 0, 1}};
    }
    frame.draws[0].textures[0].enabled2D = true;
    constexpr int kPressureTextureSide = 2048;
    constexpr unsigned kPressureTextureVersions = 13;
    for (unsigned version = 0; version < kPressureTextureVersions; ++version) {
        auto image = std::make_shared<TextureImage>();
        image->width = kPressureTextureSide;
        image->height = kPressureTextureSide;
        image->defined = true;
        image->pixels.resize(kPressureTextureSide * kPressureTextureSide * 4, 255);
        auto texture = std::make_shared<TextureObject>();
        texture->levels[0] = image;
        texture->sampler.minification = Filter::Linear;
        frame.draws[0].resources[0].texture2D = texture;
        Check(session.Render(frame, error), "Queue changing image with small geometry");
    }
    Check(session.Finish(error), "Submit all changing images");
    frame.draws.clear();
    Check(session.Render(empty, error) && session.Finish(error), "Retire pressure texture owners");
    Pixel(session);
    bytes = 0;
    for (const auto& buffer : buffers) {
        bytes += buffer.second;
    }
    for (const auto& transfer : transfers) {
        bytes += transfer.second;
    }
    Check(bytes <= kStreamingBudget, "Texture-heavy segments cannot evade the total streaming cache bound");
    // These polygons occupy one vertex page with a third index range between them.
    // Replay only the outside polygons, in reverse order, without uploading their vertices.
    {
        const auto positioned = [](float center) {
            auto polygon = Polygon(4);
            for (auto& vertex : polygon) {
                vertex.position[0] = center + vertex.position[0] * 0.3f;
                vertex.position[1] *= 0.7f;
            }
            return SurfaceMesh::Create(std::move(polygon), error);
        };
        auto batch = Frame(positioned(-0.67f));
        batch.draws.push_back(Frame(positioned(0.0f)).draws[0]);
        batch.draws.push_back(Frame(positioned(0.67f)).draws[0]);
        batch.events.resize(batch.draws.size());
        for (std::size_t index = 0; index < batch.events.size(); ++index) {
            batch.events[index].drawIndex = index;
        }
        Check(session.Render(batch, error), "Upload three polygons to the same page");
        auto clearFrame = empty;
        FrameEvent clear;
        clear.kind = FrameEventKind::Clear;
        clear.clear.mask = kClearColor;
        clearFrame.events.push_back(clear);
        Check(session.Render(clearFrame, error), "Clear the full scene before replay");
        batch.events.resize(2);
        batch.events[0].drawIndex = 2;
        batch.events[1].drawIndex = 0;
        session.EnableDiagnostics(true);
        uploads = 0;
        Check(session.Render(batch, error), "Draw noncontiguous resident ranges");
        std::array<unsigned char, kTargetSize * kRgbaComponents> pixels{};
        Check(session.ReadPixels(0, kTargetSize / 2, kTargetSize, 1, kRgbaComponents, 1, pixels.data(), error),
            "Read replayed ranges");
        Check(pixels[2 * kRgbaComponents] == 255 && pixels[(kTargetSize - 3) * kRgbaComponents] == 255 &&
            pixels[(kTargetSize / 2) * kRgbaComponents] == 0, "Keep both selected polygons and omit their neighbor");
        Check(session.Diagnostics().gpuDraws == 1, "Draw both same-state ranges in one indexed call");
        Check(uploads == 1 && session.Diagnostics().uploadBytes == 12 * sizeof(std::uint32_t),
            "Upload only selected indices");
    }
    // The engine keeps map-owned meshes alive across scene/HUD segments.
    // Advancing the map epoch releases GPU page ownership even with pending work.
    {
        auto map = Frame(SurfaceMesh::Create(Polygon(4), error));
        map.info.surfaceMeshEpoch = 1;
        Check(session.Render(map, error), "Upload a map-scoped mesh");
        session.EnableDiagnostics(true);
        Check(session.Render(map, error), "Draw another segment of the same map");
        Check(session.Diagnostics().meshOwnerChecks == 0, "Stable map needs no mesh owner sweep");
        const auto bufferCount = buffers.size();
        map.draws.clear();
        map.events.clear();
        map.info.surfaceMeshEpoch = 2;
        Check(session.Render(map, error), "Change map before the previous scene is submitted");
        Check(buffers.size() + 2 == bufferCount, "Map epoch change retires the cached vertex/index page");
        Pixel(session);
    }
    session.Shutdown();
    Check(buffers.empty() && transfers.empty(), "Shutdown releases all mesh and streaming buffers");
    SDL_Quit();
    std::cout << "PASS mesh page rollover, budget fallback and queued retirement\n";
}
