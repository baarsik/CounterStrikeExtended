#include "image_codec.h"
#include "legacy_bmp_records.h"
#include "disk_filesystem.h"
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iterator>
#include <map>
#include <string>
#include <vector>
namespace cse { namespace vgui {
constexpr int PixelChannels = 4;
constexpr int MaximumRasterDimension = 16384;
} }
constexpr int OpaqueAlpha = 255;
constexpr std::size_t MaximumFileBytes = 64 * 1024 * 1024;
struct TestRasterImage {
    int width = 0;
    int height = 0;
    std::string filename;
    std::vector<unsigned char> pixels;
    bool Assign(const unsigned char* source, int wide, int tall, bool, bool) {
        width = wide;
        height = tall;
        pixels.assign(source, source + static_cast<std::size_t>(wide) * tall * 4);
        return true;
    }
};
struct CSDLSurface {
    void DrawSetTextureFile(int, const char*, int, bool);
    bool ReadFile(const char* path, std::vector<unsigned char>& bytes, const char* = nullptr) {
        std::ifstream file(path, std::ios::binary);
        if (!file) {
            return false;
        }
        bytes.assign(std::istreambuf_iterator<char>(file), {});
        return true;
    }
    std::map<int, TestRasterImage> textures_{{1, {}}};
    int texture_ = 0;
};

using HDC = void*;
constexpr int BI_RGB = 0;
constexpr int DIB_RGB_COLORS = 0;
struct BITMAPINFOHEADER {
    int biSize, biWidth, biHeight, biPlanes, biBitCount, biCompression, biSizeImage;
    int biXPelsPerMeter, biYPelsPerMeter, biClrUsed, biClrImportant;
};
struct BITMAPINFO { BITMAPINFOHEADER bmiHeader; };
void* CreateDIBSection(HDC, BITMAPINFO* info, int, void** bits, void*, int) {
    *bits = std::calloc(static_cast<std::size_t>(info->bmiHeader.biWidth) * -info->bmiHeader.biHeight, 4);
    return *bits;
}
struct Texture {
    int _wide = 0;
    int _tall = 0;
    void* _icon = nullptr;
    void* _bitmap = nullptr;
    void* _maskBitmap = nullptr;
    void* _bits = nullptr;
    void* _maskBits = nullptr;
    bool _masked = false;
    ~Texture() { std::free(_bits); std::free(_maskBits); }
};
#pragma pack(push, 1)
struct TGAHeader_t {
    unsigned char id_length, colormap_type, image_type;
    unsigned short colormap_index, colormap_length;
    unsigned char colormap_size;
    unsigned short x_origin, y_origin, width, height;
    unsigned char pixel_size, attributes;
};
#pragma pack(pop)
struct Context { HDC hdc = nullptr; };
struct CWin32Surface {
    bool LoadTGA(Texture*, const char*);
    bool LoadWebP(Texture*, const char*);
    bool UploadImagePixels(Texture*, const unsigned char*, int, int, bool);
    bool LoadBMP(Texture*, const char*) { return false; }
    Context* ContextPlat() { return &context; }
    Context context;
};
struct Debug { void DPrintf2(const char*, ...) {} };
Debug debug;
Debug* g_pIVgui = &debug;
DiskFileSystem filesystem;
DiskFileSystem* g_pFileSystem = &filesystem;

// The runner inserts exact production decode, SDL loader and Win32 methods.
#include "surface_functions.inc"

int main(int argc, char** argv) {
    if (argc != 3) {
        return 2;
    }
    std::ifstream input(argv[1], std::ios::binary);
    const std::vector<unsigned char> webp((std::istreambuf_iterator<char>(input)), {});
    const std::string base = std::string(argv[2]) + "/surface";
    auto write = [](const std::string& path, const std::vector<unsigned char>& bytes) {
        std::ofstream output(path, std::ios::binary);
        output.write(reinterpret_cast<const char*>(bytes.data()), bytes.size());
    };
    write(base + ".webp", webp);
    CSDLSurface sdl;
    sdl.DrawSetTextureFile(1, (base + ".tga").c_str(), 1, true);
    const unsigned char rgba[] = {255, 0, 0, 255, 0, 255, 0, 128, 0, 0, 255, 0, 255, 255, 0, 64};
    if (sdl.textures_[1].pixels.size() != sizeof(rgba)
        || std::memcmp(sdl.textures_[1].pixels.data(), rgba, sizeof(rgba))) {
        std::fprintf(stderr, "FAIL: SDL legacy request prefers WebP with top-down RGBA and alpha\n");
        return 1;
    }
    write(base + ".WEBP", webp);
    sdl.DrawSetTextureFile(1, (base + ".WEBP").c_str(), 0, true);
    if (sdl.textures_[1].width != 2 || sdl.textures_[1].filename != base + ".WEBP") {
        std::fprintf(stderr, "FAIL: SDL explicit case-insensitive WebP extension loads directly\n");
        return 1;
    }
    CWin32Surface win;
    Texture winTexture;
    if (!win.LoadTGA(&winTexture, base.c_str()) || winTexture._wide != 2 || winTexture._tall != 2
        || !winTexture._masked) {
        std::fprintf(stderr, "FAIL: Win32 extensionless WebP upload retains dimensions and mask\n");
        return 1;
    }
    const auto* pixels = static_cast<const unsigned char*>(winTexture._bits);
    const auto* mask = static_cast<const std::uint32_t*>(winTexture._maskBits);
    if (pixels[0] != 0 || pixels[2] != 255 || mask[0] != 0 || mask[2] != 0xffffffffu
        || pixels[8] != 0 || pixels[9] != 0 || pixels[10] != 0) {
        std::fprintf(stderr, "FAIL: Win32 WebP pixels retain BGRA color and binary transparency convention\n");
        return 1;
    }
    std::vector<unsigned char> truncated(webp.begin(), webp.begin() + 12);
    write(base + ".webp", truncated);
    std::vector<unsigned char> tga(18, 0);
    tga[2] = 10; tga[12] = 1; tga[14] = 1; tga[16] = 32;
    tga.insert(tga.end(), {128, 3, 2, 1, 255});
    write(base + ".tga", tga);
    sdl.DrawSetTextureFile(1, base.c_str(), 0, true);
    Texture fallback;
    if (sdl.textures_[1].width != 1 || !win.LoadTGA(&fallback, (base + ".tga").c_str())
        || fallback._wide != 1) {
        std::fprintf(stderr, "FAIL: Invalid WebP replacement retains TGA fallback on both surfaces\n");
        return 1;
    }
    std::remove((base + ".tga").c_str());
    constexpr std::size_t PaletteEntries = 256;
    constexpr std::size_t HeaderBytes = sizeof(cse_bmp::FileHeader) + sizeof(cse_bmp::InfoHeader);
    constexpr std::size_t PaletteBytes = PaletteEntries * sizeof(cse_bmp::PaletteEntry);
    std::vector<unsigned char> bmp(HeaderBytes + PaletteBytes + 4, 0);
    cse_bmp::FileHeader header{};
    header.bfType = 0x4d42;
    header.bfOffBits = HeaderBytes + PaletteBytes;
    cse_bmp::InfoHeader info{};
    info.biSize = sizeof(info);
    info.biWidth = 1;
    info.biHeight = 1;
    info.biPlanes = 1;
    info.biBitCount = 8;
    std::memcpy(bmp.data(), &header, sizeof(header));
    std::memcpy(bmp.data() + sizeof(header), &info, sizeof(info));
    bmp[HeaderBytes + 4] = 3;
    bmp[HeaderBytes + 5] = 2;
    bmp[HeaderBytes + 6] = 1;
    bmp[header.bfOffBits] = 1;
    write(base + ".bmp", bmp);
    sdl.DrawSetTextureFile(1, base.c_str(), 0, true);
    const unsigned char bitmapRgba[] = {1, 2, 3, 255};
    if (sdl.textures_[1].pixels.size() != sizeof(bitmapRgba)
        || std::memcmp(sdl.textures_[1].pixels.data(), bitmapRgba, sizeof(bitmapRgba))) {
        std::fprintf(stderr, "FAIL: SDL retains indexed BMP fallback after invalid WebP and absent TGA\n");
        return 1;
    }
    return 0;
}
