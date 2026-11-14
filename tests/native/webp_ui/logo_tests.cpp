#include "image_codec.h"
#include "disk_filesystem.h"
#include <cstring>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>
using byte = unsigned char;
struct TargaHeader {
    byte id_length, colormap_type, image_type;
    unsigned short colormap_index, colormap_length;
    byte colormap_size;
    unsigned short x_origin, y_origin, width, height;
    byte pixel_size, attributes;
} targa_header;
namespace vgui2 {
DiskFileSystem disk;
DiskFileSystem* filesystem() { return &disk; }
}
// The runner inserts exact production header reader and logo image loader.
#include "logo_functions.inc"
int main(int argc, char** argv) {
    if (argc != 3) {
        return 2;
    }
    std::ifstream input(argv[1], std::ios::binary);
    const std::vector<unsigned char> webp((std::istreambuf_iterator<char>(input)), {});
    const std::string base = std::string(argv[2]) + "/logo";
    auto write = [](const std::string& path, const std::vector<unsigned char>& bytes) {
        std::ofstream output(path, std::ios::binary);
        output.write(reinterpret_cast<const char*>(bytes.data()), bytes.size());
    };
    write(base + ".webp", webp);
    unsigned char pixels[16]{};
    const unsigned char expected[] = {255, 0, 0, 255, 0, 255, 0, 128, 0, 0, 255, 0, 255, 255, 0, 64};
    int width = 0;
    int height = 0;
    if (!LoadTGA((base + ".tga").c_str(), pixels, sizeof(pixels), &width, &height, 0)
        || width != 2 || height != 2 || std::memcmp(pixels, expected, sizeof(expected))) {
        std::fprintf(stderr, "FAIL: Logo loader replaces legacy TGA name with top-down RGBA WebP\n");
        return 1;
    }
    if (LoadTGA((base + ".webp").c_str(), pixels, 8, &width, &height, 0) || width || height) {
        std::fprintf(stderr, "FAIL: Logo WebP decoding honors caller capacity and clears dimensions\n");
        return 1;
    }
    if (!LoadTGA(base.c_str(), pixels, sizeof(pixels), &width, &height, 0) || width != 2) {
        std::fprintf(stderr, "FAIL: Logo extensionless WebP request works\n");
        return 1;
    }
    std::vector<unsigned char> truncated(webp.begin(), webp.begin() + 12);
    write(base + ".webp", truncated);
    std::vector<unsigned char> tga(18, 0);
    tga[2] = 2; tga[12] = 1; tga[14] = 1; tga[16] = 32;
    tga.insert(tga.end(), {3, 2, 1, 77});
    write(base + ".tga", tga);
    if (!LoadTGA((base + ".tga").c_str(), pixels, sizeof(pixels), &width, &height, 0)
        || width != 1 || height != 1 || pixels[0] != 1 || pixels[3] != 77) {
        std::fprintf(stderr, "FAIL: Invalid replacement retains legacy logo TGA pixels\n");
        return 1;
    }
    if (LoadTGA((base + ".webp").c_str(), pixels, sizeof(pixels), &width, &height, 0)) {
        std::fprintf(stderr, "FAIL: Explicit invalid WebP logo must fail\n");
        return 1;
    }
    return 0;
}
