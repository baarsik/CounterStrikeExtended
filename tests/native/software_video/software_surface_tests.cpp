// Actual swds EngineSurface007 and Draw_RGBAImage; no replacement rasterizer.
#include "../../../GoldSrc-SDK/engine/vid.h"
#include "../../../GoldSrc-SDK/engine/IEngineSurface.h"
#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <dlfcn.h>
#include <vector>

namespace {
// An unrelated implementation must be rejected before dispatch. The production
// interface has no backend discriminator or extra virtual slots.
class ForeignSurface final : public IEngineSurface {
public:
#define FOREIGN(signature) signature override { std::abort(); }
    FOREIGN(void pushMakeCurrent(int*, int*, int*, bool))
    FOREIGN(void popMakeCurrent())
    FOREIGN(void drawFilledRect(int, int, int, int))
    FOREIGN(void drawOutlinedRect(int, int, int, int))
    FOREIGN(void drawLine(int, int, int, int))
    FOREIGN(void drawPolyLine(int*, int*, int))
    FOREIGN(void drawTexturedPolygon(vgui2::VGuiVertex*, int))
    FOREIGN(void drawSetTextureRGBA(int, const char*, int, int, int, int))
    FOREIGN(void drawSetTexture(int))
    FOREIGN(void drawTexturedRect(int, int, int, int))
    FOREIGN(void drawTexturedRectAdd(int, int, int, int))
    FOREIGN(int createNewTextureID())
    FOREIGN(void drawSetColor(int, int, int, int))
    FOREIGN(void drawSetTextColor(int, int, int, int))
    FOREIGN(void drawSetTextPos(int, int))
    FOREIGN(void drawGetTextPos(int&, int&))
    FOREIGN(void drawPrintChar(int, int, int, int, float, float, float, float))
    FOREIGN(void drawPrintCharAdd(int, int, int, int, float, float, float, float))
    FOREIGN(void drawSetTextureFile(int, const char*, int, bool))
    FOREIGN(void drawGetTextureSize(int, int&, int&))
    FOREIGN(int isTextureIdValid(int))
    FOREIGN(void drawSetSubTextureRGBA(int, int, int, const unsigned char*, int, int))
    FOREIGN(void drawFlushText())
    FOREIGN(void resetViewPort())
    FOREIGN(void drawSetTextureBGRA(int, const char*, int, int, int, int))
    FOREIGN(void drawUpdateRegionTextureBGRA(int, int, int, const unsigned char*, int, int))
#undef FOREIGN
};
template<class T> T Symbol(void* image, const char* name) {
    auto value = reinterpret_cast<T>(dlsym(image, name));
    assert(value);
    return value;
}
}
int main(int argc, char** argv) {
    assert(argc == 2 || argc == 3);
    void* image = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
    if (!image) {
        std::fprintf(stderr, "%s\n", dlerror());
        return 1;
    }
    auto factory = Symbol<void* (*)(const char*, int*)>(image, "CreateInterface");
    auto* surface = static_cast<IEngineSurface*>(factory("EngineSurface007", nullptr));
    assert(surface);
    auto upload = Symbol<bool (*)(IEngineSurface*, int, const unsigned char*, std::size_t)>(image,
        "_ZN16cse_vgui1_native20UploadEngineFontPageEP14IEngineSurfaceiPKhm");
    auto retire = Symbol<bool (*)(IEngineSurface*, int, bool)>(image,
        "_ZN16cse_vgui1_native20RetireEngineFontPageEP14IEngineSurfaceib");
    constexpr int AtlasSide = 512;
    constexpr std::size_t Channels = 4;
    std::vector<unsigned char> atlas(AtlasSide * AtlasSide * Channels, 0);
    assert(reinterpret_cast<std::uintptr_t>(atlas.data()) > UINT32_MAX);
    ForeignSurface foreign;
    assert(!retire(&foreign, 4001, true));
    if (argc == 2 || std::strcmp(argv[2], "images") != 0) {
        assert(!upload(&foreign, 4001, atlas.data(), atlas.size()));
    }
    if (argc == 3 && std::strcmp(argv[2], "images") != 0) {
        void* otherImage = dlopen(argv[2], RTLD_NOW | RTLD_LOCAL);
        assert(otherImage);
        auto otherFactory = Symbol<void* (*)(const char*, int*)>(otherImage, "CreateInterface");
        auto* hardware = static_cast<IEngineSurface*>(otherFactory("EngineSurface007", nullptr));
        assert(hardware && hardware != surface);
        assert(!upload(hardware, 4001, atlas.data(), atlas.size()));
        assert(!retire(hardware, 4001, true));
        assert(dlclose(otherImage) == 0);
    }
    const int id = surface->createNewTextureID();
    assert(!upload(surface, id, atlas.data(), atlas.size() - 1));
    // One opaque glyph sample with a transparent neighbour exercises atlas UV
    // selection; its bytes match the retained software font coverage contract.
    constexpr int GlyphX = 7;
    constexpr int GlyphY = 9;
    const std::size_t glyphOffset = (GlyphY * AtlasSide + GlyphX) * Channels;
    std::memset(atlas.data() + glyphOffset, 255, Channels);
    assert(upload(surface, id, atlas.data(), atlas.size()));
    assert(surface->isTextureIdValid(id));
    auto* engineVid = Symbol<viddef_t*>(image, "vid");
    auto* rgb555 = Symbol<int*>(image, "is15bit");
    constexpr int Width = 3;
    constexpr int Height = 3;
    constexpr int PaddedWidth = 4;
    std::vector<unsigned short> frame(PaddedWidth * Height, 0);
    assert(reinterpret_cast<std::uintptr_t>(frame.data()) > UINT32_MAX);
    engineVid->width = Width;
    engineVid->height = Height;
    engineVid->rowbytes = PaddedWidth * sizeof(unsigned short);
    engineVid->buffer = reinterpret_cast<byte*>(frame.data());
    for (int format : {0, 1}) {
        *rgb555 = format;
        std::fill(frame.begin(), frame.end(), 0);
        surface->drawSetTextColor(0, 255, 0, 0);
        surface->drawSetTexture(id);
        surface->drawPrintChar(1, 1, 2, 1, float(GlyphX) / AtlasSide, float(GlyphY) / AtlasSide,
            float(GlyphX + 2) / AtlasSide, float(GlyphY + 1) / AtlasSide);
        assert(frame[PaddedWidth + 1] == (format ? 0x03e0 : 0x07e0));
        assert(frame[PaddedWidth + 2] == 0 && frame[PaddedWidth + 3] == 0);
    }
    const int picture = surface->createNewTextureID();
    const unsigned char bgra[]{0, 0, 255, 255, 255, 0, 0, 255};
    surface->drawSetTextureBGRA(picture, reinterpret_cast<const char*>(bgra), 2, 1, 0, 0);
    surface->drawSetTexture(picture);
    surface->drawSetColor(255, 255, 255, 0);
    for (int format : {0, 1}) {
        *rgb555 = format;
        std::fill(frame.begin(), frame.end(), 0);
        surface->drawTexturedRect(0, 0, 2, 1);
        assert(frame[0] == (format ? 0x7c00 : 0xf800));
        assert(frame[1] == 0x001f);
    }
    const unsigned char initialBgra[]{
        0, 0, 255, 255, 0, 0, 255, 255, 0, 0, 255, 255,
        0, 0, 255, 255, 0, 0, 255, 255, 0, 0, 255, 255,
        0, 0, 255, 255, 0, 0, 255, 255, 0, 0, 255, 255};
    const unsigned char updatedFullFrame[]{
        255, 0, 0, 255, 255, 0, 0, 255, 255, 0, 0, 255,
        255, 0, 0, 255, 0, 255, 0, 255, 255, 0, 0, 255,
        0, 255, 0, 255, 255, 0, 0, 255, 0, 255, 0, 255};
    surface->drawSetTextureBGRA(picture, reinterpret_cast<const char*>(initialBgra), 3, 3, 0, 0);
    surface->drawUpdateRegionTextureBGRA(picture, 1, 1, updatedFullFrame, 1, 2);
    surface->drawSetTexture(picture);
    surface->drawTexturedRect(0, 0, 3, 3);
    for (int row = 0; row < 3; ++row) {
        for (int column = 0; column < 3; ++column) {
            const unsigned short expected = column == 1 && row == 1 ? 0x03e0
                : column == 1 && row == 2 ? 0x001f : 0x7c00;
            assert(frame[row * PaddedWidth + column] == expected);
        }
    }
    // Invalid updates cannot corrupt the already-populated image.
    surface->drawUpdateRegionTextureBGRA(picture, 2, 0, updatedFullFrame, 2, 1);
    surface->drawSetSubTextureRGBA(picture, -1, 0, updatedFullFrame, 2, 1);
    surface->drawSetSubTextureRGBA(0, 0, 0, updatedFullFrame, 2, 1);
    surface->drawTexturedRect(0, 0, 3, 3);
    assert(frame[0] == 0x7c00 && frame[PaddedWidth + 1] == 0x03e0);
    using DrawPixels = void (*)(int, int, int, int, int, int, int, int, unsigned char*, int,
        unsigned char, unsigned char, unsigned char, unsigned char);
    const auto raster = Symbol<DrawPixels>(image, "_Z14Draw_RGBAImageiiiiiiiiPhihhhh");
    const auto additiveRaster = Symbol<DrawPixels>(image, "_Z22Draw_RGBAImageAdditiveiiiiiiiiPhihhhh");
    const auto initLut = Symbol<void (*)()>(image, "_Z13R_DrawInitLutv");
    auto* lut = Symbol<unsigned char*>(image, "r_lut");
    constexpr std::size_t LutEntries = 65536;
    unsigned char rgbaImage[]{
        255, 255, 255, 0, 255, 0, 0, 128, 0, 255, 0, 255,
        0, 0, 255, 64, 255, 255, 255, 0, 192, 64, 32, 200,
        0, 255, 255, 255, 255, 255, 0, 128, 255, 255, 255, 0};
    unsigned char bgraImage[sizeof(rgbaImage)]{};
    for (std::size_t offset = 0; offset < sizeof(rgbaImage); offset += Channels) {
        bgraImage[offset] = rgbaImage[offset + 2];
        bgraImage[offset + 1] = rgbaImage[offset + 1];
        bgraImage[offset + 2] = rgbaImage[offset];
        bgraImage[offset + 3] = rgbaImage[offset + 3];
    }
    constexpr unsigned short Background = 0x0210;
    std::vector<unsigned short> expected(frame.size(), Background);
    for (int format : {0, 1}) {
        *rgb555 = format;
        std::memset(lut, 0, LutEntries);
        initLut();
        for (bool bgraUpload : {false, true}) {
            if (bgraUpload) {
                surface->drawSetTextureBGRA(picture, reinterpret_cast<const char*>(bgraImage), 3, 3, 0, 0);
            } else {
                surface->drawSetTextureRGBA(picture, reinterpret_cast<const char*>(rgbaImage), 3, 3, 0, 0);
            }
            surface->drawSetTexture(picture);
            for (bool additive : {false, true}) {
                for (unsigned char alpha : {static_cast<unsigned char>(128), static_cast<unsigned char>(255)}) {
                    std::fill(expected.begin(), expected.end(), Background);
                    engineVid->buffer = reinterpret_cast<byte*>(expected.data());
                    (additive ? additiveRaster : raster)(0, 0, 0, 0, 3, 3, 3, 3, rgbaImage, 0,
                        255, 255, 255, alpha);
                    std::fill(frame.begin(), frame.end(), Background);
                    engineVid->buffer = reinterpret_cast<byte*>(frame.data());
                    surface->drawSetColor(255, 255, 255, 255 - alpha);
                    if (additive) {
                        surface->drawTexturedRectAdd(0, 0, 3, 3);
                    } else {
                        surface->drawTexturedRect(0, 0, 3, 3);
                    }
                    assert(frame == expected && frame[0] == Background);
                    assert(std::count(frame.begin(), frame.end(), Background) < static_cast<int>(frame.size()));
                }
            }
            // Full upload must also obey the software nonzero glyph-mask contract.
            std::fill(frame.begin(), frame.end(), Background);
            surface->drawSetTextColor(0, 255, 0, 0);
            surface->drawPrintChar(0, 0, 3, 3, 0, 0, 1, 1);
            assert(frame[0] == Background && frame[1] == (format ? 0x03e0 : 0x07e0));
            // Dirty BGRA replacement is a full-frame source even for this one cell.
            surface->drawUpdateRegionTextureBGRA(picture, 0, 0, bgraImage, 1, 1);
            std::fill(frame.begin(), frame.end(), Background);
            surface->drawPrintChar(0, 0, 3, 3, 0, 0, 1, 1);
            assert(frame[0] == Background);
        }
    }
    assert(retire(surface, picture, true));
    assert(retire(surface, id, true));
    assert(!surface->isTextureIdValid(id));
    assert(retire(surface, id, false));
    // Retirement of a bound page must clear the borrowed current texture.
    std::fill(frame.begin(), frame.end(), 0);
    surface->drawPrintChar(0, 0, 1, 1, 0, 0, 1, 1);
    assert(frame[0] == 0);
    engineVid->buffer = nullptr;
    assert(dlclose(image) == 0);
    std::puts("PASS actual SW font upload/draw/retire, foreign backend rejection, high addresses and padded 555/565");
}
