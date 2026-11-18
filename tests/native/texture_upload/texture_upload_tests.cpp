#include "glquake.h"
#include "gl_model.h"
#include "common.h"
#include "cvar.h"
#include "sys.h"
#include "ivideomode.h"
#include "engine_apiproxy.h"
#include "protocol.h"

#ifndef TRUE
#define TRUE 1
#define FALSE 0
#endif

#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

#if __has_include("texture_upload.h")
// Windows headers can define these macros before the private helper is included.
#define min(a, b) WINDOWS_MIN_MACRO_MUST_NOT_EXPAND
#define max(a, b) WINDOWS_MAX_MACRO_MUST_NOT_EXPAND
#include "texture_upload.h"
#undef min
#undef max
#endif

cvar_t gl_max_size{};
cvar_t gl_round_down{};
cvar_t gl_picmip{};
cvar_t gl_dither{};
cvar_t gl_spriteblend{};
cvar_t gl_ansio{};
qboolean g_bSupportsNPOTTextures = TRUE;
byte texgammatable[256];
int giTotalTexBytes;
int giTotalTextures;
int texels;
int gl_filter_min = GL_LINEAR_MIPMAP_LINEAR;
int gl_filter_max = GL_LINEAR;

struct Upload {
    int width;
    int height;
    GLenum format;
    std::vector<byte> pixels;
};

static std::vector<Upload> uploads;
static std::vector<GLfloat> parameters;
static GLint unpackAlignment = 4;
static GLint unpackRowLength = 17;
static std::array<GLint, 2> boundTextures{};
static std::size_t activeTexture = 0;
static unsigned bindingQueries = 0;
namespace cse {
namespace render {
bool nativeBinding = false;
bool UsingMetal() {
    return nativeBinding;
}
}
}

static void APIENTRY Capture(GLenum, GLint level, GLint, GLsizei width, GLsizei height, GLint,
    GLenum format, GLenum, const GLvoid* data) {
    assert(level == static_cast<int>(uploads.size()));
    assert(width > 0 && height > 0);
    const auto* bytes = static_cast<const byte*>(data);
#if !defined(TEXTURE_UPLOAD_BASELINE)
    assert(unpackAlignment == 1 && unpackRowLength == 0);
#endif
    const std::size_t channels = format == GL_LUMINANCE || format == GL_COLOR_INDEX ? 1 : 4;
    uploads.push_back({width, height, format, {bytes, bytes + width * height * channels}});
}

static void APIENTRY Parameter(GLenum, GLenum, GLfloat value) {
    parameters.push_back(value);
}

decltype(qglTexImage2D) qglTexImage2D = Capture;
decltype(qglTexParameterf) qglTexParameterf = Parameter;

static void APIENTRY GetInteger(GLenum name, GLint* value) {
    if (name == GL_TEXTURE_BINDING_2D) {
        ++bindingQueries;
        *value = boundTextures[activeTexture];
    } else {
        *value = name == GL_UNPACK_ALIGNMENT ? unpackAlignment : unpackRowLength;
    }
}

static void APIENTRY PixelStore(GLenum name, GLint value) {
    (name == GL_UNPACK_ALIGNMENT ? unpackAlignment : unpackRowLength) = value;
}

decltype(qglGetIntegerv) qglGetIntegerv = GetInteger;
decltype(qglPixelStorei) qglPixelStorei = PixelStore;

void Con_Printf(const char*, ...) {}
void Con_DPrintf(const char*, ...) {}

void Sys_Error(const char*, ...) {
    throw std::runtime_error("Sys_Error");
}

void Q_memcpy(void* destination, const void* source, int count) {
    std::memcpy(destination, source, count);
}

void VideoMode_GetCurrentVideoMode(int* width, int* height, int* bpp) {
    *width = 640;
    *height = 480;
    *bpp = 32;
}

void GL_Upload32(unsigned int*, int, int, qboolean, int, int);
void BoxFilter3x3(byte*, byte*, int, int, int, int);
void GL_ResampleAlphaTexture(byte*, int, int, byte*, int, int);
void GL_ResampleTexture(unsigned int*, int, int, unsigned int*, int, int);

#include "texture_functions.inc"

constexpr int MAX_GLTEXTURES = 10000;
struct gltexture_t {
    int texnum;
    short servercount;
    short paletteIndex;
    int width;
    int height;
    qboolean mipmap;
    char identifier[MAX_QPATH];
};
gltexture_t gltextures[MAX_GLTEXTURES];
int numgltextures;
int currenttexture;
int gHostSpawnCount = 1;
modfuncs_t g_modfuncs{};
cvar_t gl_palette_tex{};
cvar_t fs_perf_warnings{};
static int generatedTextures;
static int loadCallbacks;

void V_CheckGamma() {}

GLuint GL_GenTexture() {
    return ++generatedTextures;
}

static void APIENTRY BindTexture(GLenum, GLuint name) {
    boundTextures[activeTexture] = static_cast<GLint>(name);
}

static void APIENTRY SelectTexture(GLenum target) {
    activeTexture = static_cast<std::size_t>(target - GL_TEXTURE0_SGIS);
}

decltype(qglBindTexture) qglBindTexture = BindTexture;
decltype(qglSelectTextureSGIS) qglSelectTextureSGIS = SelectTexture;
int gl_mtexable = 1;
int cnttextures[2] = {-1, -1};
int TEXTURE0_SGIS = GL_TEXTURE0_SGIS;
GLenum oldtarget = GL_TEXTURE0_SGIS;
static std::array<byte, 256 * 3> uploadedPalette{};

static void APIENTRY ColorTable(GLenum, GLenum, GLsizei, GLenum, GLenum, const void* data) {
    std::memcpy(uploadedPalette.data(), data, uploadedPalette.size());
}

static void APIENTRY DeleteTextures(GLsizei count, const GLuint* names) {
    for (int i = 0; i < count; ++i) {
        for (GLint& binding : boundTextures) {
            if (binding == static_cast<GLint>(names[i])) {
                binding = 0;
            }
        }
    }
}

decltype(qglColorTableEXT) qglColorTableEXT = ColorTable;
decltype(qglDeleteTextures) qglDeleteTextures = DeleteTextures;

void Q_memset(void* destination, int value, int count) {
    std::memset(destination, value, count);
}

#include "texture_palette.inc"
#include "texture_bind.inc"

static void RecycledBinding() {
    constexpr int kReusedName = 17;
    constexpr int kPaletteIndex = 2;
    constexpr int kEncodedTexture = ((kPaletteIndex + 1) << 16) | kReusedName;
    cse::render::nativeBinding = true;
    currenttexture = -1;
    GL_Bind(kEncodedTexture);
    assert(boundTextures[0] == kReusedName && g_currentpalette == kPaletteIndex);
    // Native deletion unbinds the object; the historical software cache still names it.
    boundTextures[0] = 0;
    g_currentpalette = -1;
    GL_Bind(kEncodedTexture);
    assert(boundTextures[0] == kReusedName && g_currentpalette == kPaletteIndex);
    GL_SelectTexture(GL_TEXTURE1_SGIS);
    GL_Bind(kEncodedTexture);
    boundTextures[1] = 0;
    GL_SelectTexture(GL_TEXTURE0_SGIS);
    GL_SelectTexture(GL_TEXTURE1_SGIS);
    GL_Bind(kEncodedTexture);
    assert(boundTextures[1] == kReusedName);
    // Raw native binding changes do not update the engine cache either.
    boundTextures[1] = 9;
    GL_Bind(kEncodedTexture);
    assert(boundTextures[1] == kReusedName);
    cse::render::nativeBinding = false;
    boundTextures[1] = 0;
    const auto queries = bindingQueries;
    GL_Bind(kEncodedTexture);
    assert(boundTextures[1] == 0 && bindingQueries == queries); // Existing GL cache hits stay query-free.
    GL_SelectTexture(GL_TEXTURE0_SGIS);
}

int Q_strcmp(const char* first, const char* second) {
    return std::strcmp(first, second);
}

void Q_strncpy(char* destination, const char* source, int count) {
    std::strncpy(destination, source, count);
}

#include "texture_cache.inc"

static void Reset() {
    uploads.clear();
    parameters.clear();
    gl_max_size.value = 256;
    gl_round_down.value = 3;
    gl_picmip.value = 0;
    gl_spriteblend.value = 0;
    gl_dither.value = 0;
    g_bSupportsNPOTTextures = TRUE;
    giTotalTexBytes = 0;
    giTotalTextures = 0;
    texels = 0;
    numgltextures = 0;
    std::memset(gltextures, 0, sizeof(gltextures));
    generatedTextures = 0;
    loadCallbacks = 0;
    gl_palette_tex.value = 0;
    GL_PaletteInit();
    unpackAlignment = 4;
    unpackRowLength = 17;
    for (int i = 0; i < 256; ++i) {
        texgammatable[i] = static_cast<byte>(i);
    }
}

static void Ordinary() {
    std::array<unsigned int, 16> rgba{};
    for (unsigned int i = 0; i < rgba.size(); ++i) {
        rgba[i] = i * 0x01010101u;
    }
    GL_Upload32(rgba.data(), 4, 4, TRUE, TEX_TYPE_RGBA, GL_NEAREST);
    assert(uploads.size() == 3);
    assert(uploads[0].pixels.size() == sizeof(rgba));
    assert(!std::memcmp(uploads[0].pixels.data(), rgba.data(), sizeof(rgba)));
    const std::vector<byte> mip1{2, 2, 2, 2, 4, 4, 4, 4, 10, 10, 10, 10, 12, 12, 12, 12};
    assert(uploads[1].pixels == mip1);
    assert(uploads[2].pixels == std::vector<byte>({7, 7, 7, 7}));
    assert(parameters.size() == 3 && parameters[0] == gl_filter_min && parameters[1] == gl_filter_max);

    Reset();
    std::array<byte, 768> palette{};
    palette[3] = 10;
    palette[4] = 20;
    palette[5] = 30;
    std::array<byte, 4> indexed{1, 255, 1, 255};
    GL_Upload16(indexed.data(), 2, 2, FALSE, TEX_TYPE_ALPHA, palette.data(), GL_NEAREST);
    assert(uploads[0].pixels == std::vector<byte>({10, 20, 30, 255, 0, 0, 0, 0,
        10, 20, 30, 255, 0, 0, 0, 0}));
}

static void Anisotropy() {
    constexpr float noAnisotropicFiltering = 1.0f;
    unsigned int pixel = 0;
    for (float requested : {0.0f, -2.0f, 0.5f, 4.0f}) {
        Reset();
        gl_ansio.value = requested;
        GL_Upload32(&pixel, 1, 1, FALSE, TEX_TYPE_NONE, GL_NEAREST);
        assert(parameters.back() == std::max(noAnisotropicFiltering, requested));
    }
}

static void LargeIndexed() {
    constexpr int width = 1024;
    constexpr int height = 512;
    std::vector<byte> source(width * height, 1);
    std::array<byte, 768> palette{};
    palette[3] = 42;
    GL_Upload16(source.data(), width, height, FALSE, TEX_TYPE_NONE, palette.data(), GL_LINEAR);
    assert(uploads.size() == 1);
    assert(uploads[0].pixels.size() == source.size() * 4);
    assert(uploads[0].pixels.back() == 255);
    assert(uploads[0].pixels[uploads[0].pixels.size() - 4] == 42);
}

static void LargePalette() {
    constexpr int width = 2048;
    constexpr int height = 1024;
    char name[] = "large-studio-skin";
    std::vector<byte> source(width * height, 1);
    std::array<byte, 768> palette{};
    source.back() = 2;
    gl_palette_tex.value = 1;
    const int texture = GL_LoadTexture2(name, GLT_STUDIO, width, height, source.data(), FALSE,
        TEX_TYPE_NONE, palette.data(), GL_LINEAR);
    assert(texture != 0);
    assert(uploads.size() == 1 && uploads[0].format == GL_COLOR_INDEX && uploads[0].pixels == source);
    assert(texture == GL_LoadTexture2(name, GLT_STUDIO, width, height, nullptr, FALSE,
        TEX_TYPE_NONE, nullptr, GL_LINEAR));

    Reset();
    gl_palette_tex.value = 1;
    gl_max_size.value = 128;
    g_bSupportsNPOTTextures = FALSE;
    assert(GL_LoadTexture2(name, GLT_STUDIO, width, height, source.data(), FALSE,
        TEX_TYPE_NONE, palette.data(), GL_LINEAR) != 0);
    assert(uploads.size() == 1 && uploads[0].width == 128 && uploads[0].height == 128);
    assert(uploads[0].format == GL_COLOR_INDEX && uploads[0].pixels.front() == source.front());

#if !defined(TEXTURE_UPLOAD_BASELINE)
    Reset();
    gl_palette_tex.value = 1;
    for (int slot = 0; slot < NORMAL_GLPALETTES; ++slot) {
        gGLPalette[slot].tag = 1;
    }
    assert(GL_LoadTexture2(name, GLT_STUDIO, width, height, source.data(), FALSE,
        TEX_TYPE_NONE, palette.data(), GL_LINEAR) == 0);
    assert(numgltextures == 0 && generatedTextures == 0 && uploads.empty());
    assert(GL_LoadTexture2(name, GLT_STUDIO, 1, 1, source.data(), FALSE,
        TEX_TYPE_NONE, palette.data(), GL_LINEAR) != 0);
    assert(uploads.size() == 1 && uploads[0].format == GL_RGBA);
#endif
}

static void ReloadPlayerPalette() {
    constexpr int remappedIndex = 160;
    constexpr int red = remappedIndex * 3;
    std::array<byte, 256 * 3> palette{};
    std::array<byte, 4> skin{};
    skin.fill(remappedIndex);
    char name[] = "DM_Base.bmp1";
    gl_palette_tex.value = 1;
    cse::render::nativeBinding = true;

    palette[red] = 42;
    const int first = GL_LoadTexture2(name, GLT_STUDIO, 2, 2, skin.data(), FALSE,
        TEX_TYPE_NONE, palette.data(), GL_LINEAR);
    GL_Bind(first);
    assert(uploads.size() == 1 && uploads[0].format == GL_COLOR_INDEX);
    assert(uploadedPalette == palette);
    GL_UnloadTexture(name);

    uploads.clear();
    palette[red] = 180;
    const int second = GL_LoadTexture2(name, GLT_STUDIO, 2, 2, skin.data(), FALSE,
        TEX_TYPE_NONE, palette.data(), GL_LINEAR);
    GL_Bind(second);
    assert((second >> 16) == (first >> 16));
    assert(uploads.size() == 1 && uploads[0].pixels.front() == remappedIndex);
    assert(uploadedPalette == palette);
    cse::render::nativeBinding = false;
}

static void Luminance() {
    std::vector<byte> source(600, 100);
    std::fill(source.begin() + 300, source.end(), 200);
    std::vector<byte> result(2);
    GL_ResampleAlphaTexture(source.data(), 600, 1, result.data(), 2, 1);
    assert(result == std::vector<byte>({100, 200}));
    GL_Upload32(reinterpret_cast<unsigned int*>(source.data()), 600, 1, TRUE, TEX_TYPE_LUM, GL_LINEAR);
    assert(uploads.back().width == 1 && uploads.back().height == 1);
    assert(uploads.back().pixels.size() == 1);
    assert(unpackAlignment == 4 && unpackRowLength == 17);

    Reset();
    std::array<byte, 4> column{4, 8, 12, 16};
    GL_Upload16(column.data(), 1, 4, TRUE, TEX_TYPE_LUM, nullptr, GL_LINEAR);
    assert(uploads[1].pixels == std::vector<byte>({6, 14}));
    assert(uploads[2].pixels == std::vector<byte>({10}));

    Reset();
    g_bSupportsNPOTTextures = FALSE;
    GL_Upload32(reinterpret_cast<unsigned int*>(source.data()), 600, 1, FALSE, TEX_TYPE_LUM, GL_LINEAR);
    assert(uploads.size() == 1 && uploads[0].width == 256);
    assert(uploads[0].pixels.front() == 100 && uploads[0].pixels.back() == 200);
}

static void Narrow() {
    std::array<unsigned int, 4> source{0x04040404u, 0x08080808u, 0x0c0c0c0cu, 0x10101010u};
    GL_Upload32(source.data(), 1, 4, TRUE, TEX_TYPE_RGBA, GL_LINEAR);
    assert(uploads.size() == 3);
    assert(uploads[1].pixels == std::vector<byte>({6, 6, 6, 6, 14, 14, 14, 14}));
    assert(uploads[2].pixels == std::vector<byte>({10, 10, 10, 10}));

    Reset();
    GL_Upload32(source.data(), 4, 1, TRUE, TEX_TYPE_RGBA, GL_LINEAR);
    assert(uploads[1].pixels == std::vector<byte>({6, 6, 6, 6, 14, 14, 14, 14}));
    assert(uploads[2].pixels == std::vector<byte>({10, 10, 10, 10}));

    Reset();
    std::array<unsigned int, 15> odd{};
    for (unsigned int i = 0; i < odd.size(); ++i) {
        odd[i] = i * 0x01010101u;
    }
    GL_Upload32(odd.data(), 3, 5, TRUE, TEX_TYPE_RGBA, GL_LINEAR);
    assert(uploads[1].width == 1 && uploads[1].height == 2);
    assert(uploads[1].pixels == std::vector<byte>({2, 2, 2, 2, 8, 8, 8, 8}));
    assert(uploads[2].pixels == std::vector<byte>({5, 5, 5, 5}));
}

static void Wide() {
    std::vector<unsigned int> source(4096, 0x12345678u);
    std::vector<unsigned int> result(2048);
    GL_ResampleTexture(source.data(), 4096, 1, result.data(), 2048, 1);
    assert(std::all_of(result.begin(), result.end(), [](unsigned int pixel) {
        return pixel == 0x12345678u;
    }));
}

static void Invalid() {
    unsigned int source = 0;
    GL_Upload32(&source, 0, 1, FALSE, TEX_TYPE_RGBA, GL_LINEAR);
    assert(uploads.empty() && giTotalTextures == 0);
    GL_Upload32(&source, -1, 1, FALSE, TEX_TYPE_RGBA, GL_LINEAR);
    GL_Upload32(&source, 1, 1, FALSE, -1, GL_LINEAR);
    GL_Upload32(nullptr, 1, 1, FALSE, TEX_TYPE_RGBA, GL_LINEAR);
    assert(uploads.empty() && giTotalTextures == 0);
    std::array<byte, 768> palette{};
    palette.fill(7);
    GL_Upload16(reinterpret_cast<byte*>(&source), 2048, 1024, FALSE, TEX_TYPE_NONE, palette.data(), GL_LINEAR);
    assert(palette[0] == 7 && uploads.empty());
    GL_Upload32(&source, std::numeric_limits<int>::max(), 2, FALSE, TEX_TYPE_RGBA, GL_LINEAR);
    assert(uploads.empty() && giTotalTextures == 0);
    int width = 0;
    int height = 0;
    gl_picmip.value = 31;
    ComputeScaledSize(&width, &height, 2, 2);
    assert(width == 1 && height == 1);
    gl_max_size.value = std::numeric_limits<float>::infinity();
    ComputeScaledSize(&width, &height, 2, 2);
    assert(width == 0 && height == 0);
    for (float value : {std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::max(),
        -std::numeric_limits<float>::infinity()}) {
        gl_max_size.value = value;
        ComputeScaledSize(&width, &height, 2, 2);
        assert(width == 0 && height == 0);
    }
}

static void Sampling() {
    std::array<byte, 64> pointSource{};
    for (std::size_t i = 0; i < pointSource.size(); ++i) {
        pointSource[i] = static_cast<byte>(i);
    }
    std::array<byte, 4> pointResult{};
    GL_ResampleTexturePoint(pointSource.data(), 8, 8, pointResult.data(), 2, 2);
    assert((pointResult == std::array<byte, 4>{1, 5, 41, 45}));
    std::array<unsigned int, 16> rgba{};
    for (unsigned int i = 0; i < rgba.size(); ++i) {
        rgba[i] = i * 0x01010101u;
    }
    std::array<unsigned int, 4> result{};
    GL_ResampleTexture(rgba.data(), 4, 4, result.data(), 2, 2);
    assert((result == std::array<unsigned int, 4>{0x02020202u, 0x04040404u, 0x0a0a0a0au, 0x0c0c0c0cu}));
    int width;
    int height;
    ComputeScaledSize(&width, &height, 300, 100);
    assert(width == 256 && height == 64);
    gl_picmip.value = 2;
    ComputeScaledSize(&width, &height, 300, 100);
    assert(width == 64 && height == 16);
}

static void PaletteModes() {
    std::array<byte, 768> palette{};
    palette[255 * 3] = 10;
    palette[255 * 3 + 1] = 20;
    palette[255 * 3 + 2] = 30;
    byte source = 128;
    GL_Upload16(&source, 1, 1, FALSE, TEX_TYPE_ALPHA_GRADIENT, palette.data(), GL_NEAREST);
    assert(uploads[0].pixels == std::vector<byte>({10, 20, 30, 128}));

    Reset();
    palette.fill(64);
    gl_dither.value = 1;
    GL_Upload16(&source, 1, 1, FALSE, TEX_TYPE_NONE, palette.data(), GL_NEAREST);
    assert(uploads[0].pixels == std::vector<byte>({65, 65, 65, 255}));

    Reset();
    palette.fill(0);
    for (int i = 0; i < 256; ++i) {
        texgammatable[i] = static_cast<byte>(255 - i);
    }
    GL_Upload16(&source, 1, 1, FALSE, TEX_TYPE_RGBA, palette.data(), GL_NEAREST);
    assert(uploads[0].pixels == std::vector<byte>({255, 255, 255, 128}));
}

static void Unaligned() {
    std::vector<byte> source(17, 0);
    std::fill(source.begin() + 5, source.end(), 40);
    gl_spriteblend.value = 1;
    GL_Upload32(reinterpret_cast<unsigned int*>(source.data() + 1), 2, 2, FALSE, TEX_TYPE_RGBA, GL_LINEAR);
    assert(uploads[0].pixels[0] == 40 && uploads[0].pixels[3] == 0);
    assert(unpackAlignment == 4 && unpackRowLength == 17);
    uploads.clear();
    giTotalTexBytes = std::numeric_limits<int>::max();
    giTotalTextures = std::numeric_limits<int>::max();
    texels = std::numeric_limits<int>::max();
    GL_Upload32(reinterpret_cast<unsigned int*>(source.data() + 1), 2, 2, TRUE, TEX_TYPE_RGBA, GL_LINEAR);
    assert(giTotalTexBytes == std::numeric_limits<int>::max());
    assert(giTotalTextures == std::numeric_limits<int>::max());
    assert(texels == std::numeric_limits<int>::max());
}

static void BoundedViews() {
#if !defined(TEXTURE_UPLOAD_BASELINE)
    std::array<byte, 16> source{};
    std::array<byte, 4> destination{7, 7, 7, 7};
    assert(!texture_upload::Resample(source.data(), 15, 2, 2, destination.data(), 4, 1, 1, 4));
    assert(!texture_upload::Resample(source.data(), 16, 2, 2, destination.data(), 3, 1, 1, 4));
    assert(!texture_upload::Resample(source.data(), 16, 2, 2, destination.data(), 4, 0, 1, 4));
    assert(!texture_upload::MipMap(source.data(), 15, 2, 2, 4));
    assert((destination == std::array<byte, 4>{7, 7, 7, 7}));
#endif
}

static void Cache() {
    char name[] = "test";
    byte source = 0;
    std::array<byte, 768> palette{};
    g_modfuncs.m_pfnTextureLoad = [](char*, int, int, char*) {
        ++loadCallbacks;
    };
    assert(GL_LoadTexture2(name, GLT_WORLD, 0, 1, &source, FALSE, TEX_TYPE_NONE,
        palette.data(), GL_LINEAR) == 0);
    assert(GL_LoadTexture2(name, GLT_WORLD, 1, 1, nullptr, FALSE, TEX_TYPE_NONE,
        palette.data(), GL_LINEAR) == 0);
    assert(numgltextures == 0 && generatedTextures == 0 && loadCallbacks == 0 && uploads.empty());
    gl_palette_tex.value = 1;
    assert(GL_LoadTexture2(name, GLT_WORLD, 1, 1, &source, FALSE, TEX_TYPE_NONE,
        palette.data(), GL_LINEAR) != 0);
    assert(uploads.size() == 1 && uploads[0].format == GL_COLOR_INDEX);
    assert(unpackAlignment == 4 && unpackRowLength == 17);
    assert(loadCallbacks == 0 && generatedTextures == 1);
    assert(GL_LoadTexture2(name, GLT_WORLD, 1, 1, nullptr, FALSE, TEX_TYPE_NONE, nullptr, GL_LINEAR) != 0);
    assert(uploads.size() == 1 && generatedTextures == 1);
    g_modfuncs.m_pfnTextureLoad = nullptr;
}

int main(int argc, char** argv) {
    const std::string selected = argc > 1 ? argv[1] : "all";
    for (const auto& test : {std::pair{"ordinary", Ordinary}, {"large-indexed", LargeIndexed},
        {"large-palette", LargePalette}, {"luminance", Luminance}, {"narrow", Narrow}, {"wide", Wide},
        {"invalid", Invalid}, {"cache", Cache},
        {"sampling", Sampling}, {"anisotropy", Anisotropy}, {"palette-modes", PaletteModes}, {"unaligned", Unaligned},
        {"bounded-views", BoundedViews}, {"recycled-binding", RecycledBinding}, {"reload-player-palette", ReloadPlayerPalette}}) {
        if (selected == "all" || selected == test.first) {
            Reset();
            test.second();
            std::printf("PASS %s\n", test.first);
        }
    }
}
