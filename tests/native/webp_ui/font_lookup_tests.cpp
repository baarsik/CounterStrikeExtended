#include "image_codec.h"
#include "fileimage.h"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

struct TestEngine {
    unsigned char* COM_LoadFile(char* path, int, int* length) {
        std::string normalized(path);
        std::replace(normalized.begin(), normalized.end(), '\\', '/');
        std::ifstream file(normalized, std::ios::binary);
        if (!file) {
            return nullptr;
        }
        const std::vector<unsigned char> bytes((std::istreambuf_iterator<char>(file)), {});
        *length = static_cast<int>(bytes.size());
        auto* data = static_cast<unsigned char*>(std::malloc(bytes.size()));
        std::memcpy(data, bytes.data(), bytes.size());
        return data;
    }
    void COM_FreeFile(void* data) { std::free(data); }
    void Con_Printf(const char*, ...) {}
} gEngfuncs;
struct TestCvar { float value = 1; } bitmapFonts;
TestCvar* g_CV_BitmapFonts = &bitmapFonts;

// The runner inserts the exact production helper (when present) and filename
// dispatch block; filesystem bytes then run through the real font-atlas decoder.
#include "font_lookup_functions.inc"

int main(int argc, char** argv) {
    if (argc != 2) {
        return 2;
    }
    std::ifstream input(argv[1], std::ios::binary);
    const std::vector<unsigned char> webp((std::istreambuf_iterator<char>(input)), {});
    auto write = [](const char* path, const std::vector<unsigned char>& bytes) {
        std::ofstream output(path, std::ios::binary);
        output.write(reinterpret_cast<const char*>(bytes.data()), bytes.size());
    };
    const char* webpName = "gfx/vgui/fonts/640_Test.webp";
    const char* tgaName = "gfx/vgui/fonts/640_Test.tga";
    write(webpName, webp);
    auto loaded = LoadSchemeFont(640, "Test");
    if (loaded != webp) {
        std::fprintf(stderr, "FAIL: Bitmap font dispatch opens WebP-only atlas for a legacy TGA request\n");
        return 1;
    }
    FileImageStream_Memory stream(loaded.data(), static_cast<int>(loaded.size()));
    FileImage image;
    if (!Load32BitTGA(&stream, &image) || image.m_Width != 2 || image.m_Height != 2
        || !image.m_pData || image.m_pData[2] != 255 || image.m_pData[7] != 128) {
        std::fprintf(stderr, "FAIL: Bitmap font WebP payload reaches the real atlas decoder\n");
        return 1;
    }
    std::vector<unsigned char> tga(18, 0);
    tga[2] = 10; tga[12] = 1; tga[14] = 1; tga[16] = 32;
    tga.insert(tga.end(), {128, 3, 2, 1, 77});
    write(tgaName, tga);
    if (LoadSchemeFont(640, "Test") != webp) {
        std::fprintf(stderr, "FAIL: Bitmap font WebP sidecar overrides an existing TGA atlas\n");
        return 1;
    }
    std::vector<unsigned char> truncated(webp.begin(), webp.begin() + 12);
    write(webpName, truncated);
    if (LoadSchemeFont(640, "Test") != tga) {
        std::fprintf(stderr, "FAIL: Invalid bitmap-font WebP replacement retains the TGA atlas\n");
        return 1;
    }
    std::remove(webpName);
    if (LoadSchemeFont(640, "Test") != tga) {
        std::fprintf(stderr, "FAIL: Missing bitmap-font WebP replacement retains the TGA atlas\n");
        return 1;
    }
    return 0;
}
