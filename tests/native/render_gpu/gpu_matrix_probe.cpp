#include "gpu_adapter.h"

#include <cstdio>
#include <fstream>
#include <string>
#include <vector>

void CompareGpuTextureRaster(const cse::render::DrawPacket& draw,
    const std::vector<std::uint8_t>& original, int profile);

// Reuse the complete owned GL oracle and its authored texture sequences. Its
// normal executable has no GPU hook and retains its independent comparisons.
#define CSE_COMPARE_GPU_TEXTURES
#define main CseTextureOracleEntryPoint
#include "../render_state/gl_oracle.cpp"
#undef main
#undef CSE_COMPARE_GPU_TEXTURES

namespace {
cse::render::GpuOfflineShaders matrixShaders;
std::size_t matrixDifferences = 0;
unsigned int matrixComparisons = 0;
unsigned int matrixUnsupported = 0;
constexpr std::size_t MaximumShaderBytes = 16 * 1024 * 1024;
constexpr int LowPrecisionProfile = 3;

std::vector<std::uint8_t> ReadMatrixShader(const std::string& path) {
    std::ifstream input(path, std::ios::binary | std::ios::ate);
    const auto count = input.tellg();
    Check(count > 0 && static_cast<std::uint64_t>(count) <= MaximumShaderBytes, "bounded matrix shader");
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(count));
    input.seekg(0);
    input.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    Check(input.good() && bytes.size() >= 4 && std::memcmp(bytes.data(), "DXBC", 4) == 0,
        "complete actual offline DXBC shader");
    return bytes;
}

SDL_GPUDevice* CreateMatrixDevice() {
    const auto properties = SDL_CreateProperties();
    Check(properties != 0, "matrix device properties");
    Check(SDL_SetStringProperty(properties, SDL_PROP_GPU_DEVICE_CREATE_NAME_STRING, "direct3d12")
        && SDL_SetBooleanProperty(properties, SDL_PROP_GPU_DEVICE_CREATE_SHADERS_DXBC_BOOLEAN, true)
        && SDL_SetBooleanProperty(properties, SDL_PROP_GPU_DEVICE_CREATE_SHADERS_DXIL_BOOLEAN, false)
        && SDL_SetBooleanProperty(properties, SDL_PROP_GPU_DEVICE_CREATE_D3D12_ALLOW_FEWER_RESOURCE_SLOTS_BOOLEAN, true)
        && SDL_SetBooleanProperty(properties, SDL_PROP_GPU_DEVICE_CREATE_DEBUGMODE_BOOLEAN, false),
        "explicit matrix D3D12 profile");
    auto* device = SDL_CreateGPUDeviceWithProperties(properties);
    SDL_DestroyProperties(properties);
    Check(device != nullptr, "real matrix GPU device");
    Check(std::strcmp(SDL_GetGPUDeviceDriver(device), "direct3d12") == 0, "actual matrix D3D12 driver");
    const auto deviceProperties = SDL_GetGPUDeviceProperties(device);
    const char* name = SDL_GetStringProperty(deviceProperties, SDL_PROP_GPU_DEVICE_NAME_STRING, "[missing]");
    std::printf("matrix_GPU_driver=%s device=%s version=%s\n", SDL_GetGPUDeviceDriver(device), name,
        SDL_GetStringProperty(deviceProperties, SDL_PROP_GPU_DEVICE_DRIVER_VERSION_STRING, "[missing]"));
    Check(std::strcmp(name, "Parallels Display Adapter (WDDM)") == 0, "pinned matrix adapter, no WARP");
    return device;
}
}

void CompareGpuTextureRaster(const cse::render::DrawPacket& draw,
    const std::vector<std::uint8_t>& original, int profile) {
    using namespace cse::render;
    if (profile == LowPrecisionProfile) {
        GpuDrawData prepared;
        Check(PrepareGpuDraw(draw, prepared) == GpuStatus::Unsupported,
            "low-precision storage remains explicitly unsupported");
        ++matrixUnsupported;
        std::puts("matrix_profile=3 status=unsupported no_precision_substitution=1");
        return;
    }
    auto* device = CreateMatrixDevice();
    std::vector<std::uint8_t> rgba;
    std::string error;
    const auto status = RenderGpuOffscreen(device, matrixShaders, draw, kOracleSide, kOracleSide, rgba, error);
    SDL_DestroyGPUDevice(device);
    std::printf("matrix_profile=%d adapter_status=%d error=%s\n", profile, static_cast<int>(status), error.c_str());
    Check(status == GpuStatus::Ok, "selected matrix render succeeds");
    constexpr std::size_t RgbChannels = 3;
    constexpr std::size_t RgbaChannels = 4;
    Check(original.size() == kOracleSide * kOracleSide * RgbChannels
        && rgba.size() == kOracleSide * kOracleSide * RgbaChannels, "complete matrix readback extents");
    std::vector<std::uint8_t> rgb(original.size());
    for (std::size_t y = 0; y < kOracleSide; ++y) {
        for (std::size_t x = 0; x < kOracleSide; ++x) {
            for (std::size_t channel = 0; channel < RgbChannels; ++channel) {
                rgb[(y * kOracleSide + x) * RgbChannels + channel] =
                    rgba[((kOracleSide - 1 - y) * kOracleSide + x) * RgbaChannels + channel];
            }
        }
    }
    const auto name = std::string("texture-raster-") + std::to_string(profile);
    std::ofstream output(name + "-gpu.rgb", std::ios::binary);
    output.write(reinterpret_cast<const char*>(rgb.data()), static_cast<std::streamsize>(rgb.size()));
    Check(output.good(), "write exact matrix GPU bytes");
    std::size_t differences = 0;
    unsigned int maximumDifference = 0;
    for (std::size_t index = 0; index < rgb.size(); ++index) {
        if (rgb[index] != original[index]) {
            ++differences;
            const auto difference = static_cast<unsigned int>(std::abs(static_cast<int>(rgb[index]) - original[index]));
            maximumDifference = (std::max)(maximumDifference, difference);
        }
    }
    matrixDifferences += differences;
    ++matrixComparisons;
    std::printf("matrix_profile=%d RGB_bytes=%zu differing_bytes=%zu maximum_channel_difference=%u "
        "normalization=row-order-only\n", profile, rgb.size(), differences, maximumDifference);
}

int main(int argc, char** argv) {
    Check(argc == 2, "matching offline shader directory required");
    matrixShaders.vertex = ReadMatrixShader(std::string(argv[1]) + "/LegacyVertex.dxbc");
    matrixShaders.fragment = ReadMatrixShader(std::string(argv[1]) + "/LegacyFragment.dxbc");
    char textureModeArgument[] = "texture-raster";
    char* arguments[] = {argv[0], textureModeArgument};
    const int result = CseTextureOracleEntryPoint(2, arguments);
    Check(result == 0 && matrixComparisons == 3 && matrixUnsupported == 1, "all bounded matrix paths observed");
    std::printf("matrix_comparisons=%u unsupported_profiles=%u differing_RGB_bytes=%zu\n",
        matrixComparisons, matrixUnsupported, matrixDifferences);
    Check(matrixDifferences == 0, "exact selected GL versus SDL_GPU matrix comparisons");
    std::puts("PASS selected GPU texture/raster matrix; no full renderer or production selection claim");
}
