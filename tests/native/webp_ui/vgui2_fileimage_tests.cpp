#include "fileimage.h"
#include "FileSystem.h"
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <vector>
namespace vgui2 {
TestFileSystem filesystem;
TestFileSystem* g_pFileSystem = &filesystem;
}
int main(int argc, char** argv) {
    if (argc != 2) {
        return 2;
    }
    std::ifstream file(argv[1], std::ios::binary);
    std::vector<unsigned char> bytes((std::istreambuf_iterator<char>(file)), {});
    FileImageStream_Memory stream(bytes.data(), static_cast<int>(bytes.size()));
    FileImage image;
    const unsigned char expected[] = {0, 0, 255, 255, 0, 255, 0, 128, 255, 0, 0, 0, 0, 255, 255, 64};
    if (!Load32BitTGA(&stream, &image) || image.m_Width != 2 || image.m_Height != 2
        || !image.m_pData || std::memcmp(image.m_pData, expected, sizeof(expected))) {
        std::fprintf(stderr, "FAIL: VGUI2 font-atlas decoder preserves WebP top-down BGRA\n");
        return 1;
    }
    bytes.resize(bytes.size() - 3);
    FileImageStream_Memory truncated(bytes.data(), static_cast<int>(bytes.size()));
    if (Load32BitTGA(&truncated, &image) || image.m_pData) {
        std::fprintf(stderr, "FAIL: VGUI2 font-atlas decoder rejects truncated WebP\n");
        return 1;
    }
    std::vector<unsigned char> tga(18, 0);
    tga[2] = 10; tga[12] = 1; tga[14] = 1; tga[16] = 32;
    tga.insert(tga.end(), {128, 3, 2, 1, 77});
    FileImageStream_Memory legacy(tga.data(), static_cast<int>(tga.size()));
    const unsigned char bgra[] = {3, 2, 1, 77};
    if (!Load32BitTGA(&legacy, &image) || image.m_Width != 1 || image.m_Height != 1
        || !image.m_pData || std::memcmp(image.m_pData, bgra, sizeof(bgra))) {
        std::fprintf(stderr, "FAIL: VGUI2 legacy RLE TGA atlas keeps exact 4-byte pixel layout\n");
        return 1;
    }
    FILE* saved = std::tmpfile();
    Save32BitTGA(saved, &image);
    std::rewind(saved);
    unsigned char encoded[23]{};
    const std::size_t count = std::fread(encoded, 1, sizeof(encoded), saved);
    std::fclose(saved);
    if (count != sizeof(encoded) || encoded[2] != 10 || encoded[18] != 128
        || std::memcmp(encoded + 19, bgra, sizeof(bgra))) {
        std::fprintf(stderr, "FAIL: VGUI2 atlas stream writer retains its TGA byte contract\n");
        return 1;
    }
    return 0;
}
