#include <webp/encode.h>
#include "vgui2/BaseUISurface_Font.h"

#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <climits>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>
#include <utility>

using byte = unsigned char;
using qboolean = int;
constexpr qboolean TRUE = 1;
constexpr qboolean FALSE = 0;
#define LOADFILE_HEAP 5
static std::map<std::string, std::vector<byte>> files;
static std::vector<void*> hunkAllocations;
static int liveFileAllocations = 0;

byte* COM_LoadFile(char* path, int, int* length) {
    const auto found = files.find(path);
    if (found == files.end()) {
        return nullptr;
    }
    *length = static_cast<int>(found->second.size());
    byte* result = static_cast<byte*>(std::malloc(found->second.size()));
    if (!found->second.empty()) {
        std::memcpy(result, found->second.data(), found->second.size());
    }
    ++liveFileAllocations;
    return result;
}

void COM_FreeFile(void* file) {
    --liveFileAllocations;
    std::free(file);
}

void* Hunk_AllocName(int bytes, const char*) {
    void* result = std::malloc(bytes);
    hunkAllocations.push_back(result);
    return result;
}

void Con_Printf(const char*, ...) {}
void Con_DPrintf(const char*, ...) {}
void Sys_Error(const char* message, ...) {
    throw std::runtime_error(message);
}

#if __has_include("engine_image.h")
#include "engine_image.h"
#endif

#ifdef TEST_HARDWARE
qboolean LoadTGA2(char*, byte*, int, int*, int*, qboolean);
#include "hardware.inc"
#else
struct colorVec { unsigned int r; unsigned int g; unsigned int b; unsigned int a; };
static bool is15bit = false;
constexpr unsigned int COLOR_GREENMASK_555 = 0x03e0;
constexpr unsigned int COLOR_REDMASK_555 = 0x7c00;
constexpr unsigned int COLOR_GREENMASK_565 = 0x07e0;
constexpr unsigned int COLOR_REDMASK_565 = 0xf800;
#include "software.inc"
#endif

#include "font.inc"

using FileHandle_t = const std::vector<byte>*;
FileHandle_t FS_Open(const char* path, const char*) {
    const auto found = files.find(path);
    return found == files.end() ? nullptr : &found->second;
}
void VGui_LoadBMP(FileHandle_t, byte*, int, int*, int*) {}
#define _snprintf snprintf
struct TestTexture { int _wide; int _tall; };
static TestTexture surfaceTexture;
static std::vector<byte> surfacePixels;
static bool textureExists = false;
#ifdef TEST_SOFTWARE
using vgui_texture_t = TestTexture;
struct TestTextureStore {
    TestTexture* GetTexture(int) { return textureExists ? &surfaceTexture : nullptr; }
};
struct TestRenderPlatform {
    TestTextureStore* textures;
    TestTexture* current;
};
static TestTextureStore textureStore;
static TestRenderPlatform renderPlatform = {&textureStore, nullptr};
#else
static TestTexture* staticGetTextureById(int) { return textureExists ? &surfaceTexture : nullptr; }
struct TestCvar { float value; };
static TestCvar fs_perf_warnings = {0};
static bool IsPowerOfTwo(int value) { return value > 0 && !(value & (value - 1)); }
#endif
class IEngineSurface {};
class EngineSurface : public IEngineSurface {
public:
#ifdef TEST_SOFTWARE
    TestRenderPlatform* _renderPlat = &renderPlatform;
#endif
    ~EngineSurface() { cse::engine_image::ForgetFileTextures(this); }
    void refreshFileTextures() {}
    void drawSetTextureFile(int id, const char* filename, int hardwareFilter, bool forceReload);
    void drawSetTexture(int) {}
    void drawSetTextureRGBA(int, const char* data, int width, int height, int, int) {
        surfacePixels.assign(data, data + width * height * 4);
        textureExists = true;
    }
};
// This loader suite runs disconnected; connected policy/cache behavior lives in
// tests/native/image_consistency and extracts the production connection lifecycle.
enum { force_exactfile = 0 };
struct consistency_t { char* filename; int issound; int check_type; };
struct TestClientState { int num_consistency; consistency_t consistency_list[1]; };
static TestClientState cl{};
#include "image_policy.inc"
#ifdef TEST_HARDWARE
#include "hardware_surface.inc"
#else
#include "software_surface.inc"
#endif

#ifdef TEST_HARDWARE
using GLuint = unsigned int;
#define GL_TEXTURE_2D 1
#define GL_COLOR_INDEX8_EXT 2
#define GL_COLOR_INDEX 3
#define GL_UNSIGNED_BYTE 4
#define GL_RGBA8 5
#define GL_RGB5_A1 6
#define GL_RGBA 7
#define GL_TEXTURE_MIN_FILTER 8
#define GL_TEXTURE_MAG_FILTER 9
#define GL_LINEAR 10
#define MAX_QPATH 64
#define MAX_PATH 512
#define MAP_IMAGE_SIZE (3 * 1024 * 1024)
static qboolean gLoadSky = TRUE;
static int qglColorTableEXT = 1;
static TestCvar gl_palette_tex = {1};
static TestCvar gl_dither = {0};
static byte texgammatable[256];
static struct { char skyName[32]; } movevars = {"test"};
constexpr int TestSkyFaceDimension = 256;
constexpr int TestSkyFacePixels = TestSkyFaceDimension * TestSkyFaceDimension;
constexpr int TestRgbaChannels = 4;
struct SkyUpload {
    int internalFormat;
    int format;
    int width;
    int height;
    std::vector<byte> pixels;
};
static std::vector<SkyUpload> skyUploads;
static int skyBmpLoadCount = 0;
static short skyPaletteAddResult = 0;
static std::vector<byte> mapPixels;
void* Mem_Malloc(int size) { return std::malloc(size); }
void Mem_Free(void* pointer) { std::free(pointer); }
void qglDeleteTextures(int, const GLuint*) {}
void GL_PaletteClearSky() {}
short GL_PaletteAdd(byte*, qboolean) {
    return skyPaletteAddResult;
}
int GL_GenTexture() { return 1; }
void GL_Bind(int) {}
void qglTexParameterf(int, int, int) {}
void VideoMode_GetCurrentVideoMode(int* width, int* height, int* bpp) {
    *width = 640;
    *height = 480;
    *bpp = 32;
}
void qglTexImage2D(int, int, int internalFormat, int width, int height, int, int format, int, const byte* data) {
    const int channels = format == GL_RGBA ? TestRgbaChannels : 1;
    skyUploads.push_back({internalFormat, format, width, height, {data, data + width * height * channels}});
}
void LoadBMP8(FileHandle_t file, byte** palette, int* count, byte** indexed, int* width, int* height) {
    constexpr int PaletteBytes = 256 * 3;
    ++skyBmpLoadCount;
    *palette = static_cast<byte*>(std::calloc(PaletteBytes, 1));
    *indexed = static_cast<byte*>(std::malloc(TestSkyFacePixels));
    std::memset(*indexed, file->front(), TestSkyFacePixels);
    *count = 1;
    *width = TestSkyFaceDimension;
    *height = TestSkyFaceDimension;
}
#define Q_strcmp strcmp
#define Q_strcpy strcpy
#define Q_strncpy strncpy
#define Q_strlen strlen
#define Q_memset memset
#define Q_memcpy memcpy
static void Q_strlwr(char* path) {
    for (; *path; ++path) {
        if (*path >= 'A' && *path <= 'Z') {
            *path += 'a' - 'A';
        }
    }
}
#include "sky.inc"
struct model_t { bool needload; };
static model_t mapModel;
model_t* Mod_FindName(bool, const char*) { return &mapModel; }
static void LoadMapSpriteHook(const char**) {}
static struct { void (*LoadMapSprite)(const char**); } g_engdstAddrs = {LoadMapSpriteHook};
void R_LoadOverviewSpriteModel(model_t*, byte* data, int horizontal, int vertical) {
    mapPixels.assign(data, data + horizontal * vertical * 128 * 128 * 4);
}
#include "map.inc"
#endif

#ifdef TEST_SOFTWARE
using pixel_t = byte;
struct mvertex_t { float position[3]; };
struct mplane_t { float normal[3]; float dist; byte type; byte signbits; byte pad[2]; };
struct texture_t { unsigned int width; unsigned int height; };
struct mtexinfo_t { float vecs[2][4]; float mipadjust; texture_t* texture; int flags; };
struct color24 { byte r; byte g; byte b; };
struct msurface_t {
    int texturemins[2];
    int extents[2];
    mtexinfo_t* texinfo;
    color24* samples;
    mplane_t* plane;
    int flags;
};
struct bedge_t { mvertex_t* v[2]; bedge_t* pnext; };
static const char* suf[6] = {"rt", "bk", "lf", "ft", "up", "dn"};
static pixel_t* gSkies[6];
static pixel_t* gFilteredSkies[6];
static int skytexorder[6] = {0, 2, 1, 3, 4, 5};
static int gFakePlaneType[6] = {1, -1, -2, 2, 3, -3};
static msurface_t gFakeSurface[6];
static texture_t gFakeTex;
static mtexinfo_t gFakeTexInfo[6];
static mplane_t gFakePlanes[6];
static bedge_t gFakeEdges[4];
static mvertex_t gFakeVerts[4];
static int gLoadSky = 1;
static struct { char skyName[32]; } movevars = {"test"};
static bool filterMode = true;
static float filterColorRed = 1.0f;
static float filterColorGreen = 1.0f;
static float filterColorBlue = 1.0f;
static float filterBrightness = 1.0f;
constexpr int SURF_DRAWSKY = 4;
constexpr int MAX_QPATH = 64;
#define Q_strcmp strcmp
#define Q_memset memset
#define VectorClear(vector) std::memset((vector), 0, 3 * sizeof(float))
void Cvar_Set(const char*, const char* value) {
    std::snprintf(movevars.skyName, sizeof(movevars.skyName), "%s", value);
}
#include "software_sky.inc"
#endif

static int failures = 0;
static void Check(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", message);
        ++failures;
    }
}

static const std::vector<byte> expected = {
    255, 0, 0, 255, 0, 255, 0, 128,
    0, 0, 255, 255, 255, 255, 255, 255
};

static std::vector<byte> WebPFixture(const std::vector<byte>& pixels = expected, int width = 2, int height = 2) {
    byte* encoded = nullptr;
    const std::size_t size = WebPEncodeLosslessRGBA(pixels.data(), width, height, width * 4, &encoded);
    std::vector<byte> result(encoded, encoded + size);
    WebPFree(encoded);
    return result;
}

static std::vector<byte> TargaFixture(int width = 2, int height = 2) {
    // A bottom-up 32-bit 2x2 TGA, yellow at every pixel, distinct from the replacement.
    std::vector<byte> result = {0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0, 2, 0, 32, 8};
    result[12] = static_cast<byte>(width);
    result[13] = static_cast<byte>(width >> 8);
    result[14] = static_cast<byte>(height);
    result[15] = static_cast<byte>(height >> 8);
    for (int pixel = 0; pixel < width * height; ++pixel) {
        result.insert(result.end(), {0, 255, 255, 255});
    }
    return result;
}

static bool Load(const char* path, std::vector<byte>& output, int& width, int& height) {
    return LoadTGA(const_cast<char*>(path), output.data(), static_cast<int>(output.size()), &width, &height) != FALSE;
}

// These cases exercise the extracted production loaders, including their filesystem ownership.
static void TestLegacyTargaBounds() {
    constexpr std::size_t HeaderBytes = 18;
    constexpr std::size_t GuardBytes = 16;
    constexpr byte GuardValue = 0xa5;
    constexpr int TestWidth = 3;
    constexpr int TestHeight = 2;
    constexpr int PixelCount = TestWidth * TestHeight;
    constexpr int RawImageType = 2;
    constexpr int RleImageType = 10;
    constexpr int RgbBits = 24;
    constexpr int RgbaBits = 32;
    constexpr int BitsPerByte = 8;
    constexpr int UnknownDimension = -1;
    constexpr std::size_t ImageIdBytes = 3;
    constexpr int RgbaBytes = PixelCount * cse::image::RgbaChannels;
#ifdef TEST_SOFTWARE
    constexpr int PackedBytes = PixelCount * sizeof(unsigned short);
#endif
    const char* path = "bounded-legacy.tga";
    const auto header = [](int imageType, int pixelBits, int width = TestWidth, int height = TestHeight) {
        std::vector<byte> result(HeaderBytes, 0);
        result[2] = static_cast<byte>(imageType);
        result[12] = static_cast<byte>(width);
        result[13] = static_cast<byte>(width >> BitsPerByte);
        result[14] = static_cast<byte>(height);
        result[15] = static_cast<byte>(height >> BitsPerByte);
        result[16] = static_cast<byte>(pixelBits);
        return result;
    };
    const auto checkCase = [&](const std::vector<byte>& fixture, bool succeeds, int capacity,
                               const std::vector<byte>& pixels, const char* label, bool packed = false,
                               bool fatal = false) {
        files[path] = fixture;
        std::vector<byte> guarded(GuardBytes + capacity + GuardBytes, GuardValue);
        byte* output = guarded.data() + GuardBytes;
        int width = UnknownDimension;
        int height = UnknownDimension;
        const int allocationsBefore = liveFileAllocations;
        bool loaded = false;
        bool threw = false;
        try {
#ifdef TEST_HARDWARE
            (void)packed;
            loaded = LoadTGA2(const_cast<char*>(path), output, capacity, &width, &height, fatal) != FALSE;
#else
            (void)fatal;
            loaded = R_LoadTGA(const_cast<char*>(path), output, capacity, &width, &height, !packed) != nullptr;
#endif
        } catch (const std::runtime_error&) {
            threw = true;
        }
        const std::string description(label);
#ifdef TEST_HARDWARE
        Check(fatal && !succeeds ? threw : !threw && loaded == succeeds,
              (description + ": preserves the requested failure behavior").c_str());
#else
        Check(!threw && loaded == succeeds, (description + ": returns the expected load result").c_str());
#endif
        Check(liveFileAllocations == allocationsBefore, (description + ": releases the source file").c_str());
        Check(std::all_of(guarded.begin(), guarded.begin() + GuardBytes,
                          [](byte value) { return value == GuardValue; })
                  && std::all_of(guarded.end() - GuardBytes, guarded.end(),
                                 [](byte value) { return value == GuardValue; }),
              (description + ": preserves output canaries").c_str());
        if (succeeds) {
            Check(width == TestWidth && height == TestHeight,
                  (description + ": preserves dimensions").c_str());
            Check(std::equal(pixels.begin(), pixels.end(), output),
                  (description + ": preserves decoded pixel bytes").c_str());
            Check(std::all_of(output + pixels.size(), output + capacity,
                              [](byte value) { return value == GuardValue; }),
                  (description + ": preserves unused output capacity").c_str());
        }
    };

    for (std::size_t length = 0; length < HeaderBytes; ++length) {
        checkCase(std::vector<byte>(length, 0), false, RgbaBytes, {}, "truncated TGA header");
    }
    std::vector<byte> truncatedId = header(RawImageType, RgbBits);
    truncatedId[0] = 255;
    truncatedId.resize(HeaderBytes + 2, 0);
    checkCase(truncatedId, false, RgbaBytes, {}, "TGA image ID exceeds the file");
    for (std::vector<byte> invalid : {header(RawImageType, RgbBits, 0, TestHeight),
                                     header(RawImageType, RgbBits, TestWidth, 0)}) {
        invalid.resize(HeaderBytes + 2, 0);
        checkCase(invalid, false, RgbaBytes, {}, "TGA zero dimension");
    }

    // File order is bottom-up: blue, white, white / white, white, red.
    const std::vector<byte> rgba = {
        255, 255, 255, 73, 255, 255, 255, 73, 255, 0, 0, 19,
        0, 0, 255, 41, 255, 255, 255, 73, 255, 255, 255, 73
    };
    std::vector<byte> opaqueRgba = rgba;
    for (std::size_t offset = 3; offset < opaqueRgba.size(); offset += cse::image::RgbaChannels) {
        opaqueRgba[offset] = 255;
    }
#ifdef TEST_SOFTWARE
    const std::vector<byte> packed565 = {255, 255, 255, 255, 0, 248, 31, 0, 255, 255, 255, 255};
    const std::vector<byte> packed555 = {255, 127, 255, 127, 0, 124, 31, 0, 255, 127, 255, 127};
    is15bit = false;
#endif
    for (int pixelBits : {RgbBits, RgbaBits}) {
        std::vector<byte> raw = header(RawImageType, pixelBits);
        // Include an image ID so successful ID skipping is covered as well.
        raw[0] = ImageIdBytes;
        raw.insert(raw.end(), {7, 11, 13});
        for (const std::vector<byte>& color : {std::vector<byte>{255, 0, 0, 41},
                                              std::vector<byte>{255, 255, 255, 73},
                                              std::vector<byte>{255, 255, 255, 73},
                                              std::vector<byte>{255, 255, 255, 73},
                                              std::vector<byte>{255, 255, 255, 73},
                                              std::vector<byte>{0, 0, 255, 19}}) {
            raw.insert(raw.end(), color.begin(), color.begin() + pixelBits / BitsPerByte);
        }
        const std::vector<byte>& rawExpected = pixelBits == RgbaBits ? rgba : opaqueRgba;
        checkCase(raw, true, RgbaBytes, rawExpected, "valid raw TGA");
        checkCase(raw, false, RgbaBytes - 1, {}, "raw TGA output is one byte short");
#ifdef TEST_HARDWARE
        checkCase(raw, false, RgbaBytes - 1, {}, "fatal insufficient output frees its file", false, true);
#endif
#ifdef TEST_SOFTWARE
        checkCase(raw, true, PackedBytes, packed565, "raw TGA packed RGB565", true);
        checkCase(raw, false, PackedBytes - 1, {}, "raw packed TGA output is one byte short", true);
        is15bit = true;
        checkCase(raw, true, PackedBytes, packed555, "raw TGA packed RGB555", true);
        is15bit = false;
#endif
        for (std::size_t length = HeaderBytes + ImageIdBytes; length < raw.size(); ++length) {
            checkCase(std::vector<byte>(raw.begin(), raw.begin() + length), false, RgbaBytes, {},
                      "truncated raw TGA pixels");
        }
        std::vector<byte> rle = header(RleImageType, pixelBits);
        rle.push_back(0); // One literal blue pixel.
        const std::vector<byte> blue = {255, 0, 0, 41};
        rle.insert(rle.end(), blue.begin(), blue.begin() + pixelBits / BitsPerByte);
        rle.push_back(0x83); // Four white pixels, crossing the row boundary.
        const std::vector<byte> white = {255, 255, 255, 73};
        rle.insert(rle.end(), white.begin(), white.begin() + pixelBits / BitsPerByte);
        rle.push_back(0); // One literal red pixel.
        const std::vector<byte> red = {0, 0, 255, 19};
        rle.insert(rle.end(), red.begin(), red.begin() + pixelBits / BitsPerByte);
#ifdef TEST_HARDWARE
        checkCase(rle, true, RgbaBytes, rawExpected, "valid cross-row RLE TGA");
#else
        checkCase(rle, true, RgbaBytes, packed565, "RLE TGA retains historical packed public output");
        checkCase(rle, true, PackedBytes, packed565, "RLE public output accepts its exact packed capacity");
        checkCase(rle, false, PackedBytes - 1, {}, "RLE public output is one byte short");
        checkCase(rle, true, PackedBytes, packed565, "RLE TGA packed RGB565", true);
        checkCase(rle, false, PackedBytes - 1, {}, "RLE packed TGA output is one byte short", true);
        is15bit = true;
        checkCase(rle, true, PackedBytes, packed555, "RLE TGA packed RGB555", true);
        is15bit = false;
#endif
        // Also exercise a literal packet crossing the row boundary.
        std::vector<byte> literal = header(RleImageType, pixelBits);
        literal.push_back(PixelCount - 1);
        literal.insert(literal.end(), raw.begin() + HeaderBytes + ImageIdBytes, raw.end());
#ifdef TEST_HARDWARE
        checkCase(literal, true, RgbaBytes, rawExpected, "valid cross-row literal TGA packet");
#else
        checkCase(literal, true, PackedBytes, packed565, "valid cross-row literal packed TGA packet", true);
#endif
        // Legacy decoding stops at the final output pixel even for a larger declared packet count.
        std::vector<byte> oversizedFinalPacket = rle;
        oversizedFinalPacket[oversizedFinalPacket.size() - pixelBits / BitsPerByte - 1] = 0x7f;
#ifdef TEST_HARDWARE
        checkCase(oversizedFinalPacket, true, RgbaBytes, rawExpected, "oversized final literal packet count");
#else
        checkCase(oversizedFinalPacket, true, PackedBytes, packed565, "oversized final literal packet count", true);
        files[path] = rle;
        const std::size_t hunkCount = hunkAllocations.size();
        byte* allocated = R_LoadTGA(const_cast<char*>(path), nullptr, 0, nullptr, nullptr, TRUE);
        Check(allocated && std::memcmp(allocated, packed565.data(), packed565.size()) == 0
                  && hunkAllocations.size() == hunkCount + 1,
              "software RLE hunk allocation preserves packed output");
#endif
        for (const std::vector<byte>* encoded : {&rle, &literal}) {
            for (std::size_t length = HeaderBytes; length < encoded->size(); ++length) {
                checkCase(std::vector<byte>(encoded->begin(), encoded->begin() + length), false,
                          RgbaBytes, {}, "truncated RLE TGA packet header or pixel");
#ifdef TEST_SOFTWARE
                files[path] = std::vector<byte>(encoded->begin(), encoded->begin() + length);
                const std::size_t hunkCount = hunkAllocations.size();
                const int fileCount = liveFileAllocations;
                Check(!R_LoadTGA(const_cast<char*>(path), nullptr, 0, nullptr, nullptr, FALSE)
                          && hunkAllocations.size() == hunkCount && liveFileAllocations == fileCount,
                      "truncated RLE stream is rejected and released before hunk allocation");
#endif
            }
        }
    }
    // These dimensions overflow the historical signed pixel/output calculations.
    for (int height : {32768, 65535}) {
        std::vector<byte> oversized = header(RawImageType, RgbaBits, 65535, height);
        oversized.resize(HeaderBytes + 2, 0);
        checkCase(oversized, false, RgbaBytes, {}, "oversized TGA dimensions");
    }
#ifdef TEST_HARDWARE
    for (const std::vector<byte>& malformed : {
             std::vector<byte>(HeaderBytes - 1, 0), truncatedId, header(1, RgbBits),
             header(RawImageType, 16), header(RleImageType, RgbaBits)}) {
        checkCase(malformed, false, RgbaBytes, {}, "fatal TGA failure frees its file", false, true);
    }
#endif
    files.erase(path);
}

int main() {
    TestLegacyTargaBounds();
    const std::vector<byte> webp = WebPFixture();
    files["legacy.webp"] = webp;
    files["legacy.tga"] = TargaFixture();
    files["explicit.WEBP"] = webp;
    files["payload.tga"] = webp;
    files["invalid.webp"] = {1, 2, 3, 4};
    files["invalid.tga"] = TargaFixture();
    files["fallback.tga"] = TargaFixture();
    files["raw"] = TargaFixture();
    std::vector<byte> output(expected.size(), 0);
    int width = -1;
    int height = -1;
    Check(Load("legacy.tga", output, width, height) && output == expected && width == 2 && height == 2,
          "legacy .tga selects same-base WebP with top-down RGBA and alpha");
    std::fill(output.begin(), output.end(), 0);
    Check(Load("legacy", output, width, height) && output == expected,
          "extensionless path selects same-base WebP");
    std::fill(output.begin(), output.end(), 0);
    Check(Load("explicit.WEBP", output, width, height) && output == expected,
          "explicit case-insensitive WebP extension decodes");
    std::fill(output.begin(), output.end(), 0);
    Check(Load("payload.tga", output, width, height) && output == expected,
          "WebP bytes carried in a legacy .tga resource decode without renaming");
    std::fill(output.begin(), output.end(), 0);
    Check(Load("invalid.tga", output, width, height) && output[0] == 255 && output[1] == 255 && output[2] == 0,
          "invalid replacement falls back to the original TGA");
    Check(Load("fallback", output, width, height) && output[0] == 255 && output[1] == 255 && output[2] == 0,
          "extensionless path falls back to the .tga file");
    Check(Load("raw", output, width, height) && output[0] == 255 && output[1] == 255 && output[2] == 0,
          "existing extensionless TGA files remain loadable");
    std::vector<byte> tooSmall(8, 0xa5);
    Check(!Load("explicit.WEBP", tooSmall, width, height)
              && std::all_of(tooSmall.begin(), tooSmall.end(), [](byte value) { return value == 0xa5; }),
          "WebP output capacity rejects the image without modifying pixels");
    Check(!Load("invalid.webp", output, width, height), "explicit invalid WebP is rejected");
#ifdef TEST_SOFTWARE
    byte* sky = R_LoadTGA(const_cast<char*>("legacy.tga"), nullptr, 0, &width, &height, FALSE);
    const unsigned short expected565[] = {0xf800, 0x07e0, 0x001f, 0xffff};
    Check(sky && std::memcmp(sky, expected565, sizeof(expected565)) == 0 && width == 2 && height == 2,
          "software skybox converts top-down WebP through RGB565 dithering");
    std::vector<byte> packedOutput(sizeof(expected565), 0);
    sky = R_LoadTGA(const_cast<char*>("payload.tga"), packedOutput.data(),
                    static_cast<int>(packedOutput.size()), &width, &height, FALSE);
    Check(sky == packedOutput.data() && std::memcmp(sky, expected565, sizeof(expected565)) == 0,
          "software legacy WebP payload respects a caller-owned 16-bit output buffer");
    is15bit = true;
    sky = R_LoadTGA(const_cast<char*>("legacy.tga"), nullptr, 0, &width, &height, FALSE);
    const unsigned short expected555[] = {0x7c00, 0x03e0, 0x001f, 0x7fff};
    Check(sky && std::memcmp(sky, expected555, sizeof(expected555)) == 0,
          "software skybox preserves the RGB555 mode");
#endif
    FileImage fontImage = {};
    FileImageStream_Memory fontStream(const_cast<byte*>(webp.data()), static_cast<int>(webp.size()));
    const std::vector<byte> expectedBgra = {
        0, 0, 255, 255, 0, 255, 0, 128,
        255, 0, 0, 255, 255, 255, 255, 255
    };
    Check(Load32BitTGA(&fontStream, &fontImage) && fontImage.m_Width == 2 && fontImage.m_Height == 2
              && fontImage.m_pData && std::memcmp(fontImage.m_pData, expectedBgra.data(), expectedBgra.size()) == 0,
          "engine font image stream accepts WebP and retains top-down BGRA pixels");
    fontImage.Term();
    std::vector<byte> rleFont = TargaFixture();
    rleFont.resize(18);
    rleFont[2] = 10;
    rleFont.insert(rleFont.end(), {0x81, 0, 255, 255, 255, 0x81, 0, 255, 255, 255});
    FileImageStream_Memory targaFontStream(rleFont.data(), static_cast<int>(rleFont.size()));
    Check(Load32BitTGA(&targaFontStream, &fontImage) && fontImage.m_Width == 2 && fontImage.m_Height == 2
              && fontImage.m_pData[0] == 0 && fontImage.m_pData[1] == 255 && fontImage.m_pData[2] == 255,
          "existing RLE TGA font streams retain BGRA decoding");
    fontImage.Term();
    std::vector<byte> hugeRiff = webp;
    std::fill(hugeRiff.begin() + 4, hugeRiff.begin() + 8, 0xff);
    FileImageStream_Memory oversizedFont(hugeRiff.data(), static_cast<int>(hugeRiff.size()));
    Check(!Load32BitTGA(&oversizedFont, &fontImage) && fontImage.m_pData == nullptr,
          "oversized RIFF font length is rejected before reading or allocating its payload");
    FileImageStream_Memory truncatedFont(const_cast<byte*>(webp.data()), static_cast<int>(webp.size() - 1));
    Check(!Load32BitTGA(&truncatedFont, &fontImage) && fontImage.m_pData == nullptr,
          "truncated WebP font stream is rejected without retaining pixels");
    std::vector<byte> atlas(256 * 4, 0);
    atlas[65 * 4] = 255;
    atlas[65 * 4 + 1] = 255;
    atlas[65 * 4 + 2] = 255;
    atlas[65 * 4 + 3] = 255;
    std::vector<byte> atlasEncoded = WebPFixture(atlas, 256, 1);
    FileImageStream_Memory atlasStream(atlasEncoded.data(), static_cast<int>(atlasEncoded.size()));
    VFontData fontData;
    Check(LoadVFontDataFrom32BitTGA(&atlasStream, &fontData) && fontData.m_BitmapCharWidth == 1
              && fontData.m_BitmapCharHeight == 1 && fontData.m_pBitmap[65] == 1 && fontData.m_pBitmap[64] == 0,
          "engine WebP font atlas preserves glyph coverage extraction");
    EngineSurface surface;
    surface.drawSetTextureFile(1, "legacy.tga", 0, true);
    Check(surfacePixels == expected, "surface accepts a complete legacy .tga path and its WebP replacement");
    surfacePixels.clear();
    surface.drawSetTextureFile(1, "explicit.WEBP", 0, true);
    Check(surfacePixels == expected, "surface accepts an explicit WebP path without appending .tga");
#ifdef TEST_HARDWARE
    std::vector<byte> skyPixels(TestSkyFacePixels * TestRgbaChannels, 0);
    std::vector<byte> targaSkyPixels(TestSkyFacePixels * TestRgbaChannels, 0);
    for (int pixel = 0; pixel < TestSkyFacePixels; ++pixel) {
        skyPixels[pixel * TestRgbaChannels] = 255;
        skyPixels[pixel * TestRgbaChannels + 3] = 255;
        targaSkyPixels[pixel * TestRgbaChannels] = 255;
        targaSkyPixels[pixel * TestRgbaChannels + 1] = 255;
        targaSkyPixels[pixel * TestRgbaChannels + 3] = 255;
    }
    const std::vector<byte> skyWebP = WebPFixture(skyPixels, TestSkyFaceDimension, TestSkyFaceDimension);
    const char* faces[] = {"rt", "bk", "lf", "ft", "up", "dn"};
    constexpr std::size_t SkyFaceCount = sizeof(faces) / sizeof(faces[0]);
    const int rgbaFormats[] = {GL_RGBA, GL_RGBA, GL_RGBA, GL_RGBA, GL_RGBA, GL_RGBA};
    const int indexedFormats[] = {
        GL_COLOR_INDEX, GL_COLOR_INDEX, GL_COLOR_INDEX, GL_COLOR_INDEX, GL_COLOR_INDEX, GL_COLOR_INDEX
    };
    auto resetHardwareSky = [&]() {
        for (const char* face : faces) {
            for (const char* extension : {".webp", ".bmp", ".tga"}) {
                files.erase(std::string("gfx/env/test") + face + extension);
            }
        }
        skyUploads.clear();
        skyBmpLoadCount = 0;
        skyPaletteAddResult = 0;
        qglColorTableEXT = 1;
        gl_palette_tex.value = 1;
        std::snprintf(movevars.skyName, sizeof(movevars.skyName), "%s", "test");
        gLoadSky = TRUE;
    };
    auto checkHardwareSky = [&](const int* formats, const std::vector<std::vector<byte>>& pixels, const char* label) {
        const std::string description(label);
        Check(skyUploads.size() == SkyFaceCount,
              (description + ": uploads all six faces (got " + std::to_string(skyUploads.size()) + ")").c_str());
        for (std::size_t face = 0; face < std::min(skyUploads.size(), SkyFaceCount); ++face) {
            const SkyUpload& upload = skyUploads[face];
            const std::string faceDescription = description + ": " + faces[face];
            const int internalFormat = formats[face] == GL_RGBA ? GL_RGBA8 : GL_COLOR_INDEX8_EXT;
            Check(upload.format == formats[face] && upload.internalFormat == internalFormat,
                  (faceDescription + " uses the expected upload formats").c_str());
            Check(upload.width == TestSkyFaceDimension && upload.height == TestSkyFaceDimension,
                  (faceDescription + " uploads a complete 256x256 face").c_str());
            Check(upload.pixels == pixels[face], (faceDescription + " preserves every face byte").c_str());
        }
    };
    resetHardwareSky();
    for (const char* face : faces) {
        files[std::string("gfx/env/test") + face + ".webp"] = skyWebP;
        files[std::string("gfx/env/test") + face + ".bmp"] = {1};
    }
    R_LoadSkys();
    checkHardwareSky(rgbaFormats, std::vector<std::vector<byte>>(SkyFaceCount, skyPixels),
                     "hardware sky WebP replaces same-base paletted BMPs");
    skyUploads.clear();
    for (const char* face : faces) {
        files[std::string("gfx/env/test") + face + ".webp"] = webp;
    }
    gLoadSky = TRUE;
    R_LoadSkys();
    checkHardwareSky(indexedFormats,
                     std::vector<std::vector<byte>>(SkyFaceCount, std::vector<byte>(TestSkyFacePixels, 1)),
                     "wrong-sized WebP sky replacement retains valid BMP fallback");
    skyUploads.clear();
    for (const char* face : faces) {
        files.erase(std::string("gfx/env/test") + face + ".bmp");
        files[std::string("gfx/env/test") + face + ".tga"] =
            TargaFixture(TestSkyFaceDimension, TestSkyFaceDimension);
    }
    gLoadSky = TRUE;
    R_LoadSkys();
    checkHardwareSky(rgbaFormats, std::vector<std::vector<byte>>(SkyFaceCount, targaSkyPixels),
                     "wrong-sized WebP sky replacement retains valid TGA fallback");
    struct MixedSkyCase {
        const char* label;
        std::size_t webpFace;
        int formats[SkyFaceCount];
    };
    const MixedSkyCase mixedSkyCases[] = {
        {"WebP first, six BMP originals", 0,
         {GL_RGBA, GL_COLOR_INDEX, GL_COLOR_INDEX, GL_COLOR_INDEX, GL_COLOR_INDEX, GL_COLOR_INDEX}},
        {"WebP middle, six BMP originals", 2,
         {GL_COLOR_INDEX, GL_COLOR_INDEX, GL_RGBA, GL_COLOR_INDEX, GL_COLOR_INDEX, GL_COLOR_INDEX}},
        {"WebP last, six BMP originals", 5,
         {GL_COLOR_INDEX, GL_COLOR_INDEX, GL_COLOR_INDEX, GL_COLOR_INDEX, GL_COLOR_INDEX, GL_RGBA}}
    };
    const byte bmpFaceIndices[] = {11, 29, 53, 97, 151, 233};
    for (const MixedSkyCase& test : mixedSkyCases) {
        resetHardwareSky();
        std::vector<std::vector<byte>> expectedSky;
        for (std::size_t face = 0; face < SkyFaceCount; ++face) {
            files[std::string("gfx/env/test") + faces[face] + ".bmp"] = {bmpFaceIndices[face]};
            expectedSky.push_back(std::vector<byte>(TestSkyFacePixels, bmpFaceIndices[face]));
        }
        files[std::string("gfx/env/test") + faces[test.webpFace] + ".webp"] = skyWebP;
        expectedSky[test.webpFace] = skyPixels;
        R_LoadSkys();
        checkHardwareSky(test.formats, expectedSky, test.label);
        Check(skyBmpLoadCount == static_cast<int>(SkyFaceCount) - 1,
              (std::string(test.label) + ": loads the other five BMP faces (got "
               + std::to_string(skyBmpLoadCount) + ")").c_str());
    }
    const int disabledPaletteCapabilities[][2] = {{0, 1}, {1, 0}};
    for (const auto& capability : disabledPaletteCapabilities) {
        resetHardwareSky();
        qglColorTableEXT = capability[0];
        gl_palette_tex.value = static_cast<float>(capability[1]);
        std::vector<std::vector<byte>> expectedSky(SkyFaceCount, targaSkyPixels);
        for (const char* face : faces) {
            files[std::string("gfx/env/test") + face + ".bmp"] = {1};
            files[std::string("gfx/env/test") + face + ".tga"] =
                TargaFixture(TestSkyFaceDimension, TestSkyFaceDimension);
        }
        files["gfx/env/testlf.webp"] = skyWebP;
        expectedSky[2] = skyPixels;
        R_LoadSkys();
        checkHardwareSky(rgbaFormats, expectedSky, "disabled palette capability retains WebP/TGA selection");
        Check(skyBmpLoadCount == 0, "disabled palette capability never loads a BMP sky face");
    }
    resetHardwareSky();
    for (const char* face : faces) {
        files[std::string("gfx/env/test") + face + ".bmp"] = {1};
        files[std::string("gfx/env/test") + face + ".tga"] =
            TargaFixture(TestSkyFaceDimension, TestSkyFaceDimension);
    }
    constexpr short InvalidPaletteIndex = -1;
    skyPaletteAddResult = InvalidPaletteIndex;
    R_LoadSkys();
    checkHardwareSky(rgbaFormats, std::vector<std::vector<byte>>(SkyFaceCount, targaSkyPixels),
                     "failed BMP palette keeps TGA fallback for remaining sky faces");
    Check(skyBmpLoadCount == 1, "failed BMP palette disables later BMP sky loads");
    resetHardwareSky();
    for (const char* face : faces) {
        files[std::string("gfx/env/test") + face + ".bmp"] = {1};
        files[std::string("gfx/env/test") + face + ".tga"] =
            TargaFixture(TestSkyFaceDimension, TestSkyFaceDimension);
    }
    files.erase("gfx/env/testrt.bmp");
    R_LoadSkys();
    checkHardwareSky(rgbaFormats, std::vector<std::vector<byte>>(SkyFaceCount, targaSkyPixels),
                     "missing first BMP retains TGA fallback for remaining sky faces");
    Check(skyBmpLoadCount == 0, "missing first BMP retains the existing skip of later BMP sky loads");
    std::vector<byte> overview(128 * 128 * 4, 0);
    for (int pixel = 0; pixel < 128 * 128; ++pixel) {
        overview[pixel * 4] = 255;
        overview[pixel * 4 + 3] = 255;
    }
    files["map.webp"] = WebPFixture(overview, 128, 128);
    try {
        Check(R_LoadMapSprite("map.webp") && mapPixels == overview,
              "map sprite accepts an explicit WebP image and preserves tile pixels");
    } catch (const std::runtime_error&) {
        Check(false, "map sprite accepts an explicit WebP image and preserves tile pixels");
    }
#endif
#ifdef TEST_SOFTWARE
    for (int fifteenBit = 0; fifteenBit <= 1; ++fifteenBit) {
        is15bit = fifteenBit != 0;
        for (unsigned int packed = 0; packed <= USHRT_MAX; ++packed) {
            const unsigned short stored = static_cast<unsigned short>(packed);
            short signedPixel;
            std::memcpy(&signedPixel, &stored, sizeof(signedPixel));
            colorVec color{};
            GetRGB(signedPixel, &color);
            const unsigned int red = fifteenBit ? packed / 1024 % 32 * 8 : packed / 2048 % 32 * 8;
            const unsigned int green = fifteenBit ? packed / 32 % 32 * 8 : packed / 32 % 64 * 4;
            Check(color.r == red && color.g == green && color.b == packed % 32 * 8,
                  "all packed RGB555/RGB565 values preserve the renderer's channel expansion");
        }
    }
    is15bit = false;
    std::vector<byte> originalSky = TargaFixture(256, 256);
    for (std::size_t offset = 18; offset < originalSky.size(); offset += 4) {
        originalSky[offset] = 255;
        originalSky[offset + 1] = 0;
        originalSky[offset + 2] = 0;
    }
    const char* softwareFaces[] = {"rt", "bk", "lf", "ft", "up", "dn"};
    for (const char* face : softwareFaces) {
        files[std::string("gfx/env/test") + face + ".webp"] = webp;
        files[std::string("gfx/env/test") + face + ".tga"] = originalSky;
    }
    R_LoadSkys();
    Check(gSkies[0] && reinterpret_cast<const unsigned short*>(gSkies[0])[0] == 0x001f,
          "software sky consumer ignores a tiny WebP sidecar and loads the valid 256x256 TGA");
    R_FilterSkyBox();
    Check(gFakeSurface[0].samples && reinterpret_cast<const unsigned short*>(gFakeSurface[0].samples)[65535] == 0x001f,
          "software sky filtering reads the complete validated 256x256 face");
    const auto reloadSoftwareSky = []() {
        std::snprintf(movevars.skyName, sizeof(movevars.skyName), "%s", "test");
        gLoadSky = TRUE;
        R_LoadSkys();
    };
    const auto checkBlueSoftwareSky = [](const char* message) {
        constexpr std::size_t SkyPixelCount = 256 * 256;
        for (pixel_t* face : gSkies) {
            Check(face && reinterpret_cast<const unsigned short*>(face)[0] == 0x001f, message);
        }
        R_FilterSkyBox();
        for (const msurface_t& surface : gFakeSurface) {
            Check(surface.samples && reinterpret_cast<const unsigned short*>(surface.samples)[SkyPixelCount - 1]
                      == 0x001f, message);
        }
    };
    std::vector<byte> nonsquarePixels(128 * 512 * 4, 0);
    for (std::size_t offset = 0; offset < nonsquarePixels.size(); offset += 4) {
        nonsquarePixels[offset + 1] = 255;
        nonsquarePixels[offset + 3] = 255;
    }
    const std::vector<byte> nonsquareWebP = WebPFixture(nonsquarePixels, 128, 512);
    for (const char* face : softwareFaces) {
        files[std::string("gfx/env/test") + face + ".webp"] = nonsquareWebP;
    }
    reloadSoftwareSky();
    checkBlueSoftwareSky("software sky rejects a nonsquare WebP with the same pixel count and uses the TGA");
    std::vector<byte> bluePixels(256 * 256 * 4, 0);
    for (std::size_t offset = 0; offset < bluePixels.size(); offset += 4) {
        bluePixels[offset + 2] = 255;
        bluePixels[offset + 3] = 255;
    }
    const std::vector<byte> validSkyWebP = WebPFixture(bluePixels, 256, 256);
    for (const char* face : softwareFaces) {
        files[std::string("gfx/env/test") + face + ".webp"] = validSkyWebP;
        files[std::string("gfx/env/test") + face + ".tga"] = TargaFixture(256, 256);
    }
    reloadSoftwareSky();
    checkBlueSoftwareSky("software sky uses valid 256x256 WebP pixels before original TGA pixels");
    std::vector<byte> redSkyPixels(256 * 256 * 4, 0);
    for (std::size_t offset = 0; offset < redSkyPixels.size(); offset += 4) {
        redSkyPixels[offset] = 255;
        redSkyPixels[offset + 3] = 255;
    }
    const std::vector<byte> redSkyWebP = WebPFixture(redSkyPixels, 256, 256);
    for (const char* face : softwareFaces) {
        files[std::string("gfx/env/test") + face + ".webp"] = redSkyWebP;
    }
    reloadSoftwareSky();
    R_FilterSkyBox();
    for (const msurface_t& surface : gFakeSurface) {
        Check(surface.samples && reinterpret_cast<const unsigned short*>(surface.samples)[65535] == 0xf800,
              "software WebP sky filtering preserves red pixels with the packed sign bit set");
    }
    for (const char* face : softwareFaces) {
        files.erase(std::string("gfx/env/test") + face + ".webp");
        files[std::string("gfx/env/test") + face + ".tga"] = TargaFixture();
        files[std::string("gfx/env/desert") + face + ".tga"] = originalSky;
    }
    reloadSoftwareSky();
    Check(std::strcmp(movevars.skyName, "desert") == 0,
          "software sky rejects tiny original TGA faces and selects the default sky");
    checkBlueSoftwareSky("default sky replaces invalid original TGA geometry with complete valid faces");
    for (const char* face : softwareFaces) {
        files[std::string("gfx/env/test") + face + ".tga"] = webp;
    }
    reloadSoftwareSky();
    Check(std::strcmp(movevars.skyName, "desert") == 0,
          "software sky rejects tiny WebP payloads carried under legacy TGA names");
    checkBlueSoftwareSky("default sky replaces invalid WebP payload geometry with complete valid faces");
    for (const char* face : softwareFaces) {
        files[std::string("gfx/env/test") + face + ".webp"] = webp;
        files[std::string("gfx/env/test") + face + ".tga"] = validSkyWebP;
    }
    reloadSoftwareSky();
    checkBlueSoftwareSky("software sky falls back to a valid WebP payload when its sidecar has invalid geometry");
    for (const char* face : softwareFaces) {
        files.erase(std::string("gfx/env/test") + face + ".webp");
        files[std::string("gfx/env/test") + face + ".tga"] = TargaFixture();
        files.erase(std::string("gfx/env/desert") + face + ".tga");
    }
    reloadSoftwareSky();
    Check(std::all_of(std::begin(gSkies), std::end(gSkies), [](const pixel_t* face) { return face == nullptr; }),
          "software sky leaves every face empty when original and default sky geometry are invalid or absent");
    R_FilterSkyBox();
    Check(gFakeSurface[0].samples == nullptr, "software filtering safely skips unavailable sky faces");
#endif
    Check(liveFileAllocations == 0, "all loaded files are released after success and failure");
    for (void* allocation : hunkAllocations) {
        std::free(allocation);
    }
    if (failures == 0) {
        std::printf("engine image loader tests passed\n");
    }
    return failures ? 1 : 0;
}
