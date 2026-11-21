#include "browser_paint.h"

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <vector>

namespace {
constexpr int kViewWidth = 728;
constexpr int kViewHeight = 432;
constexpr cse::browser::PaintBounds kDamage{8, 55, 500, 220};
constexpr std::size_t kIterations = 10000;
constexpr std::size_t kTrials = 4;

bool Run(bool partial, std::size_t trial) {
    const auto imageBytes = static_cast<std::size_t>(kViewWidth) * kViewHeight *
        cse::browser::PaintBytesPerPixel;
    const auto dirtyBytes = static_cast<std::size_t>(kDamage.width) * kDamage.height *
        cse::browser::PaintBytesPerPixel;
    std::vector<std::uint8_t> source(imageBytes);
    for (std::size_t index = 0; index < source.size(); ++index) {
        source[index] = static_cast<std::uint8_t>(index * 37);
    }
    std::vector<std::uint8_t> backing = source;
    const std::vector<cse::browser::PaintBounds> rectangles{kDamage};
    std::uint64_t checksum = 0;
    const auto started = std::chrono::steady_clock::now();
    for (std::size_t iteration = 0; iteration < kIterations; ++iteration) {
        const auto row = kDamage.y + static_cast<int>(iteration % kDamage.height);
        const auto offset = (static_cast<std::size_t>(row) * kViewWidth + kDamage.x) *
            cse::browser::PaintBytesPerPixel;
        source[offset] = static_cast<std::uint8_t>(iteration);
        if (partial) {
            cse::browser::UpdatePaintBacking(backing, source.data(), kViewWidth, kViewHeight, rectangles);
        } else {
            backing.assign(source.begin(), source.end());
        }
        checksum += backing[offset] + backing[(offset + dirtyBytes) % imageBytes];
    }
    const auto milliseconds = std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - started).count() / kIterations;
    std::printf("%zu,%s,%zu,%zu,%.6f,%llu\n", trial, partial ? "dirty_rows" : "full_image", imageBytes,
        dirtyBytes, milliseconds, static_cast<unsigned long long>(checksum));
    return backing == source;
}
}

int main() {
    std::puts("trial,mode,image_bytes,dirty_bytes,milliseconds_per_paint,checksum");
    const std::array<bool, kTrials * 2> order{false, true, true, false, false, true, true, false};
    for (std::size_t index = 0; index < order.size(); ++index) {
        if (!Run(order[index], index / 2 + 1)) {
            std::fputs("browser_paint_backing_benchmark_mismatch\n", stderr);
            return 1;
        }
    }
}
