#include "browser_wire.h"

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <vector>

namespace {
constexpr std::size_t PaintWidth = 1920;
constexpr std::size_t PaintHeight = 1080;
constexpr std::size_t BytesPerPixel = 4;
constexpr std::size_t Iterations = 300;
constexpr std::size_t SamplesPerPaint = 32;
constexpr std::size_t Trials = 4;

bool Run(const std::vector<std::uint8_t>& payload, bool borrowed, std::size_t trial) {
    std::uint64_t checksum = 0;
    const auto start = std::chrono::steady_clock::now();
    for (std::size_t iteration = 0; iteration < Iterations; ++iteration) {
        cse::browser::Reader reader(payload);
        std::vector<std::uint8_t> copy;
        const std::uint8_t* view = nullptr;
        std::size_t size = 0;
        if (borrowed) {
            if (!reader.BytesView(view, size)) {
                return false;
            }
        } else {
            if (!reader.Bytes(copy)) {
                return false;
            }
            view = copy.data();
            size = copy.size();
        }
        if (!reader.Finish() || size != PaintWidth * PaintHeight * BytesPerPixel) {
            return false;
        }
        for (std::size_t sample = 0; sample < SamplesPerPaint; ++sample) {
            checksum += view[(iteration * 8191 + sample * 131071) % size];
        }
    }
    const auto elapsed = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start);
    std::printf("%zu,%s,%zu,%.6f,%llu\n", trial, borrowed ? "borrowed" : "copy",
        PaintWidth * PaintHeight * BytesPerPixel, elapsed.count() / Iterations,
        static_cast<unsigned long long>(checksum));
    return true;
}
}

int main() {
    constexpr std::size_t ImageBytes = PaintWidth * PaintHeight * BytesPerPixel;
    std::vector<std::uint8_t> image(ImageBytes);
    for (std::size_t index = 0; index < image.size(); ++index) {
        image[index] = static_cast<std::uint8_t>(index * 37);
    }
    cse::browser::Writer writer;
    if (!writer.Bytes(image.data(), image.size())) {
        return 1;
    }
    const auto& payload = writer.Data();
    std::puts("trial,mode,image_bytes,milliseconds_per_paint,checksum");
    const std::array<bool, Trials * 2> order{false, true, true, false, false, true, true, false};
    for (std::size_t trial = 0; trial < order.size(); ++trial) {
        if (!Run(payload, order[trial], trial / 2 + 1)) {
            return 1;
        }
    }
}
