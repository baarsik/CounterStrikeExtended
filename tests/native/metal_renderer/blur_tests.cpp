#include "radar_renderer_gpu.h"
#include "metal_session.h"
#include "../../../src/public/vgui_backdrop.h"
#include <SDL3/SDL.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <vector>

using namespace cse::render;
namespace {
constexpr unsigned kSide = 64;
constexpr unsigned kRgbChannels = 3;
constexpr double kByteMaximum = 255;
constexpr int kRadarRadius = 16;
constexpr double kRadarSigma = 6;
constexpr double kSmokedOffsetScale = 1.15;
constexpr unsigned kMaskInset = 2;
constexpr unsigned kEpoch = 7;
constexpr int kTolerance = 2;
using Pixel = std::array<double, kRgbChannels>;
using Image = std::vector<Pixel>;

void Require(bool condition, const std::string& message) {
    if (!condition) {
        std::cerr << message << '\n';
        std::exit(1);
    }
}

Pixel Sample(const Image& pixels, unsigned side, double x, double y) {
    const int left = int(std::floor(x));
    const int top = int(std::floor(y));
    const double horizontal = x - left;
    const double vertical = y - top;
    Pixel result{};
    for (int row = 0; row < 2; ++row) {
        for (int column = 0; column < 2; ++column) {
            const auto& sample = pixels[std::clamp(top + row, 0, int(side) - 1) * side +
                std::clamp(left + column, 0, int(side) - 1)];
            const auto weight = (column ? horizontal : 1 - horizontal) * (row ? vertical : 1 - vertical);
            for (unsigned channel = 0; channel < kRgbChannels; ++channel) {
                result[channel] += sample[channel] * weight;
            }
        }
    }
    return result;
}

Image Reference(bool smoked) {
    const unsigned side = smoked ? kSide / 4 : kSide / 2;
    Image input(side * side);
    for (unsigned y = 0; y < side; ++y) {
        for (unsigned x = 0; x < side; ++x) {
            input[y * side + x] = {{x < side / 2 ? 1.0 : 0.0, y < side / 2 ? 1.0 : 0.0,
                x < side / 2 ? 0.0 : 1.0}};
        }
    }
    for (unsigned direction = 0; direction < 2; ++direction) {
        Image output(input.size());
        for (unsigned y = 0; y < side; ++y) {
            for (unsigned x = 0; x < side; ++x) {
                Pixel value{};
                if (smoked) {
                    constexpr double weights[] = {0.2270270270, 0.3162162162, 0.3162162162, 0.0702702703, 0.0702702703};
                    constexpr double offsets[] = {0, 1.3846153846, -1.3846153846, 3.2307692308, -3.2307692308};
                    for (unsigned tap = 0; tap < std::size(weights); ++tap) {
                        const double offset = offsets[tap] * kSmokedOffsetScale;
                        const auto sample = Sample(input, side, double(x) + (direction ? 0 : offset),
                            double(y) + (direction ? offset : 0));
                        for (unsigned channel = 0; channel < kRgbChannels; ++channel) {
                            // Actual legacy Smoked writes each additive tap to an RGBA8 attachment.
                            value[channel] = std::round((value[channel] + sample[channel] * weights[tap]) * kByteMaximum) / kByteMaximum;
                        }
                    }
                } else {
                    double sum = 0;
                    for (int offset = -kRadarRadius; offset <= kRadarRadius; ++offset) {
                        const double weight = std::exp(-double(offset * offset) / (2 * kRadarSigma * kRadarSigma));
                        const auto sample = Sample(input, side, double(x) + (direction ? 0 : offset),
                            double(y) + (direction ? offset : 0));
                        sum += weight;
                        for (unsigned channel = 0; channel < kRgbChannels; ++channel) {
                            value[channel] += sample[channel] * weight;
                        }
                    }
                    for (auto& channel : value) {
                        channel = std::round(channel / sum * kByteMaximum) / kByteMaximum;
                    }
                }
                output[y * side + x] = value;
            }
        }
        input = std::move(output);
    }
    Image result(kSide * kSide);
    for (unsigned y = 0; y < kSide; ++y) {
        for (unsigned x = 0; x < kSide; ++x) {
            result[y * kSide + x] = Sample(input, side, (x + 0.5) * side / kSide - 0.5,
                (y + 0.5) * side / kSide - 0.5);
        }
    }
    return result;
}
}

int main() {
    Require(SDL_Init(SDL_INIT_VIDEO), "SDL init");
    std::string error;
    MetalSession session;
    Require(session.Initialize(nullptr, true, error), error);
    Require(session.Resize(kSide, kSide, 1, error), error);
    RadarRendererGpu effects(session, kEpoch);
    RasterState raster;
    raster.viewport = {{0, 0, kSide, kSide}};
    FrameSnapshot frame;
    frame.info.width = kSide;
    frame.info.height = kSide;
    for (unsigned y = 0; y < 2; ++y) {
        for (unsigned x = 0; x < 2; ++x) {
            FrameEvent clear;
            clear.kind = FrameEventKind::Clear;
            clear.clear.mask = kClearColor;
            clear.clear.color = {{x ? 0.0f : 1.0f, y ? 0.0f : 1.0f, x ? 1.0f : 0.0f, 1}};
            clear.clear.scissorEnabled = true;
            clear.clear.scissor = {{int(x * kSide / 2), int(y * kSide / 2), kSide / 2, kSide / 2}};
            frame.events.push_back(clear);
        }
    }
    for (bool smoked : {false, true}) {
        Require(session.Render(frame, error), error);
        if (smoked) {
            const cse::ui::BackdropRect panel{0, 0, kSide, kSide};
            const cse::ui::BackdropStyle style{1, 1, 0};
            effects.BeginSmokedFrame();
            Require(!effects.DrawSmoked(panel, panel, kSide, kSide, style, 0, raster), "initial capture request");
            Require(effects.CaptureSmoked(0, 0, kSide, kSide), "Smoked capture");
            Require(effects.DrawSmoked(panel, panel, kSide, kSide, style, 0, raster), "Smoked draw");
        } else {
            CseRadarBackdropDraw backdrop{};
            backdrop.size = sizeof(backdrop);
            backdrop.rendererEpoch = kEpoch;
            backdrop.hud = {kSide, kSide};
            backdrop.extent = kSide;
            backdrop.shape = CSE_RADAR_SHAPE_SQUARE;
            backdrop.darken = 1;
            backdrop.opacity = 1;
            Require(effects.DrawRadarBackdrop(backdrop, raster) == CSE_RADAR_OK, "radar backdrop");
        }
        std::vector<unsigned char> actual(kSide * kSide * 4);
        Require(session.ReadPixels(0, 0, kSide, kSide, 4, 1, actual.data(), error), error);
        const auto expected = Reference(smoked);
        int maximum = 0;
        // Exclude only the analytic 1.5px outer backdrop mask; compare the complete interior transition.
        for (unsigned y = kMaskInset; y < kSide - kMaskInset; ++y) {
            for (unsigned x = kMaskInset; x < kSide - kMaskInset; ++x) {
                for (unsigned channel = 0; channel < kRgbChannels; ++channel) {
                    const int byte = int(std::round(expected[y * kSide + x][channel] * kByteMaximum));
                    maximum = std::max(maximum, std::abs(int(actual[(y * kSide + x) * 4 + channel]) - byte));
                }
            }
        }
        std::cout << (smoked ? "Smoked5" : "radar33") << " max component delta " << maximum << '\n';
        Require(maximum <= kTolerance, "filter differs from independent double/RGBA8 convolution oracle");
    }
}
