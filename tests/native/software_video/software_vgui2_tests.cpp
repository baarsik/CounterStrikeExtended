// Real GameUI VGUI2 font manager/cache/surface draw chain. Initialization only
// supplies the actual SW backend, avoiding unrelated Steam/scheme/window startup.
#include "../../../GoldSrc-SDK/engine/vgui2/BaseUISurface.h"
#include "../../../GoldSrc-SDK/engine/vgui_EngineSurface.h"
#include "../../../GoldSrc-SDK/engine/vid.h"
#include "../../../GoldSrc-SDK/vgui2/vgui_surfacelib/FontManager.h"
#include "../../../GoldSrc-SDK/vgui2/vgui_surfacelib/LinuxFont.h"
#include <algorithm>
#include <cassert>
#include <cstdio>
#include <vector>

extern BaseUISurface g_BaseUISurface;
extern BaseUISurface* staticSurface;
extern int is15bit;
namespace {
template<class Tag, typename Tag::Type Member> struct Access {
    friend typename Tag::Type Field(Tag) {
        return Member;
    }
};
struct BackendTag {
    using Type = IEngineSurface* BaseUISurface::*;
    friend Type Field(BackendTag);
};
template struct Access<BackendTag, &BaseUISurface::_engineSurface>;
}
int main() {
    EngineSurface backend;
    BaseUISurface& surface = g_BaseUISurface;
    surface.*Field(BackendTag{}) = &backend;
    staticSurface = &surface;
    constexpr int FontHeight = 24;
    constexpr int FontWeight = 400;
    constexpr int Antialias = 0x010;
    const auto font = surface.CreateFont();
    assert(surface.AddGlyphSetToFont(font, "Helvetica", FontHeight, FontWeight, 0, 0, Antialias, 0, 0));
    auto* face = FontManager().GetFontForChar(font, L'A');
    assert(face);
    int a = 0;
    int b = 0;
    int c = 0;
    surface.GetCharABCwide(font, 'A', a, b, c);
    const int height = surface.GetFontTall(font);
    constexpr int Channels = 4;
    std::vector<unsigned char> reference(b * height * Channels, 0);
    face->GetCharRGBA('A', 0, 0, b, height, reference.data());
    int transparentWhite = 0;
    int visible = 0;
    for (std::size_t offset = 0; offset < reference.size(); offset += Channels) {
        transparentWhite += reference[offset] == 255 && reference[offset + 3] == 0;
        visible += reference[offset + 3] != 0;
    }
    assert(transparentWhite > 0 && visible > 0);
    constexpr int Width = 128;
    constexpr int Height = 64;
    constexpr int RowPixels = Width + 1;
    constexpr unsigned short Background = 0x1234;
    std::vector<unsigned short> frame(RowPixels * Height, Background);
    vid.width = Width;
    vid.height = Height;
    vid.rowbytes = RowPixels * sizeof(unsigned short);
    vid.buffer = reinterpret_cast<byte*>(frame.data());
    constexpr int PenX = 3;
    constexpr int PenY = 2;
    surface.DrawSetTextFont(font);
    surface.DrawSetTextColor(0, 255, 0, 255);
    for (int format : {0, 1}) {
        is15bit = format;
        std::fill(frame.begin(), frame.end(), Background);
        surface.DrawSetTextPos(PenX, PenY);
        surface.DrawUnicodeChar(L'A');
        int mismatch = 0;
        for (int row = 0; row < height; ++row) {
            for (int column = 0; column < b; ++column) {
                const bool ink = reference[(row * b + column) * Channels + 3] != 0;
                const unsigned short expected = ink ? (format ? 0x03e0 : 0x07e0) : Background;
                mismatch += frame[(row + PenY) * RowPixels + PenX + a + column] != expected;
            }
        }
        std::printf("VGUI2 format=%d alpha-zero-white=%d visible=%d mismatched=%d\n",
            format, transparentWhite, visible, mismatch);
        std::fflush(stdout);
        assert(mismatch == 0);
    }
    FontManager().ClearAllFonts();
    vid.buffer = nullptr;
    surface.*Field(BackendTag{}) = nullptr;
    staticSurface = nullptr;
    std::puts("PASS actual VGUI2 FontManager/cache/subtexture/DrawUnicodeChar software glyph transparency");
}
