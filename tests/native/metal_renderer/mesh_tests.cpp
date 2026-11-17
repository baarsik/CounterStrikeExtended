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
    Check(buffers.size() <= 6, "Only the three reusable vertex/index streaming pairs remain");
    session.Shutdown();
    Check(buffers.empty(), "Shutdown releases all mesh and streaming buffers");
    SDL_Quit();
    std::cout << "PASS mesh page rollover, budget fallback and queued retirement\n";
}
