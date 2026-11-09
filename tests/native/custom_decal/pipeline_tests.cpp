#include "draw.h"
#include "custom.h"
#include "model.h"
#include "bspfile.h"
#include "common.h"
#include "conprint.h"
#include "sys.h"
#if defined(_WIN64) || defined(__LP64__)
#include "custom_decal_records.h"
#endif
#if !defined(SW_RENDERER)
#include "glquake.h"
#endif
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

qboolean gfCustomBuild = false;
char szCustName[10] = "T00500";
byte texgammatable[256]{};
int gl_filter_max = 0;

namespace {
std::vector<byte> capturedBitmap;
std::vector<byte> capturedPalette;
unsigned uploads = 0;
unsigned unloads = 0;
int capturedType = -1;
void (*afterFixtureCopy)(void*, int) = nullptr;

void Check(bool value, const char* message) {
    if (!value) {
        std::fprintf(stderr, "custom-builder:%s\n", message);
        std::exit(17);
    }
}

int NoSwap(int value) {
    return value;
}

std::vector<byte> Load(const char* path) {
    std::ifstream file(path, std::ios::binary);
    Check(file.good(), "open independently authored fixture");
    std::vector<byte> bytes;
    char value;
    while (file.get(value)) {
        bytes.push_back(static_cast<byte>(value));
    }
    Check(file.eof(), "complete fixture read");
    return bytes;
}

void Save(const std::string& path, const std::vector<byte>& bytes) {
    std::ofstream file(path, std::ios::binary);
    Check(file.good(), "open owned oracle output");
    file.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    Check(file.good(), "complete oracle write");
}
} // namespace

decltype(LittleLong) LittleLong = NoSwap;
void Q_memcpy(void* output, const void* input, int size) {
    std::memcpy(output, input, static_cast<std::size_t>(size));
    if (afterFixtureCopy) {
        afterFixtureCopy(output, size);
    }
}
void Q_strncpy(char* output, const char* input, int size) {
    std::strncpy(output, input, static_cast<std::size_t>(size));
}
void Con_Printf(const char*, ...) {
}
[[noreturn]] void Sys_Error(const char*, ...) {
    std::abort();
}
#if !defined(SW_RENDERER)
void GL_UnloadTexture(char* name) {
    Check(name[0] == '}' && std::strcmp(name + 1, szCustName + 1) == 0, "actual custom unload name");
    ++unloads;
}
int GL_LoadTexture2(char* name, GL_TEXTURETYPE kind, int width, int height, byte* bitmap, qboolean mipmap,
                    int type, byte* palette, int filter) {
    Check(kind == GLT_DECAL && mipmap && filter == gl_filter_max
        && std::strcmp(name + 1, gfCustomBuild ? szCustName + 1 : "rdinary") == 0,
        "real builder renderer arguments");
    ++uploads;
    capturedType = type;
    capturedBitmap.assign(bitmap, bitmap + width * height);
    constexpr unsigned PaletteBytes = 256 * 3;
    capturedPalette.assign(palette, palette + PaletteBytes);
    return 1234;
}
#endif

#include "logo_validation.inc"
#include "builders.inc"

int main(int argc, char** argv) {
    std::printf("texture=%zu,mip=%zu,prefix=%zu,selected=%d\n", sizeof(texture_t), sizeof(miptex_t),
        sizeof(texture_t) - sizeof(miptex_t), static_cast<int>(MIP_EXTRASIZE));
    if (argc == 3 && std::strcmp(argv[1], "--palette-span") == 0) {
        const std::vector<byte> raw = Load(argv[2]);
        wadinfo_t header{};
        std::memcpy(&header, raw.data(), sizeof(header));
        lumpinfo_t lump{};
        std::memcpy(&lump, raw.data() + header.infotableofs, sizeof(lump));
        std::vector<byte> cache(static_cast<std::size_t>(lump.size) + MIP_EXTRASIZE + 1);
        std::memcpy(cache.data() + MIP_EXTRASIZE, raw.data() + lump.filepos, static_cast<std::size_t>(lump.size));
        // This observer overallocates backing storage, so the old validator's reads
        // stay defined. Its logical lump intentionally excludes the renderer palette.
        constexpr int ShortLogicalLump = 40 + 340 + 2;
        lump.size = ShortLogicalLump;
        lump.disksize = ShortLogicalLump;
        cachewad_t wad{};
        wad.cacheExtra = static_cast<int>(MIP_EXTRASIZE);
        wad.name = const_cast<char*>("tempdecal.wad");
        Check(!Draw_ValidateCustomLogo(&wad, cache.data(), &lump), "logical palette span must cover all renderer reads");
        return 0;
    }
    Check(MIP_EXTRASIZE == sizeof(texture_t) - sizeof(miptex_t), "cached pixels must begin after native header");
    if (argc == 1) {
        return 0;
    }
    Check(argc == 3 || argc == 4, "fixture and owned output stem required");
    const bool overlapReference = argc == 4 && std::strcmp(argv[3], "--overlap-reference") == 0;
    const bool ordinary = argc == 4 && std::strcmp(argv[3], "--ordinary") == 0;
    const std::vector<byte> raw = Load(argv[1]);
    Check(raw.size() >= sizeof(wadinfo_t) + sizeof(lumpinfo_t), "complete authored header/table");
    wadinfo_t header{};
    std::memcpy(&header, raw.data(), sizeof(header));
    Check(header.numlumps == 1 && header.infotableofs >= static_cast<int>(sizeof(header)) &&
          static_cast<std::size_t>(header.infotableofs) + sizeof(lumpinfo_t) == raw.size(), "authored table contained");
    lumpinfo_t lump{};
    std::memcpy(&lump, raw.data() + header.infotableofs, sizeof(lump));
    Check(lump.filepos >= static_cast<int>(sizeof(header)) && lump.size == lump.disksize && lump.size > 0 &&
          lump.filepos <= header.infotableofs && lump.size <= header.infotableofs - lump.filepos,
          "authored payload fully contained");
    miptex_t mip{};
    Check(static_cast<std::size_t>(lump.size) >= sizeof(mip), "authored mip header exists");
    std::memcpy(&mip, raw.data() + lump.filepos, sizeof(mip));
    Check(mip.width && mip.height && mip.width <= 256 && mip.height <= 256, "authored bounded dimensions");
    const unsigned pixels = mip.width * mip.height;
    const unsigned sum = pixels + (pixels >> 2) + (pixels >> 4) + (pixels >> 6);
    constexpr unsigned PaletteBytes = 256 * 3;
    const unsigned paletteOffset = mip.offsets[0] + sum + 2;
    Check(paletteOffset + PaletteBytes <= static_cast<unsigned>(lump.size) &&
          sizeof(mip) + sum + 2 <= static_cast<unsigned>(lump.size), "all legacy palette reads physically contained");
    unsigned levelPixels = pixels;
    for (unsigned index = 0; index < 3; ++index) {
        Check(mip.offsets[index] + levelPixels == mip.offsets[index + 1], "legacy contiguous mip offsets");
        levelPixels >>= 2;
    }
    const bool aliasesHeader = mip.offsets[0] < sizeof(mip);
    Check(overlapReference ? aliasesHeader && sizeof(void*) == 4 : !aliasesHeader,
          "only explicit Win32 witness may execute header-alias input");
    std::vector<byte> cache(static_cast<std::size_t>(lump.size) + MIP_EXTRASIZE + 1);
    std::memcpy(cache.data() + MIP_EXTRASIZE, raw.data() + lump.filepos, static_cast<std::size_t>(lump.size));
    const std::vector<byte> before = cache;
    cachewad_t wad{};
    wad.cacheExtra = static_cast<int>(MIP_EXTRASIZE);
    wad.name = const_cast<char*>("tempdecal.wad");
    Check(Draw_ValidateCustomLogo(&wad, cache.data(), &lump), "actual legacy validation accepts authored case");
    for (unsigned index = 0; index < 256; ++index) {
        texgammatable[index] = static_cast<byte>((index * 13 + 7) & 255);
    }
    gfCustomBuild = !ordinary;
    Draw_MiptexTexture(&wad, cache.data());
    gfCustomBuild = false;
    const texture_t* texture = reinterpret_cast<const texture_t*>(cache.data());
    Check(texture->width == mip.width && texture->height == mip.height, "actual native texture dimensions");
    Check(texture->offsets[0] == mip.offsets[0] + MIP_EXTRASIZE, "actual cached first mip offset");
    const byte* bitmap = cache.data() + texture->offsets[0];
    const byte* palette = cache.data() + MIP_EXTRASIZE + paletteOffset;
#if defined(SW_RENDERER)
    capturedBitmap.assign(bitmap, bitmap + pixels);
    capturedPalette.assign(palette, palette + PaletteBytes);
    capturedType = texture->name[0] == '{' ? 1 : 3;
#else
    Check(uploads == 1 && texture->gl_texturenum == 1234, "one real builder upload through typed renderer seam");
    Check(unloads == static_cast<unsigned>(!ordinary && capturedType == TEX_TYPE_ALPHA_GRADIENT),
          "retained unload policy");
    Check(std::equal(capturedBitmap.begin(), capturedBitmap.end(), bitmap) &&
          std::equal(capturedPalette.begin(), capturedPalette.end(), palette), "captured synchronous data matches cache");
#endif
    const bool changedPixels = !std::equal(capturedBitmap.begin(), capturedBitmap.end(),
                                          before.begin() + texture->offsets[0]);
    Check(aliasesHeader ? changedPixels : !changedPixels, "header overlap characterization versus preserved mip bytes");
    if (!aliasesHeader) {
        Check(std::equal(cache.begin() + texture->offsets[0], cache.begin() + texture->offsets[0] + sum,
                         before.begin() + texture->offsets[0]), "all four mip levels remain outside native header writes");
    }
    Save(std::string(argv[2]) + "-bitmap.bin", capturedBitmap);
    Save(std::string(argv[2]) + "-palette.bin", capturedPalette);
    std::printf("builder:width=%u,height=%u,type=%d,header_alias=%d,changed_pixels=%d,uploads=%u,unloads=%u\n",
        mip.width, mip.height, capturedType, aliasesHeader, changedPixels, uploads, unloads);
    return 0;
}
