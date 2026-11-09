#include <SDL3/SDL.h>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <cstddef>
#include <vector>
#include <cstdio>
#include <cstring>
#include <fstream>
#if defined(_WIN32)
#include <windows.h>
#endif

namespace {

constexpr Uint32 kSide = 64;
constexpr Uint32 kChannels = 4;
constexpr Uint32 kReadbackBytes = kSide * kSide * kChannels;
constexpr int kUnsupported = 2;
constexpr int kMissingShaders = 3;
constexpr Uint32 kTriangleOffset = kSide / 4;
constexpr Uint32 kTriangleSize = kSide / 2;
constexpr std::size_t kShaderMagicBytes = 4;
constexpr unsigned char kOpaqueByte = 255;
constexpr std::size_t kMaximumShaderBytes = 16 * 1024 * 1024;

struct alignas(16) SourceRegion {
    std::array<float, 2> leftTop{};
    std::array<float, 2> dimensions{{1, 1}};
    std::uint32_t mip = 0;
    float layer = 0;
    std::array<float, 2> padding{};
};

static_assert(sizeof(SourceRegion) == 32, "SDL stock shader uniform with std140 block padding");
static_assert(offsetof(SourceRegion, mip) == 16, "pinned shader SourceRegion contract");

void Log(void*, int category, SDL_LogPriority priority, const char* message) {
    std::printf("SDL_log category=%d priority=%d %s\n", category, static_cast<int>(priority), message);
}

struct Resources {
    SDL_GPUDevice* device = nullptr;
    SDL_GPUTexture* color = nullptr;
    SDL_GPUTexture* depth = nullptr;
    SDL_GPUTransferBuffer* transfer = nullptr;
    SDL_GPUFence* fence = nullptr;
    SDL_GPUShader* vertexShader = nullptr;
    SDL_GPUShader* fragmentShader = nullptr;
    SDL_GPUGraphicsPipeline* pipeline = nullptr;
    SDL_GPUTexture* source = nullptr;
    SDL_GPUTransferBuffer* upload = nullptr;
    SDL_GPUSampler* sampler = nullptr;

    ~Resources() {
        if (device) {
            SDL_WaitForGPUIdle(device);
            if (pipeline) {
                SDL_ReleaseGPUGraphicsPipeline(device, pipeline);
            }
            if (vertexShader) {
                SDL_ReleaseGPUShader(device, vertexShader);
            }
            if (fragmentShader) {
                SDL_ReleaseGPUShader(device, fragmentShader);
            }
            if (source) {
                SDL_ReleaseGPUTexture(device, source);
            }
            if (upload) {
                SDL_ReleaseGPUTransferBuffer(device, upload);
            }
            if (sampler) {
                SDL_ReleaseGPUSampler(device, sampler);
            }
            if (fence) {
                SDL_ReleaseGPUFence(device, fence);
            }
            if (transfer) {
                SDL_ReleaseGPUTransferBuffer(device, transfer);
            }
            if (depth) {
                SDL_ReleaseGPUTexture(device, depth);
            }
            if (color) {
                SDL_ReleaseGPUTexture(device, color);
            }
            SDL_DestroyGPUDevice(device);
        }
    }
};

SDL_GPUShader* LoadShader(SDL_GPUDevice* device, const char* path, SDL_GPUShaderFormat format,
    SDL_GPUShaderStage stage) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    const auto length = file.tellg();
    if (length < static_cast<std::streamoff>(kShaderMagicBytes)
        || static_cast<std::uint64_t>(length) > kMaximumShaderBytes) {
        std::printf("invalid_shader_file=%s\n", path);
        return nullptr;
    }
    std::vector<unsigned char> bytes(static_cast<std::size_t>(length));
    file.seekg(0);
    file.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    const char* magic = format == SDL_GPU_SHADERFORMAT_METALLIB ? "MTLB" : "DXBC";
    if (!file.good() || std::memcmp(bytes.data(), magic, kShaderMagicBytes) != 0) {
        std::printf("invalid_offline_shader_container=%s\n", path);
        return nullptr;
    }
    SDL_GPUShaderCreateInfo shader{};
    shader.code_size = bytes.size();
    shader.code = bytes.data();
    shader.format = format;
    shader.stage = stage;
    shader.entrypoint = stage == SDL_GPU_SHADERSTAGE_VERTEX ? "FullscreenVert" : "BlitFrom2D";
    if (stage == SDL_GPU_SHADERSTAGE_FRAGMENT) {
        shader.num_samplers = 1;
        shader.num_uniform_buffers = 1;
    }
    std::printf("offline_shader=%s bytes=%zu entrypoint=%s format=%u samplers=%u uniforms=%u storage=0\n",
        path, bytes.size(), shader.entrypoint, format, shader.num_samplers, shader.num_uniform_buffers);
    return SDL_CreateGPUShader(device, &shader);
}

bool PrepareTriangle(Resources& resources, SDL_GPUShaderFormat format, const char* vertex, const char* fragment) {
    resources.vertexShader = LoadShader(resources.device, vertex, format, SDL_GPU_SHADERSTAGE_VERTEX);
    resources.fragmentShader = LoadShader(resources.device, fragment, format, SDL_GPU_SHADERSTAGE_FRAGMENT);
    if (!resources.vertexShader || !resources.fragmentShader) {
        return false;
    }
    SDL_GPUColorTargetDescription target{};
    target.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
    SDL_GPUGraphicsPipelineCreateInfo pipeline{};
    pipeline.vertex_shader = resources.vertexShader;
    pipeline.fragment_shader = resources.fragmentShader;
    pipeline.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
    pipeline.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
    pipeline.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_NONE;
    pipeline.rasterizer_state.enable_depth_clip = true;
    pipeline.multisample_state.sample_count = SDL_GPU_SAMPLECOUNT_1;
    pipeline.depth_stencil_state.enable_depth_test = true;
    pipeline.depth_stencil_state.enable_depth_write = true;
    pipeline.depth_stencil_state.compare_op = SDL_GPU_COMPAREOP_LESS;
    pipeline.target_info.num_color_targets = 1;
    pipeline.target_info.color_target_descriptions = &target;
    pipeline.target_info.has_depth_stencil_target = true;
    pipeline.target_info.depth_stencil_format = SDL_GPU_TEXTUREFORMAT_D32_FLOAT;
    resources.pipeline = SDL_CreateGPUGraphicsPipeline(resources.device, &pipeline);
    if (!resources.pipeline) {
        return false;
    }
    SDL_GPUTextureCreateInfo texture{};
    texture.type = SDL_GPU_TEXTURETYPE_2D;
    texture.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
    texture.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER;
    texture.width = kSide;
    texture.height = kSide;
    texture.layer_count_or_depth = 1;
    texture.num_levels = 1;
    texture.sample_count = SDL_GPU_SAMPLECOUNT_1;
    resources.source = SDL_CreateGPUTexture(resources.device, &texture);
    SDL_GPUTransferBufferCreateInfo transfer{};
    transfer.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    transfer.size = kReadbackBytes;
    resources.upload = SDL_CreateGPUTransferBuffer(resources.device, &transfer);
    SDL_GPUSamplerCreateInfo sampler{};
    sampler.min_filter = SDL_GPU_FILTER_NEAREST;
    sampler.mag_filter = SDL_GPU_FILTER_NEAREST;
    sampler.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST;
    sampler.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    sampler.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    sampler.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    resources.sampler = SDL_CreateGPUSampler(resources.device, &sampler);
    if (!resources.source || !resources.upload || !resources.sampler) {
        return false;
    }
    auto* mapped = static_cast<unsigned char*>(SDL_MapGPUTransferBuffer(resources.device, resources.upload, false));
    if (!mapped) {
        return false;
    }
    for (Uint32 index = 0; index < kReadbackBytes; ++index) {
        const auto component = index % kChannels;
        mapped[index] = component == 0 || component == kChannels - 1 ? kOpaqueByte : 0;
    }
    SDL_UnmapGPUTransferBuffer(resources.device, resources.upload);
    return true;
}

bool ClearAndReadback(Resources& resources, bool triangle = false) {
    if (resources.fence) {
        SDL_ReleaseGPUFence(resources.device, resources.fence);
        resources.fence = nullptr;
    }
    auto* command = SDL_AcquireGPUCommandBuffer(resources.device);
    if (!command) {
        return false;
    }
    if (triangle) {
        auto* upload = SDL_BeginGPUCopyPass(command);
        if (!upload) {
            SDL_CancelGPUCommandBuffer(command);
            return false;
        }
        SDL_GPUTextureTransferInfo source{};
        source.transfer_buffer = resources.upload;
        source.pixels_per_row = kSide;
        source.rows_per_layer = kSide;
        SDL_GPUTextureRegion destination{};
        destination.texture = resources.source;
        destination.w = kSide;
        destination.h = kSide;
        destination.d = 1;
        SDL_UploadToGPUTexture(upload, &source, &destination, false);
        SDL_EndGPUCopyPass(upload);
    }
    SDL_GPUColorTargetInfo color{};
    color.texture = resources.color;
    color.clear_color = {0, 0, 0, 1};
    color.load_op = SDL_GPU_LOADOP_CLEAR;
    color.store_op = SDL_GPU_STOREOP_STORE;
    SDL_GPUDepthStencilTargetInfo depth{};
    depth.texture = resources.depth;
    depth.clear_depth = 1;
    depth.load_op = SDL_GPU_LOADOP_CLEAR;
    depth.store_op = SDL_GPU_STOREOP_DONT_CARE;
    depth.stencil_load_op = SDL_GPU_LOADOP_DONT_CARE;
    depth.stencil_store_op = SDL_GPU_STOREOP_DONT_CARE;
    auto* pass = SDL_BeginGPURenderPass(command, &color, 1, &depth);
    if (!pass) {
        SDL_CancelGPUCommandBuffer(command);
        return false;
    }
    if (triangle) {
        SDL_BindGPUGraphicsPipeline(pass, resources.pipeline);
        const SDL_GPUViewport viewport{static_cast<float>(kTriangleOffset), static_cast<float>(kTriangleOffset),
            static_cast<float>(kTriangleSize), static_cast<float>(kTriangleSize), 0, 1};
        SDL_SetGPUViewport(pass, &viewport);
        const SDL_Rect scissor{0, 0, static_cast<int>(kSide), static_cast<int>(kSide)};
        SDL_SetGPUScissor(pass, &scissor);
        const SDL_GPUTextureSamplerBinding binding{resources.source, resources.sampler};
        SDL_BindGPUFragmentSamplers(pass, 0, &binding, 1);
        const SourceRegion region{};
        SDL_PushGPUFragmentUniformData(command, 0, &region, static_cast<Uint32>(sizeof(region)));
        constexpr Uint32 triangleVertices = 3;
        SDL_DrawGPUPrimitives(pass, triangleVertices, 1, 0, 0);
    }
    SDL_EndGPURenderPass(pass);
    auto* copy = SDL_BeginGPUCopyPass(command);
    if (!copy) {
        SDL_CancelGPUCommandBuffer(command);
        return false;
    }
    SDL_GPUTextureRegion source{};
    source.texture = resources.color;
    source.w = kSide;
    source.h = kSide;
    source.d = 1;
    SDL_GPUTextureTransferInfo destination{};
    destination.transfer_buffer = resources.transfer;
    destination.pixels_per_row = kSide;
    destination.rows_per_layer = kSide;
    SDL_DownloadFromGPUTexture(copy, &source, &destination);
    SDL_EndGPUCopyPass(copy);
    resources.fence = SDL_SubmitGPUCommandBufferAndAcquireFence(command);
    if (!resources.fence || !SDL_WaitForGPUFences(resources.device, true, &resources.fence, 1)) {
        return false;
    }
    const auto* mapped = static_cast<const unsigned char*>(SDL_MapGPUTransferBuffer(resources.device,
        resources.transfer, false));
    if (!mapped) {
        return false;
    }
    std::array<unsigned char, kReadbackBytes> bytes{};
    std::memcpy(bytes.data(), mapped, bytes.size());
    SDL_UnmapGPUTransferBuffer(resources.device, resources.transfer);
    std::size_t differences = 0;
    std::uint64_t hash = UINT64_C(14695981039346656037);
    constexpr std::uint64_t prime = UINT64_C(1099511628211);
    for (std::size_t index = 0; index < bytes.size(); ++index) {
        const auto x = (index / kChannels) % kSide;
        const auto y = index / (kChannels * kSide);
        const bool covered = triangle && x >= kTriangleOffset && x < kTriangleOffset + kTriangleSize
            && y >= kTriangleOffset && y < kTriangleOffset + kTriangleSize;
        const auto component = index % kChannels;
        const unsigned char expected = component == kChannels - 1 || (covered && component == 0)
            ? kOpaqueByte : 0;
        differences += bytes[index] != expected ? 1 : 0;
        hash = (hash ^ bytes[index]) * prime;
    }
    std::ofstream file(triangle ? "gpu-triangle-readback.rgba" : "gpu-clear-readback.rgba", std::ios::binary);
    file.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    std::printf("readback triangle=%d bytes=%zu differing_bytes=%zu FNV1a=%016llx\n",
        triangle ? 1 : 0, bytes.size(), differences,
        static_cast<unsigned long long>(hash));
    return differences == 0 && file.good();
}

int Probe(const char* driver, bool fewerResources, bool dxbc, bool triangle,
    const char* vertexPath, const char* fragmentPath) {
    const SDL_GPUShaderFormat requested = std::strcmp(driver, "metal") == 0
        ? SDL_GPU_SHADERFORMAT_METALLIB : (dxbc ? SDL_GPU_SHADERFORMAT_DXBC : SDL_GPU_SHADERFORMAT_DXIL);
    for (int index = 0; index < SDL_GetNumGPUDrivers(); ++index) {
        std::printf("compiled_GPU_driver[%d]=%s\n", index, SDL_GetGPUDriver(index));
    }
    const bool supported = SDL_GPUSupportsShaderFormats(requested, driver);
    std::printf("requested_driver=%s requested_offline_shader_format=%u supported=%d GPU_validation=0\n",
        driver, requested, supported ? 1 : 0);
    const auto properties = SDL_CreateProperties();
    SDL_SetStringProperty(properties, SDL_PROP_GPU_DEVICE_CREATE_NAME_STRING, driver);
    SDL_SetBooleanProperty(properties, SDL_PROP_GPU_DEVICE_CREATE_DEBUGMODE_BOOLEAN, false);
    SDL_SetBooleanProperty(properties, SDL_PROP_GPU_DEVICE_CREATE_VERBOSE_BOOLEAN, true);
    SDL_SetBooleanProperty(properties, SDL_PROP_GPU_DEVICE_CREATE_SHADERS_METALLIB_BOOLEAN,
        requested == SDL_GPU_SHADERFORMAT_METALLIB);
    SDL_SetBooleanProperty(properties, SDL_PROP_GPU_DEVICE_CREATE_SHADERS_DXIL_BOOLEAN,
        requested == SDL_GPU_SHADERFORMAT_DXIL);
    SDL_SetBooleanProperty(properties, SDL_PROP_GPU_DEVICE_CREATE_SHADERS_DXBC_BOOLEAN,
        requested == SDL_GPU_SHADERFORMAT_DXBC);
    SDL_SetBooleanProperty(properties, SDL_PROP_GPU_DEVICE_CREATE_D3D12_ALLOW_FEWER_RESOURCE_SLOTS_BOOLEAN,
        fewerResources);
    const bool propertySupport = SDL_GPUSupportsProperties(properties);
    std::printf("fewer_resource_slots=%d property_support=%d clear_storage_resources=0 "
        "documented_D3D12_compatibility_storage_limit_across_stages=8\n",
        fewerResources ? 1 : 0, propertySupport ? 1 : 0);
    Resources resources;
    resources.device = SDL_CreateGPUDeviceWithProperties(properties);
    if (!resources.device) {
        std::printf("device_creation=unsupported SDL_error=%s\n", SDL_GetError());
        SDL_DestroyProperties(properties);
        return kUnsupported;
    }
    SDL_DestroyProperties(properties);
    const char* actualDriver = SDL_GetGPUDeviceDriver(resources.device);
    std::printf("device_creation=success actual_driver=%s shader_formats=%u\n",
        actualDriver ? actualDriver : "[missing]",
        SDL_GetGPUShaderFormats(resources.device));
    if (!actualDriver || std::strcmp(driver, actualDriver) != 0) {
        std::puts("requested backend was not selected");
        return kUnsupported;
    }
    const auto deviceProperties = SDL_GetGPUDeviceProperties(resources.device);
    for (const char* key : {SDL_PROP_GPU_DEVICE_NAME_STRING, SDL_PROP_GPU_DEVICE_DRIVER_NAME_STRING,
        SDL_PROP_GPU_DEVICE_DRIVER_VERSION_STRING, SDL_PROP_GPU_DEVICE_DRIVER_INFO_STRING}) {
        std::printf("property %s=%s\n", key, SDL_GetStringProperty(deviceProperties, key, "[not reported]"));
    }
    const auto colorUsage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER;
    const bool colorSupported = SDL_GPUTextureSupportsFormat(resources.device, SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,
        SDL_GPU_TEXTURETYPE_2D, colorUsage);
    for (auto format : {SDL_GPU_TEXTUREFORMAT_D24_UNORM, SDL_GPU_TEXTUREFORMAT_D32_FLOAT}) {
        std::printf("depth_format=%d supported=%d\n", static_cast<int>(format),
            SDL_GPUTextureSupportsFormat(resources.device, format, SDL_GPU_TEXTURETYPE_2D,
                SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET) ? 1 : 0);
    }
    std::printf("RGBA8_color_sampler_supported=%d\n", colorSupported ? 1 : 0);
    SDL_GPUTextureCreateInfo texture{};
    texture.type = SDL_GPU_TEXTURETYPE_2D;
    texture.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
    texture.usage = colorUsage;
    texture.width = kSide;
    texture.height = kSide;
    texture.layer_count_or_depth = 1;
    texture.num_levels = 1;
    texture.sample_count = SDL_GPU_SAMPLECOUNT_1;
    resources.color = SDL_CreateGPUTexture(resources.device, &texture);
    texture.format = SDL_GPU_TEXTUREFORMAT_D32_FLOAT;
    texture.usage = SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET;
    resources.depth = SDL_CreateGPUTexture(resources.device, &texture);
    SDL_GPUTransferBufferCreateInfo transfer{};
    transfer.usage = SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD;
    transfer.size = kReadbackBytes;
    resources.transfer = SDL_CreateGPUTransferBuffer(resources.device, &transfer);
    std::printf("offscreen_color=%d offscreen_depth_D32=%d download_buffer=%d\n", resources.color ? 1 : 0,
        resources.depth ? 1 : 0, resources.transfer ? 1 : 0);
    if (!resources.color || !resources.depth || !resources.transfer || !ClearAndReadback(resources)) {
        std::printf("offscreen_clear_readback=failed SDL_error=%s\n", SDL_GetError());
        return kUnsupported;
    }
    std::puts("offscreen_clear_readback=success");
    if (triangle) {
        if (!vertexPath || !fragmentPath) {
            std::puts("triangle=blocked missing_offline_shader_paths");
            return kMissingShaders;
        }
        if (!PrepareTriangle(resources, requested, vertexPath, fragmentPath)
            || !ClearAndReadback(resources, true)) {
            std::printf("triangle=failed SDL_error=%s\n", SDL_GetError());
            return kUnsupported;
        }
        std::puts("triangle=success sampled_fullscreen_triangle viewport=16,16,32,32 "
            "fragment_samplers=1 fragment_uniform_buffers=1 storage_resources=0 offline_only=1");
    } else {
        std::puts("triangle=not_attempted");
    }
    return EXIT_SUCCESS;
}

}

int main(int argc, char** argv) {
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    SDL_SetLogOutputFunction(Log, nullptr);
    SDL_SetLogPriorities(SDL_LOG_PRIORITY_VERBOSE);
#if defined(_WIN32)
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
    constexpr DWORD maximumModulePath = 32768;
    std::array<char, maximumModulePath> path{};
    const auto length = GetModuleFileNameA(GetModuleHandleA("SDL3.dll"), path.data(), maximumModulePath);
    if (length == 0 || length >= maximumModulePath) {
        return EXIT_FAILURE;
    }
    std::printf("loaded_SDL3=%s\n", path.data());
#endif
    std::printf("SDL_runtime=%d header=%d\n", SDL_GetVersion(), SDL_VERSION);
    const bool triangle = argc == 5 || (argc == 3 && std::strcmp(argv[2], "offline-triangle") == 0);
    const bool dxbc = (argc == 3 || argc == 5) && std::strcmp(argv[2], "dxbc-fewer-resource-slots") == 0;
    const bool fewerResources = dxbc || ((argc == 3 || argc == 5)
        && std::strcmp(argv[2], "fewer-resource-slots") == 0);
    const bool validProfile = argc == 2 || ((argc == 3 || argc == 5)
        && (dxbc || fewerResources || std::strcmp(argv[2], "offline-triangle") == 0));
    if (!validProfile
        || (std::strcmp(argv[1], "metal") != 0 && std::strcmp(argv[1], "direct3d12") != 0)
        || SDL_GetVersion() != SDL_VERSION || !SDL_Init(SDL_INIT_VIDEO)) {
        std::printf("probe_init=failed SDL_error=%s\n", SDL_GetError());
        return EXIT_FAILURE;
    }
    const int result = Probe(argv[1], fewerResources, dxbc, triangle,
        argc == 5 ? argv[3] : nullptr, argc == 5 ? argv[4] : nullptr);
    SDL_Quit();
    return result;
}
