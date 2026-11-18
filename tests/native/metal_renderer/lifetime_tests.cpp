#include "metal_session.h"
#include "radar_renderer_gpu.h"
#include <SDL3/SDL.h>
#include <algorithm>
#include <cstdlib>
#include <dispatch/dispatch.h>
#include <dlfcn.h>
#include <iostream>
#include <string>

using namespace cse::render;

namespace {
std::string error;
std::size_t liveGpuTextures = 0;
std::size_t createdGpuTextures = 0;
int installedRamMiB = -1;
dispatch_queue_t pressureTestQueue = nullptr;
dispatch_source_t pressureTestSource = nullptr;
constexpr unsigned kTargetSize = 16;
constexpr std::uint32_t kMaximumEngineName = 65535;
constexpr std::size_t kRgbaComponents = 4;

void Check(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL lifetime: " << message << " " << error << '\n';
        std::exit(1);
    }
}

template<typename Function> Function Original(const char* name) {
    auto function = reinterpret_cast<Function>(dlsym(RTLD_NEXT, name));
    Check(function != nullptr, "resolve actual SDL resource owner");
    return function;
}

TextureUpload Image(int width, int height, std::uint8_t red, std::uint8_t green) {
    TextureUpload image;
    image.width = width;
    image.height = height;
    image.bytes.resize(static_cast<std::size_t>(width) * height * kRgbaComponents);
    for (std::size_t offset = 0; offset < image.bytes.size(); offset += kRgbaComponents) {
        image.bytes[offset] = red;
        image.bytes[offset + 1] = green;
        image.bytes[offset + 3] = 255;
    }
    return image;
}

FrameSnapshot Frame(std::shared_ptr<const TextureObject> object) {
    FrameSnapshot frame;
    frame.info.width = kTargetSize;
    frame.info.height = kTargetSize;
    DrawPacket draw;
    draw.primitive = Primitive::Quads;
    draw.raster.viewport = {{0, 0, kTargetSize, kTargetSize}};
    draw.vertices.resize(4);
    draw.vertices[0].position = {{-1, -1, 0, 1}};
    draw.vertices[1].position = {{1, -1, 0, 1}};
    draw.vertices[2].position = {{1, 1, 0, 1}};
    draw.vertices[3].position = {{-1, 1, 0, 1}};
    for (auto& matrix : draw.textureMatrices) {
        matrix = Matrix4::Identity();
    }
    for (auto& vertex : draw.vertices) {
        vertex.color = {{1, 1, 1, 1}};
    }
    draw.textures[0].environment = EnvironmentMode::Replace;
    if (object->target == TextureTarget::Rectangle) {
        draw.textures[0].enabledRectangle = true;
        draw.resources[0].rectangle = std::move(object);
    } else {
        draw.textures[0].enabled2D = true;
        draw.resources[0].texture2D = std::move(object);
    }
    frame.draws.push_back(std::move(draw));
    FrameEvent event;
    event.kind = FrameEventKind::Draw;
    event.drawIndex = 0;
    frame.events.push_back(event);
    return frame;
}

void Pixel(MetalSession& session, std::uint8_t red, std::uint8_t green) {
    std::array<std::uint8_t, kRgbaComponents> pixel{};
    Check(session.ReadPixels(8, 8, 1, 1, kRgbaComponents, 1, pixel.data(), error), "read queued content");
    Check(pixel[0] == red && pixel[1] == green, "queued version has expected independent color");
}

CseRadarMapTextureDraw Tile(CseRadarTexture texture) {
    CseRadarMapTextureDraw tile{};
    tile.size = sizeof(tile);
    tile.rendererEpoch = 1;
    tile.hud = {kTargetSize, kTargetSize};
    tile.texture = texture;
    tile.mode = CSE_RADAR_MAP_PLAIN_OVERVIEW;
    std::fill(std::begin(tile.modulation), std::end(tile.modulation), 1);
    tile.vertexCount = 4;
    tile.vertices[0] = {{0, 0}, {0, 0}};
    tile.vertices[1] = {{kTargetSize, 0}, {0, 1}};
    tile.vertices[2] = {{kTargetSize, kTargetSize}, {1, 1}};
    tile.vertices[3] = {{0, kTargetSize}, {1, 0}};
    return tile;
}

void Clear(MetalSession& session) {
    FrameSnapshot frame;
    frame.info.width = kTargetSize;
    frame.info.height = kTargetSize;
    FrameEvent clear;
    clear.kind = FrameEventKind::Clear;
    clear.clear.mask = kClearColor;
    frame.events.push_back(clear);
    Check(session.Render(frame, error), "initialize effect target");
}

template<typename T> struct CountingAllocator {
    using value_type = T;
    std::size_t* liveBlocks;

    explicit CountingAllocator(std::size_t& count) : liveBlocks(&count) {}

    template<typename U> CountingAllocator(const CountingAllocator<U>& other) : liveBlocks(other.liveBlocks) {}

    T* allocate(std::size_t count) {
        auto* block = std::allocator<T>{}.allocate(count);
        ++*liveBlocks;
        return block;
    }

    void deallocate(T* block, std::size_t count) {
        --*liveBlocks;
        std::allocator<T>{}.deallocate(block, count);
    }
};

void RadarRecords() {
    // allocate_shared's counted block survives object destruction while any registry weak reference remains.
    // This measures stale registry metadata without adding a test-only production accessor.
    std::size_t liveBlocks = 0;
    MetalSession session;
    RadarRendererGpu effects(session, 1);
    for (unsigned cycle = 0; cycle < 96; ++cycle) {
        auto object = std::allocate_shared<TextureObject>(CountingAllocator<TextureObject>(liveBlocks));
        object->generation = cycle + 1;
        const auto handle = effects.RegisterTexture(cycle + 1, RadarRendererGpu::TextureRole::World, object, nullptr);
        Check(handle.serial != 0 && liveBlocks == 1, "one live borrowed registry control block");
        object.reset();
        effects.BeginSmokedFrame();
        Check(liveBlocks == 0, "frame sweep erases stale registry metadata without querying abandoned handles");
    }
    std::cout << "radar records: 96 abandoned borrowed identities; weak control blocks=0 after every sweep\n";
}

void Names() {
    TextureStore store;
    LegacyState state;
    std::vector<std::uint32_t> names;
    constexpr unsigned kTurnovers = 2 * (kMaximumEngineName + 1);
    for (unsigned cycle = 0; cycle < kTurnovers; ++cycle) {
        Check(store.Generate(1, names) == Status::Ok, "bounded turnover generates a name");
        Check(names[0] > 0 && names[0] <= kMaximumEngineName, "generated name fits engine low16 domain");
        store.Delete(state, names);
    }
    Check(store.Generate(kMaximumEngineName, names) == Status::Ok, "all legal simultaneous names available");
    auto sorted = names;
    std::sort(sorted.begin(), sorted.end());
    Check(sorted.front() == 1 && sorted.back() == kMaximumEngineName &&
        std::adjacent_find(sorted.begin(), sorted.end()) == sorted.end(), "full domain unique and nonzero");
    std::vector<std::uint32_t> sentinel{123};
    Check(store.Generate(1, sentinel) == Status::LimitExceeded && sentinel == std::vector<std::uint32_t>{123},
        "simultaneous exhaustion explicit and output preserved");
    store.Delete(state, {names[100]});
    Check(store.Generate(1, sentinel) == Status::Ok && sentinel[0] == names[100], "single released name reusable");
    std::cout << "names: " << kTurnovers << " turnovers; " << names.size()
        << " simultaneous names; explicit exhaustion\n";
}

void SmallResidentImages() {
    Check(SDL_Init(SDL_INIT_VIDEO), "initialize SDL for resident image budget");
    MetalSession session;
    Check(session.Initialize(nullptr, true, error) && session.Resize(kTargetSize, kTargetSize, 1, error),
        "Metal session for resident images");
    constexpr unsigned kImagesPerGroup = 300;
    const auto group = [](std::uint8_t red, std::uint8_t green) {
        FrameSnapshot frame;
        frame.info.width = kTargetSize;
        frame.info.height = kTargetSize;
        for (unsigned index = 0; index < kImagesPerGroup; ++index) {
            auto image = std::make_shared<TextureImage>();
            image->width = 1;
            image->height = 1;
            image->defined = true;
            image->pixels = {red, green, 0, 255};
            auto texture = std::make_shared<TextureObject>();
            texture->levels[0] = std::move(image);
            auto single = Frame(std::move(texture));
            auto event = single.events.front();
            event.drawIndex = frame.draws.size();
            frame.events.push_back(event);
            frame.draws.push_back(std::move(single.draws.front()));
        }
        return frame;
    };
    const auto first = group(255, 0);
    const auto second = group(0, 255);
    session.EnableDiagnostics(true);
    Check(session.Render(first, error), "first sampled working set");
    Pixel(session, 255, 0);
    Check(session.Render(second, error), "second sampled working set");
    Pixel(session, 0, 255);
    const auto allocations = createdGpuTextures;
    Check(session.Render(first, error), "reuse earlier sampled working set");
    Pixel(session, 255, 0);
    Check(session.Diagnostics().sampledTextureCapacityEvictions == 0 && createdGpuTextures == allocations,
        "small sampled working sets above 512 handles stay resident");
    session.Shutdown();
    Check(liveGpuTextures == 0, "releases all small sampled images");
    SDL_Quit();
}

void LargeSampledScene(int ramMiB, std::uint64_t expectedPasses) {
    installedRamMiB = ramMiB;
    Check(SDL_Init(SDL_INIT_VIDEO), "initialize SDL for sampled scene");
    MetalSession session;
    Check(session.Initialize(nullptr, false, error) && session.Resize(kTargetSize, kTargetSize, 4, error),
        "Metal multisample scene");
    session.EnableDiagnostics(true);
    FrameSnapshot frame;
    frame.info.width = kTargetSize;
    frame.info.height = kTargetSize;
    FrameEvent clear;
    clear.kind = FrameEventKind::Clear;
    clear.clear.mask = kClearColor | kClearDepth;
    clear.clear.color = {{0, 0, 1, 1}};
    frame.events.push_back(clear);
    constexpr unsigned kDraws = 1024; // A fallback texture occupies one more entry.
    for (unsigned index = 0; index < kDraws; ++index) {
        auto image = std::make_shared<TextureImage>();
        image->width = 1;
        image->height = 1;
        image->defined = true;
        const bool last = index == kDraws - 1;
        image->pixels = {static_cast<std::uint8_t>(last ? 0 : 255),
            static_cast<std::uint8_t>(last ? 255 : 0), 0, static_cast<std::uint8_t>(last ? 128 : 255)};
        auto texture = std::make_shared<TextureObject>();
        texture->levels[0] = std::move(image);
        auto single = Frame(std::move(texture));
        auto& draw = single.draws.front();
        if (index == 0) {
            draw.raster.enabled[static_cast<std::size_t>(Capability::DepthTest)] = true;
            draw.raster.enabled[static_cast<std::size_t>(Capability::ScissorTest)] = true;
            draw.raster.scissor = {{0, 0, kTargetSize / 2, kTargetSize}};
            for (auto& vertex : draw.vertices) {
                vertex.position[2] = -0.5f;
            }
        } else if (last) {
            draw.raster.enabled[static_cast<std::size_t>(Capability::DepthTest)] = true;
            draw.raster.enabled[static_cast<std::size_t>(Capability::Blend)] = true;
            draw.raster.blendSource = BlendFactor::SourceAlpha;
            draw.raster.blendDestination = BlendFactor::InverseSourceAlpha;
        } else {
            draw.raster.colorWrite = {{false, false, false, false}};
        }
        auto event = single.events.front();
        event.drawIndex = frame.draws.size();
        frame.events.push_back(event);
        frame.draws.push_back(std::move(draw));
    }
    Check(session.Render(frame, error, SceneOutput::Resolved), "render the entire sampled scene");
    Check(session.Diagnostics().renderPasses == expectedPasses, "use the configured sampled-texture tier");
    Check(session.Diagnostics().sceneResolves == 1, "resolve the combined multisample scene once");
    std::array<std::uint8_t, kRgbaComponents> left{};
    Check(session.ReadPixels(4, 8, 1, 1, kRgbaComponents, 1, left.data(), error), "read the occluded pixel");
    Check(left[0] == 255 && left[1] == 0, "preserve earlier color and depth across batches");
    std::array<std::uint8_t, kRgbaComponents> right{};
    Check(session.ReadPixels(12, 8, 1, 1, kRgbaComponents, 1, right.data(), error), "read the blended pixel");
    Check(right[0] == 0 && right[1] == 128 && right[2] == 127, "blend the later draw over the earlier color");
    session.Shutdown();
    Check(liveGpuTextures == 0, "retire all sampled scene allocations");
    SDL_Quit();
    installedRamMiB = -1;
}

void SampledTextureMemoryPressure() {
    Check(SDL_Init(SDL_INIT_VIDEO), "initialize SDL for memory pressure");
    pressureTestQueue = dispatch_queue_create("cse.test-memory-pressure", DISPATCH_QUEUE_SERIAL);
    MetalSession session;
    Check(session.Initialize(nullptr, false, error) && session.Resize(kTargetSize, kTargetSize, 1, error),
        "Metal session for memory pressure");
    Check(pressureTestSource != nullptr, "subscribe to macOS memory pressure");

    const auto picture = [](std::uint8_t red, std::uint8_t green) {
        auto image = std::make_shared<TextureImage>();
        image->width = 1;
        image->height = 1;
        image->defined = true;
        image->pixels = {red, green, 0, 255};
        auto object = std::make_shared<TextureObject>();
        object->levels[0] = std::move(image);
        return Frame(std::move(object));
    };
    const auto first = picture(255, 0);
    const auto second = picture(0, 255);
    Check(session.Render(first, error) && session.Render(second, error), "populate the reusable texture cache");
    Pixel(session, 0, 255);
    const auto before = liveGpuTextures;
    const auto allocations = createdGpuTextures;
    dispatch_source_merge_data(pressureTestSource, 1);
    const auto deadline = SDL_GetTicks() + 1000;
    while (liveGpuTextures == before && SDL_GetTicks() < deadline) {
        // Continue drawing the green image as if it were still on screen.
        Check(session.Render(second, error) && session.Finish(error), "render while processing memory pressure");
        SDL_Delay(1);
    }
    Check(liveGpuTextures + 1 == before, "release the idle image and retain the active image");
    Check(createdGpuTextures == allocations, "reuse the active image across pressure");
    Check(session.Render(first, error), "reupload the evicted image from its captured source");
    Pixel(session, 255, 0);
    session.Shutdown();
    Check(liveGpuTextures == 0, "release the texture cache");
    pressureTestSource = nullptr;
    dispatch_release(pressureTestQueue);
    pressureTestQueue = nullptr;
    SDL_Quit();
}

void Session() {
    Check(SDL_Init(SDL_INIT_VIDEO), "initialize SDL");
    MetalSession session;
    Check(session.Initialize(nullptr, true, error) && session.Resize(kTargetSize, kTargetSize, 1, error),
        "Metal session");
    // Three NPOT source versions fit; an accumulating cache fails before its unrelated GPU byte limit.
    constexpr int kWidth = 13;
    constexpr int kHeight = 7;
    constexpr std::size_t kImageBytes = kWidth * kHeight * kRgbaComponents;
    TextureStore store(3 * kImageBytes);
    LegacyState state;
    Check(store.Bind(state, TextureTarget::Rectangle, 1) == Status::Ok, "bind NPOT video image");
    std::size_t peakBytes = 0;
    for (unsigned cycle = 0; cycle < 96; ++cycle) {
        auto upload = Image(kWidth, kHeight, static_cast<std::uint8_t>(cycle), 0);
        Check(store.Upload(state, TextureTarget::Rectangle, upload, {}, cycle != 0) == Status::Ok,
            "completed NPOT update fits controlled source budget");
        {
            auto frame = Frame(store.Bound(state, TextureTarget::Rectangle));
            Check(session.Render(frame, error) && session.Finish(error), "complete NPOT submission");
            Pixel(session, static_cast<std::uint8_t>(cycle), 0);
            const auto allocations = createdGpuTextures;
            if (cycle == 0) {
                const auto sampler = store.Bound(state, TextureTarget::Rectangle)->sampler;
                const std::array<std::pair<TextureParameter, double>, 6> repeated{{
                    {TextureParameter::MinFilter, static_cast<int>(sampler.minification)},
                    {TextureParameter::MagFilter, static_cast<int>(sampler.magnification)},
                    {TextureParameter::WrapS, static_cast<int>(sampler.wrapS)},
                    {TextureParameter::WrapT, static_cast<int>(sampler.wrapT)},
                    {TextureParameter::MaximumLevel, sampler.maximumLevel},
                    {TextureParameter::Anisotropy, sampler.requestedAnisotropy}
                }};
                for (const auto& parameter : repeated) {
                    Check(store.Parameter(state, TextureTarget::Rectangle, parameter.first, parameter.second) ==
                        Status::Ok, "repeat unchanged texture sampling state");
                }
                auto repeatedFrame = Frame(store.Bound(state, TextureTarget::Rectangle));
                Check(session.Render(repeatedFrame, error), "draw after redundant texture parameters");
                Pixel(session, static_cast<std::uint8_t>(cycle), 0);
                Check(createdGpuTextures == allocations, "redundant parameters preserve the uploaded GPU image");
            }
            Check(session.Render(frame, error), "repeat unchanged live texture");
            Check(createdGpuTextures == allocations, "persistent session cache reuses live immutable identity");
        }
        peakBytes = std::max(peakBytes, store.StorageBytes());
        Check(store.StorageBytes() == kImageBytes, "completed cache retains only current NPOT source image");
    }
    store.Delete(state, {1});
    Check(store.StorageBytes() == 0, "completed session cache does not own deleted sources");
    std::cout << "session: 96 completed NPOT updates; peak retained source=" << peakBytes << " bytes; deleted=0\n";
    session.Shutdown();
    Check(liveGpuTextures == 0, "session releases all GPU texture owners");
    SDL_Quit();
}

void QueuedReuse() {
    Check(SDL_Init(SDL_INIT_VIDEO), "initialize SDL");
    MetalSession session;
    Check(session.Initialize(nullptr, true, error) && session.Resize(kTargetSize, kTargetSize, 1, error),
        "Metal session");
    {
        TextureStore store(64);
        LegacyState state;
        RadarRendererGpu effects(session, 1);
        effects.SetTextureSource(&store, &state, nullptr);
        std::vector<std::uint32_t> names;
        Check(store.Generate(kMaximumEngineName, names) == Status::Ok, "reserve domain to force exact name reuse");
        const auto name = names.front();
        Check(store.Bind(state, TextureTarget::Texture2D, name) == Status::Ok, "bind old name");
        Check(store.Upload(state, TextureTarget::Texture2D, Image(2, 2, 255, 0), {}, false) == Status::Ok,
            "upload queued old red image");
        Check(store.Parameter(state, TextureTarget::Texture2D, TextureParameter::MinFilter,
            static_cast<int>(Filter::Nearest)) == Status::Ok, "single-level old image");
        auto oldFrame = Frame(store.Bound(state, TextureTarget::Texture2D));
        const auto oldHandle = effects.RegisterTexture(name, RadarRendererGpu::TextureRole::Sprite,
            store.Bound(state, TextureTarget::Texture2D), nullptr);
        store.Delete(state, {name});
        Check(store.StorageBytes() == 16, "queued old packet independently retains source storage");
        std::vector<std::uint32_t> reused;
        Check(store.Generate(1, reused) == Status::Ok && reused.front() == name, "reuse exact deleted name");
        Check(store.Bind(state, TextureTarget::Texture2D, name) == Status::Ok, "bind replacement name");
        Check(store.Upload(state, TextureTarget::Texture2D, Image(2, 2, 0, 255), {}, false) == Status::Ok,
            "upload independent replacement green image");
        Check(store.Parameter(state, TextureTarget::Texture2D, TextureParameter::MinFilter,
            static_cast<int>(Filter::Nearest)) == Status::Ok, "single-level new image");
        auto newFrame = Frame(store.Bound(state, TextureTarget::Texture2D));
        const auto newHandle = effects.RegisterTexture(name, RadarRendererGpu::TextureRole::Sprite,
            store.Bound(state, TextureTarget::Texture2D), nullptr);
        Check(oldHandle.serial != newHandle.serial, "reused engine name receives new service serial");
        Check(oldFrame.draws[0].resources[0].texture2D->generation !=
            newFrame.draws[0].resources[0].texture2D->generation, "queued and replacement generations differ");
        RasterState raster;
        raster.viewport = {{0, 0, kTargetSize, kTargetSize}};
        Check(effects.DrawMapTexture(Tile(oldHandle), raster) == CSE_RADAR_INVALID_TEXTURE,
            "old borrowed handle cannot resolve to replacement even while old packet is alive");
        Check(session.Render(oldFrame, error), "render queued old version after name reuse");
        Pixel(session, 255, 0);
        Check(session.Render(newFrame, error), "render replacement version");
        Pixel(session, 0, 255);
        Check(effects.DrawMapTexture(Tile(newHandle), raster) == CSE_RADAR_OK, "replacement borrowed handle live");
        Pixel(session, 0, 255);
        store.Delete(state, {name});
        oldFrame = {};
        newFrame = {};
        Check(store.StorageBytes() == 0, "both caches release sources once queued packets retire");
        effects.BeginSmokedFrame();
        Clear(session);
        std::cout << "queued reuse: same name, distinct generations/handles, red old pixels and green new pixels; "
            << "source=0\n";
    }
    session.Shutdown();
    Check(liveGpuTextures == 0, "queued reuse releases all GPU owners");
    SDL_Quit();
}

void Radar() {
    Check(SDL_Init(SDL_INIT_VIDEO), "initialize SDL");
    MetalSession session;
    Check(session.Initialize(nullptr, true, error) && session.Resize(kTargetSize, kTargetSize, 1, error),
        "Metal session");
    {
        TextureStore store(64);
        LegacyState state;
        RadarRendererGpu effects(session, 1);
        effects.SetTextureSource(&store, &state, nullptr);
        Clear(session);
        const auto baselineGpuTextures = liveGpuTextures;
        RasterState raster;
        raster.viewport = {{0, 0, kTargetSize, kTargetSize}};
        CseRadarTexture oldHandle{};
        for (unsigned cycle = 0; cycle < 96; ++cycle) {
            std::vector<std::uint32_t> names;
            Check(store.Generate(1, names) == Status::Ok, "generate map material");
            Check(store.Bind(state, TextureTarget::Texture2D, names[0]) == Status::Ok, "bind map material");
            Check(store.Upload(state, TextureTarget::Texture2D, Image(2, 2, 255, 0), {}, false) == Status::Ok,
                "map-like upload fits before next service registration");
            Check(store.Parameter(state, TextureTarget::Texture2D, TextureParameter::MinFilter,
                static_cast<int>(Filter::Nearest)) == Status::Ok, "complete single-level map texture");
            const auto handle = effects.RegisterTexture(names[0], RadarRendererGpu::TextureRole::Sprite,
                store.Bound(state, TextureTarget::Texture2D), nullptr);
            Check(handle.serial != 0 && handle.serial != oldHandle.serial, "map material gets fresh service identity");
            oldHandle = handle;
            const auto tile = Tile(handle);
            Check(effects.DrawMapTexture(tile, raster) == CSE_RADAR_OK, "upload borrowed radar GPU image");
            Check(liveGpuTextures == baselineGpuTextures + 1, "exactly one current borrowed GPU owner");
            const auto allocations = createdGpuTextures;
            effects.BeginSmokedFrame();
            Check(effects.DrawMapTexture(tile, raster) == CSE_RADAR_OK, "live borrowed handle survives sweep");
            Check(createdGpuTextures == allocations, "persistent radar cache reuses live immutable identity");
            store.Delete(state, names);
            Check(store.StorageBytes() == 0, "borrowed registry releases deleted source without stale-handle lookup");
            effects.BeginSmokedFrame();
            Check(liveGpuTextures == baselineGpuTextures, "map retirement bounds GPU owners");
            Pixel(session, 255, 0); // Submitted GPU work survives release of the registry's owning reference.
        }
        std::cout << "radar: 96 map-like register/delete cycles; retained source=0 after each deletion\n";
    }
    session.Shutdown();
    SDL_Quit();
}
}

extern "C" SDL_GPUTexture* SDL_CreateGPUTexture(SDL_GPUDevice* device, const SDL_GPUTextureCreateInfo* info) {
    auto* texture = Original<decltype(&SDL_CreateGPUTexture)>("SDL_CreateGPUTexture")(device, info);
    if (texture) {
        ++liveGpuTextures;
        ++createdGpuTextures;
    }
    return texture;
}

extern "C" void SDL_ReleaseGPUTexture(SDL_GPUDevice* device, SDL_GPUTexture* texture) {
    Check(liveGpuTextures > 0, "balanced actual SDL texture ownership");
    --liveGpuTextures;
    Original<decltype(&SDL_ReleaseGPUTexture)>("SDL_ReleaseGPUTexture")(device, texture);
}

extern "C" int SDLCALL SDL_GetSystemRAM() {
    return installedRamMiB >= 0 ? installedRamMiB : Original<decltype(&SDL_GetSystemRAM)>("SDL_GetSystemRAM")();
}

extern "C" dispatch_source_t dispatch_source_create(dispatch_source_type_t type, uintptr_t handle,
    unsigned long mask, dispatch_queue_t queue) {
    const auto create = Original<decltype(&dispatch_source_create)>("dispatch_source_create");
    if (pressureTestQueue && type == DISPATCH_SOURCE_TYPE_MEMORYPRESSURE) {
        // A data source lets the fixture deliver a real dispatch event without stressing the host.
        pressureTestSource = create(DISPATCH_SOURCE_TYPE_DATA_ADD, 0, 0, pressureTestQueue);
        return pressureTestSource;
    }
    return create(type, handle, mask, queue);
}

int main(int argc, char** argv) {
    Check(argc == 2, "select lifetime fixture");
    const std::string mode = argv[1];
    if (mode == "names") {
        Names();
    } else if (mode == "session") {
        Session();
        SmallResidentImages();
        LargeSampledScene(8192, 2);
        LargeSampledScene(8193, 1);
    } else if (mode == "memory-pressure") {
        SampledTextureMemoryPressure();
    } else if (mode == "radar") {
        Radar();
    } else if (mode == "queued-reuse") {
        QueuedReuse();
    } else if (mode == "radar-records") {
        RadarRecords();
    } else {
        Check(false, "known lifetime fixture");
    }
}
