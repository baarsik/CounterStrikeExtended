#include <SDL3/SDL.h>
#include "../../../GoldSrc-SDK/public/native_module.h"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <limits>
#include <vector>

namespace {
constexpr int SourceFrames = 4097;
constexpr std::array<int, 4> Rates{11025, 22050, 44100, 48000};
constexpr std::array<int, 2> Channels{1, 2};
constexpr std::array<SDL_AudioFormat, 3> Formats{SDL_AUDIO_U8, SDL_AUDIO_S16LE, SDL_AUDIO_F32LE};
constexpr std::uint32_t Seed = 0x63736531U;
constexpr std::uint32_t Multiplier = 1664525U;
constexpr std::uint32_t Increment = 1013904223U;

void WriteU32(std::ofstream& output, std::uint32_t value) {
    const std::array<unsigned char, 4> bytes{static_cast<unsigned char>(value),
        static_cast<unsigned char>(value >> 8), static_cast<unsigned char>(value >> 16),
        static_cast<unsigned char>(value >> 24)};
    output.write(reinterpret_cast<const char*>(bytes.data()), bytes.size());
}

std::vector<unsigned char> Samples(SDL_AudioFormat format, int channels) {
    const int sampleBytes = SDL_AUDIO_BYTESIZE(format);
    std::vector<unsigned char> bytes(static_cast<std::size_t>(SourceFrames) * channels * sampleBytes);
    std::uint32_t state = Seed;
    for (std::size_t offset = 0; offset < bytes.size(); offset += sampleBytes) {
        state = state * Multiplier + Increment;
        std::uint32_t bits = state;
        if (format == SDL_AUDIO_F32LE) {
            constexpr int Midpoint = 1 << 23;
            const float value = static_cast<float>(static_cast<int>(state >> 8) - Midpoint) / Midpoint;
            std::memcpy(&bits, &value, sizeof(bits));
        }
        for (int index = 0; index < sampleBytes; ++index) {
            bytes[offset + index] = static_cast<unsigned char>(bits >> (index * 8));
        }
    }
    return bytes;
}
}

int main(int argc, char** argv) {
    if (argc != 3) {
        return 1;
    }
    void* module = cse_platform::LoadModule(argv[1], cse_platform::ModuleFallback::None);
    if (!module) {
        return 2;
    }
    const auto version = reinterpret_cast<decltype(&SDL_GetVersion)>(cse_platform::GetSymbol(module, "SDL_GetVersion"));
    const auto convert = reinterpret_cast<decltype(&SDL_ConvertAudioSamples)>(
        cse_platform::GetSymbol(module, "SDL_ConvertAudioSamples"));
    const auto release = reinterpret_cast<decltype(&SDL_free)>(cse_platform::GetSymbol(module, "SDL_free"));
    if (!version || !convert || !release || version() != SDL_VERSIONNUM(3, 4, 20)) {
        return 3;
    }
    std::ofstream output(argv[2], std::ios::binary);
    output.write("CSEPCM1\0", 8);
    std::uint32_t records = 0;
    for (const auto sourceFormat : Formats) {
        for (const auto destinationFormat : Formats) {
            for (const int sourceChannels : Channels) {
                const auto source = Samples(sourceFormat, sourceChannels);
                for (const int destinationChannels : Channels) {
                    for (const int sourceRate : Rates) {
                        for (const int destinationRate : Rates) {
                            const SDL_AudioSpec sourceSpec{sourceFormat, sourceChannels, sourceRate};
                            const SDL_AudioSpec destinationSpec{destinationFormat, destinationChannels, destinationRate};
                            unsigned char* destination = nullptr;
                            int length = 0;
                            if (!convert(&sourceSpec, source.data(), static_cast<int>(source.size()),
                                &destinationSpec, &destination, &length) || !destination || length <= 0) {
                                return 4;
                            }
                            WriteU32(output, static_cast<std::uint32_t>(sourceFormat));
                            WriteU32(output, static_cast<std::uint32_t>(destinationFormat));
                            WriteU32(output, static_cast<std::uint32_t>(sourceChannels));
                            WriteU32(output, static_cast<std::uint32_t>(destinationChannels));
                            WriteU32(output, static_cast<std::uint32_t>(sourceRate));
                            WriteU32(output, static_cast<std::uint32_t>(destinationRate));
                            WriteU32(output, static_cast<std::uint32_t>(length));
                            output.write(reinterpret_cast<const char*>(destination), length);
                            release(destination);
                            ++records;
                        }
                    }
                }
            }
        }
    }
    output.close();
    if (!output.good()) {
        return 5;
    }
    cse_platform::UnloadModule(module);
    std::printf("Converted %u format/channel/rate records.\n", records);
    return 0;
}
