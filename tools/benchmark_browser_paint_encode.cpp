#include "browser_paint_wire.h"

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <vector>

namespace {
constexpr std::size_t SamplesPerPaint = 32;
constexpr std::size_t Trials = 4;
constexpr std::size_t Iterations = 300;
constexpr std::size_t SampleStride = 131071;
constexpr std::size_t IterationStride = 8191;

__attribute__((noinline)) std::uint64_t SampleCopied(const std::vector<std::uint8_t>& payload,
    std::size_t imageBytes, std::size_t iteration) {
    constexpr std::size_t PrefixBytes = cse::browser::PaintWireParts::PrefixFields * sizeof(std::uint32_t);
    std::uint64_t checksum = payload.size() + payload[iteration % PrefixBytes] + payload[PrefixBytes + imageBytes];
    for (std::size_t sample = 0; sample < SamplesPerPaint; ++sample) {
        checksum += payload[PrefixBytes + (iteration * IterationStride + sample * SampleStride) % imageBytes];
    }
    return checksum;
}

__attribute__((noinline)) std::uint64_t SampleSegmented(const cse::browser::PaintWireParts& parts,
    std::size_t iteration) {
    std::uint64_t checksum = parts.payloadBytes + parts.prefix[iteration % parts.prefix.size()] +
        parts.popupLength[0];
    for (std::size_t sample = 0; sample < SamplesPerPaint; ++sample) {
        checksum += parts.viewPixels[(iteration * IterationStride + sample * SampleStride) % parts.viewBytes];
    }
    return checksum;
}

bool Run(int width, int height, const std::vector<std::uint8_t>& image, bool segmented, std::size_t trial) {
    std::uint64_t checksum = 0;
    const auto started = std::chrono::steady_clock::now();
    for (std::size_t iteration = 0; iteration < Iterations; ++iteration) {
        if (segmented) {
            cse::browser::PaintWireParts parts;
            if (!cse::browser::EncodePaintWireParts(1, width, height, {0, 0, width, height},
                0, 0, image, {}, parts)) {
                return false;
            }
            checksum += SampleSegmented(parts, iteration);
        } else {
            cse::browser::Writer payload;
            for (const int field : {1, width, height, 0, 0, width, height, 0, 0}) {
                payload.I32(field);
            }
            if (!payload.Bytes(image.data(), image.size()) || !payload.Bytes(nullptr, 0)) {
                return false;
            }
            checksum += SampleCopied(payload.Data(), image.size(), iteration);
        }
    }
    const double milliseconds = std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - started).count() / Iterations;
    std::printf("%dx%d,%zu,%s,%zu,%.6f,%llu\n", width, height, trial,
        segmented ? "segmented" : "copy", image.size(), milliseconds,
        static_cast<unsigned long long>(checksum));
    return true;
}
}

int main() {
    std::puts("size,trial,mode,image_bytes,milliseconds_per_paint,checksum");
    for (const auto dimensions : {std::array<int, 2>{546, 324}, std::array<int, 2>{1920, 1080}}) {
        const int width = dimensions[0];
        const int height = dimensions[1];
        std::vector<std::uint8_t> image(static_cast<std::size_t>(width) * height *
            cse::browser::PaintBytesPerPixel);
        for (std::size_t index = 0; index < image.size(); ++index) {
            image[index] = static_cast<std::uint8_t>(index * 37);
        }
        const std::array<bool, Trials * 2> order{false, true, true, false, false, true, true, false};
        for (std::size_t index = 0; index < order.size(); ++index) {
            if (!Run(width, height, image, order[index], index / 2 + 1)) {
                return 1;
            }
        }
    }
}
