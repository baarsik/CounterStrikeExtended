#include "VGUI_BitmapTGA.h"
#include "VGUI_FileInputStream.h"
#include "fileimage.h"
#include "image_codec.h"
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

struct TestEngine {
    unsigned char* COM_LoadFile(char* path, int, int* length) {
        std::ifstream file(path, std::ios::binary);
        if (!file) {
            return nullptr;
        }
        std::vector<unsigned char> bytes((std::istreambuf_iterator<char>(file)), {});
        *length = static_cast<int>(bytes.size());
        auto* data = static_cast<unsigned char*>(std::malloc(bytes.size()));
        std::memcpy(data, bytes.data(), bytes.size());
        return data;
    }
    void COM_FreeFile(void* data) { std::free(data); }
} gEngfuncs;
// The runner inserts the exact client filesystem stream and loader functions.
#include "client_functions.inc"

namespace {
int failures = 0;
void Check(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", message);
        ++failures;
    }
}

class UnknownLengthStream : public vgui::InputStream {
public:
    explicit UnknownLengthStream(const std::vector<unsigned char>& bytes) : bytes_(bytes) {}
    void seekStart(bool& success) override { position_ = 0; success = true; }
    void seekRelative(int count, bool& success) override { position_ += count; success = position_ <= bytes_.size(); }
    void seekEnd(bool& success) override { position_ = bytes_.size(); success = true; }
    int getAvailable(bool& success) override { success = false; return 0; }
    unsigned char readUChar(bool& success) override {
        success = position_ < bytes_.size();
        return success ? bytes_[position_++] : 0;
    }
    void readUChar(unsigned char* output, int count, bool& success) override {
        success = count >= 0 && static_cast<std::size_t>(count) <= bytes_.size() - position_;
        if (success) {
            std::memcpy(output, bytes_.data() + position_, count);
            position_ += count;
        }
    }
    void close(bool& success) override { success = true; }
private:
    const std::vector<unsigned char>& bytes_;
    std::size_t position_ = 0;
};

void Write(const std::string& path, const std::vector<unsigned char>& bytes) {
    std::ofstream output(path, std::ios::binary);
    output.write(reinterpret_cast<const char*>(bytes.data()), bytes.size());
}
}

int main(int argc, char** argv) {
    if (argc != 3) {
        return 2;
    }
    std::ifstream input(argv[1], std::ios::binary);
    const std::vector<unsigned char> webp((std::istreambuf_iterator<char>(input)), {});
    const unsigned char expected[] = {255, 0, 0, 255, 0, 255, 0, 128, 0, 0, 255, 0, 255, 255, 0, 64};
    UnknownLengthStream stream(webp);
    vgui::BitmapTGA bitmap(&stream, true);
    Check(bitmap.width == 2 && bitmap.height == 2, "BitmapTGA decodes WebP without a stream length");
    Check(bitmap.pixels.size() == sizeof(expected) && !std::memcmp(bitmap.pixels.data(), expected, sizeof(expected)),
          "BitmapTGA preserves top-down RGBA pixels and alpha");
    UnknownLengthStream inverseStream(webp);
    vgui::BitmapTGA inverse(&inverseStream, false);
    Check(inverse.pixels.size() == sizeof(expected) && inverse.pixels[3] == 0 && inverse.pixels[7] == 127
          && inverse.pixels[11] == 255 && inverse.pixels[15] == 191,
          "BitmapTGA retains the no-invert entry point's legacy inverted alpha");

    FileImageStream_Memory fontStream(const_cast<unsigned char*>(webp.data()), static_cast<int>(webp.size()));
    FileImage font;
    Check(Load32BitTGA(&fontStream, &font), "Font atlas loader accepts WebP payload");
    const unsigned char expectedBgra[] = {0, 0, 255, 255, 0, 255, 0, 128, 255, 0, 0, 0, 0, 255, 255, 64};
    Check(font.m_Width == 2 && font.m_Height == 2 && font.m_pData
          && !std::memcmp(font.m_pData, expectedBgra, sizeof(expectedBgra)),
          "Font atlas WebP uses existing top-down BGRA layout");

    std::vector<unsigned char> truncated(webp.begin(), webp.end() - 3);
    FileImageStream_Memory badStream(truncated.data(), static_cast<int>(truncated.size()));
    FileImage bad;
    Check(!Load32BitTGA(&badStream, &bad) && !bad.m_pData, "Truncated WebP fails without publishing an atlas");
    std::vector<unsigned char> excessive(webp.begin(), webp.begin() + 12);
    excessive[4] = excessive[5] = excessive[6] = excessive[7] = 255;
    UnknownLengthStream excessiveStream(excessive);
    vgui::BitmapTGA excessiveBitmap(&excessiveStream, true);
    Check(excessiveBitmap.width == 0, "RIFF sizes beyond the encoded limit are rejected");

    // Existing bottom-up, RLE 32-bit TGA still loads; the file atlas is BGRA.
    std::vector<unsigned char> tga(18, 0);
    tga[2] = 10; tga[12] = 1; tga[14] = 1; tga[16] = 32;
    tga.insert(tga.end(), {128, 3, 2, 1, 77});
    FileImageStream_Memory legacyStream(tga.data(), static_cast<int>(tga.size()));
    FileImage legacy;
    Check(Load32BitTGA(&legacyStream, &legacy) && legacy.m_Width == 1 && legacy.m_pData[0] == 3,
          "Legacy RLE TGA atlas remains usable");

    const std::string base = std::string(argv[2]) + "/replacement";
    Write(base + ".webp", webp);
    Write(base + ".tga", tga);
    auto* clientBitmap = vgui_LoadTGA((base + ".tga").c_str());
    Check(clientBitmap && clientBitmap->width == 2 && clientBitmap->height == 2,
          "Client engine-filesystem adapter replaces legacy TGA names");
    delete clientBitmap;
    vgui::FileInputStream replacement((base + ".tga").c_str(), false);
    vgui::BitmapTGA replaced(&replacement, true);
    Check(replaced.width == 2 && replaced.height == 2, "Legacy .tga request prefers same-basename WebP");
    replacement.close();
    vgui::FileInputStream extensionless(base.c_str(), false);
    vgui::BitmapTGA fromBase(&extensionless, true);
    Check(fromBase.width == 2, "Extensionless stream request accepts WebP");
    extensionless.close();
    Write(base + ".webp", truncated);
    clientBitmap = vgui_LoadTGA((base + ".tga").c_str());
    Check(clientBitmap && clientBitmap->width == 1, "Client adapter falls back after invalid WebP");
    delete clientBitmap;
    vgui::FileInputStream fallback((base + ".tga").c_str(), false);
    vgui::BitmapTGA fromFallback(&fallback, true);
    Check(fromFallback.width == 1 && fromFallback.height == 1, "Invalid replacement falls back to legacy TGA");
    fallback.close();
    return failures ? 1 : 0;
}
