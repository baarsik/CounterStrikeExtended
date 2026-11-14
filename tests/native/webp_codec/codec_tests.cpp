#include <cstdio>

#include "image_codec.h"
#include <climits>
#include <cstring>
#include <vector>
#include <webp/encode.h>

namespace {
int failures = 0;
void Check(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", message);
        ++failures;
    }
}
}

int main() {
    using namespace cse::image;
    const unsigned char rgba[] = {
        255, 0, 0, 255, 0, 255, 0, 128,
        0, 0, 255, 64, 12, 34, 56, 0
    };
    std::vector<unsigned char> encoded;
    Check(EncodeWebPLossless(rgba, 2, 2, 8, 4, false, encoded), "encode RGBA");
    Check(IsWebP(encoded.data(), encoded.size()), "encoded file is WebP");
    Image image;
    Check(DecodeWebP(encoded.data(), encoded.size(), image), "decode RGBA");
    Check(image.width == 2 && image.height == 2, "preserve dimensions");
    Check(image.rgba == std::vector<unsigned char>(rgba, rgba + sizeof(rgba)),
          "preserve every RGBA channel including transparent RGB");
    Check(!DecodeWebP(encoded.data(), encoded.size(), image, sizeof(rgba) - 1), "reject small capacity");
    Check(image.width == 0 && image.height == 0 && image.rgba.empty(), "clear failed decode output");
    Check(!DecodeWebP(encoded.data(), encoded.size() - 1, image), "reject truncated WebP");
    Check(!DecodeWebP(nullptr, 0, image), "reject empty WebP");
    Check(!DecodeWebP(rgba, sizeof(rgba), image), "reject arbitrary bytes");
    auto animated = encoded;
    // Extended RIFF header with the animation feature flag. No frame should be silently selected.
    animated.insert(animated.begin() + 12, {'V', 'P', '8', 'X', 10, 0, 0, 0, 2, 0, 0, 0, 1, 0, 0, 1, 0, 0});
    const unsigned int riffLength = static_cast<unsigned int>(animated.size() - 8);
    for (unsigned int index = 0; index < 4; ++index) {
        animated[4 + index] = static_cast<unsigned char>(riffLength >> (8 * index));
    }
    Check(!DecodeWebP(animated.data(), animated.size(), image), "reject animation");
    unsigned char* lossy = nullptr;
    const std::size_t lossySize = WebPEncodeRGBA(rgba, 2, 2, 8, 90.0f, &lossy);
    Check(lossySize != 0 && DecodeWebP(lossy, lossySize, image), "decode lossy WebP");
    Check(image.width == 2 && image.height == 2 && image.rgba[7] == 128 && image.rgba[11] == 64,
          "preserve dimensions and alpha with lossy color");
    WebPFree(lossy);
    const unsigned char bottomUpRgb[] = {0, 0, 255, 12, 34, 56, 99, 99, 255, 0, 0, 0, 255, 0, 99, 99};
    Check(EncodeWebPLossless(bottomUpRgb, 2, 2, 8, 3, true, encoded), "encode padded bottom-up RGB");
    Check(DecodeWebP(encoded.data(), encoded.size(), image), "decode RGB capture");
    const unsigned char expectedRgb[] = {255, 0, 0, 255, 0, 255, 0, 255, 0, 0, 255, 255, 12, 34, 56, 255};
    Check(image.rgba == std::vector<unsigned char>(expectedRgb, expectedRgb + sizeof(expectedRgb)),
          "preserve RGB capture orientation and padding");
    Check(!EncodeWebPLossless(rgba, 0, 2, 8, 4, false, encoded), "reject zero width");
    Check(encoded.empty(), "clear failed encode output");
    Check(!EncodeWebPLossless(rgba, INT_MAX, 2, 8, 4, false, encoded), "reject excessive dimensions");
    Check(!EncodeWebPLossless(rgba, 2, 2, 7, 4, false, encoded), "reject short stride");
    Check(!EncodeWebPLossless(nullptr, 2, 2, 8, 4, false, encoded), "reject null pixels");
    Check(!EncodeWebPLossless(rgba, 2, 2, 8, 2, false, encoded), "reject unsupported channels");
    Check(WebPPath("gfx/panel.tga") == "gfx/panel.webp", "replace TGA extension");
    Check(WebPPath("gfx/panel") == "gfx/panel.webp", "append extension");
    Check(WebPPath("gfx.v1/panel") == "gfx.v1/panel.webp", "preserve directory dots");
    Check(WebPPath("gfx.v1\\panel.TGA") == "gfx.v1\\panel.webp", "Windows paths");
    Check(WebPPath("gfx/panel.WeBp") == "gfx/panel.WeBp", "preserve explicit path case");
    Check(HasWebPExtension("gfx/PANEL.WEBP"), "case-insensitive WebP extension");
    Check(!HasWebPExtension("gfx/panel.webp.tga"), "only final extension counts");
    Check(WebPPath(nullptr).empty() && WebPPath("").empty(), "reject empty path");
    std::printf("WebP codec: %d failures\n", failures);
    return failures != 0;
}
