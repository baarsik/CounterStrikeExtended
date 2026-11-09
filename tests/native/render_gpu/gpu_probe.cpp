#include "gpu_adapter.h"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#if defined(_WIN32)
#include <windows.h>
#endif

using namespace cse::render;

namespace {
constexpr std::uint32_t kSide = 64;
constexpr std::size_t kMaximumShaderBytes = 16 * 1024 * 1024;
#if defined(__APPLE__)
constexpr const char* kDriver = "metal";
constexpr const char* kShaderExtension = ".metallib";
constexpr const char* kShaderMagic = "MTLB";
#else
constexpr const char* kDriver = "direct3d12";
constexpr const char* kShaderExtension = ".dxbc";
constexpr const char* kShaderMagic = "DXBC";
#endif

void Check(bool value, const char* stage) {
    if (!value) {
        std::fprintf(stderr, "FAIL GPU adapter %s: %s\n", stage, SDL_GetError());
        std::exit(EXIT_FAILURE);
    }
}

std::vector<std::uint8_t> Read(const std::string& path, std::size_t maximum) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    const auto bytes = file.tellg();
    Check(bytes > 0 && static_cast<std::uint64_t>(bytes) <= maximum, "bounded input file");
    std::vector<std::uint8_t> result(static_cast<std::size_t>(bytes));
    file.seekg(0);
    file.read(reinterpret_cast<char*>(result.data()), static_cast<std::streamsize>(result.size()));
    Check(file.good(), "read complete input");
    return result;
}

Command Make(Op op, std::initializer_list<double> values = {}, std::uint32_t argument = 0) {
    Command command;
    command.op = op;
    command.argument = argument;
    std::copy(values.begin(), values.end(), command.values.begin());
    return command;
}

DrawPacket Packet() {
    Recorder recorder;
    const auto send = [&recorder](const Command& command) {
        Check(recorder.Submit(command) == Status::Ok, "capture selected command");
    };
    send(Make(Op::Viewport, {0, 0, kSide, kSide}));
    send(Make(Op::Scale, {0.5, 0.5, 1}));
    send(Make(Op::Enable, {1}, static_cast<std::uint32_t>(Capability::Texture2D)));
    auto bind = Make(Op::BindTexture);
    bind.second = 201;
    send(bind);
    auto image = Make(Op::TextureImage);
    image.upload.width = 2;
    image.upload.height = 2;
    image.upload.bytes = {255, 0, 0, 255, 0, 255, 0, 255, 0, 0, 255, 255, 255, 255, 255, 255};
    send(image);
    for (auto parameter : {TextureParameter::MinFilter, TextureParameter::MagFilter,
        TextureParameter::WrapS, TextureParameter::WrapT}) {
        auto command = Make(Op::TextureParameter, {parameter <= TextureParameter::MagFilter ?
            static_cast<double>(Filter::Nearest) : static_cast<double>(Wrap::ClampToEdge)});
        command.second = static_cast<std::uint32_t>(parameter);
        send(command);
    }
    auto environment = Make(Op::TextureEnvironment, {}, static_cast<std::uint32_t>(EnvironmentParameter::Mode));
    environment.second = static_cast<std::uint32_t>(EnvironmentMode::Replace);
    send(environment);
    send(Make(Op::Begin));
    constexpr std::array<std::array<float, 2>, 6> corners{{{{-2, -2}}, {{2, -2}}, {{2, 2}},
        {{-2, -2}}, {{2, 2}}, {{-2, 2}}}};
    for (const auto& corner : corners) {
        send(Make(Op::TexCoord, {(corner[0] + 2) / 4, (corner[1] + 2) / 4}));
        send(Make(Op::Vertex, {corner[0], corner[1], -0.5}));
    }
    send(Make(Op::End));
    return recorder.Draws().front();
}

void Log(void*, int category, SDL_LogPriority priority, const char* message) {
    std::printf("SDL_log category=%d priority=%d %s\n", category, static_cast<int>(priority), message);
}
}

int main(int argc, char** argv) {
    std::setvbuf(stdout, nullptr, _IONBF, 0);
#if defined(_WIN32)
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
#endif
    Check(argc == 3, "shader directory and frozen reference required");
    GpuOfflineShaders shaders;
    shaders.vertex = Read(std::string(argv[1]) + "/LegacyVertex" + kShaderExtension, kMaximumShaderBytes);
    shaders.fragment = Read(std::string(argv[1]) + "/LegacyFragment" + kShaderExtension, kMaximumShaderBytes);
#if defined(__APPLE__)
    shaders.format = SDL_GPU_SHADERFORMAT_METALLIB;
#endif
    Check(shaders.vertex.size() >= 4 && shaders.fragment.size() >= 4 &&
        std::memcmp(shaders.vertex.data(), kShaderMagic, 4) == 0 &&
        std::memcmp(shaders.fragment.data(), kShaderMagic, 4) == 0,
        "actual offline shader containers");
    const auto expected = Read(argv[2], kSide * kSide * 3);
    Check(expected.size() == kSide * kSide * 3, "frozen GL RGB byte count");
    Check(SDL_GetVersion() == SDL_VERSION, "matching pinned SDL3.4.20");
    SDL_SetLogOutputFunction(Log, nullptr);
    SDL_SetLogPriorities(SDL_LOG_PRIORITY_VERBOSE);
    std::puts("stage=initialize-SDL-video");
    Check(SDL_Init(SDL_INIT_VIDEO), "SDL video");
    const auto properties = SDL_CreateProperties();
    SDL_SetStringProperty(properties, SDL_PROP_GPU_DEVICE_CREATE_NAME_STRING, kDriver);
#if defined(__APPLE__)
    SDL_SetBooleanProperty(properties, SDL_PROP_GPU_DEVICE_CREATE_SHADERS_METALLIB_BOOLEAN, true);
#else
    SDL_SetBooleanProperty(properties, SDL_PROP_GPU_DEVICE_CREATE_SHADERS_DXBC_BOOLEAN, true);
    SDL_SetBooleanProperty(properties, SDL_PROP_GPU_DEVICE_CREATE_SHADERS_DXIL_BOOLEAN, false);
    SDL_SetBooleanProperty(properties, SDL_PROP_GPU_DEVICE_CREATE_D3D12_ALLOW_FEWER_RESOURCE_SLOTS_BOOLEAN, true);
#endif
    SDL_SetBooleanProperty(properties, SDL_PROP_GPU_DEVICE_CREATE_VERBOSE_BOOLEAN, true);
    SDL_SetBooleanProperty(properties, SDL_PROP_GPU_DEVICE_CREATE_DEBUGMODE_BOOLEAN, false);
    std::printf("stage=create-real-GPU-device driver=%s format=%u storage_resources=0 "
        "fragment_samplers=3 UBOs=1-per-stage\n", kDriver, shaders.format);
    auto* device = SDL_CreateGPUDeviceWithProperties(properties);
    SDL_DestroyProperties(properties);
    Check(device != nullptr, "GPU device");
    Check(std::strcmp(SDL_GetGPUDeviceDriver(device), kDriver) == 0, "selected actual GPU driver");
    const auto deviceProperties = SDL_GetGPUDeviceProperties(device);
    const auto* name = SDL_GetStringProperty(deviceProperties, SDL_PROP_GPU_DEVICE_NAME_STRING, "[missing]");
    std::printf("actual_GPU_driver=%s actual_device=%s shader_formats=%u driver_version=%s\n",
        SDL_GetGPUDeviceDriver(device), name, SDL_GetGPUShaderFormats(device),
        SDL_GetStringProperty(deviceProperties, SDL_PROP_GPU_DEVICE_DRIVER_VERSION_STRING, "[missing]"));
#if defined(__APPLE__)
    Check(std::strncmp(name, "Apple ", 6) == 0, "actual Apple GPU identity");
#else
    Check(std::strcmp(name, "Parallels Display Adapter (WDDM)") == 0, "pinned real adapter identity, no WARP");
#endif
    const auto packet = Packet();
    std::vector<std::uint8_t> actual;
    std::string error;
    std::puts("stage=render-owned-packet");
    const auto result = RenderGpuOffscreen(device, shaders, packet, kSide, kSide, actual, error);
    std::printf("adapter_status=%d error=%s rgba_bytes=%zu\n", static_cast<int>(result), error.c_str(), actual.size());
    SDL_DestroyGPUDevice(device);
    SDL_Quit();
    Check(result == GpuStatus::Ok && actual.size() == kSide * kSide * 4, "actual authored shader render");
    std::ofstream raw("gpu-authored-top-first.rgba", std::ios::binary);
    raw.write(reinterpret_cast<const char*>(actual.data()), static_cast<std::streamsize>(actual.size()));
    Check(raw.good(), "write raw GPU readback");
    std::vector<std::uint8_t> canonical(expected.size());
    for (std::size_t y = 0; y < kSide; ++y) {
        for (std::size_t x = 0; x < kSide; ++x) {
            for (std::size_t channel = 0; channel < 3; ++channel) {
                canonical[(y * kSide + x) * 3 + channel] =
                    actual[((kSide - 1 - y) * kSide + x) * 4 + channel];
            }
        }
    }
    std::ofstream rgb("gpu-authored-gl-row-order.rgb", std::ios::binary);
    rgb.write(reinterpret_cast<const char*>(canonical.data()), static_cast<std::streamsize>(canonical.size()));
    Check(rgb.good(), "write canonical RGB readback");
    std::size_t differences = 0;
    for (std::size_t index = 0; index < canonical.size(); ++index) {
        differences += canonical[index] != expected[index] ? 1 : 0;
    }
    std::printf("GL_reference_RGB_bytes=%zu differing_bytes=%zu transform=dyadic-half-scale "
        "normalization=readback-row-order-only\n", canonical.size(), differences);
    Check(differences == 0, "first authored GPU versus frozen actual GL raster");
    std::puts("PASS first authored shader/owned packet proof; no untested branch or full renderer acceptance");
}
