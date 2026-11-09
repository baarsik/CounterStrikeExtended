#include "VGUI_Bitmap.h"
#include "VGUI_Cursor.h"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <type_traits>

// Whole actual source, including its real anonymous CursorBitmap class. This
// permits correct complete-type teardown; no class/object/constructor substitute.
#include CSE_CURSOR_SOURCE

namespace {
constexpr int CursorSide = 16;
constexpr int CursorCenter = CursorSide / 2;
constexpr int ChannelMaximum = 255;
constexpr int ExpectedOrdinaryCount = vgui::Cursor::dc_last - 1;

template<class Tag, typename Tag::Type Member> struct MemberAccess {
    friend typename Tag::Type Field(Tag) {
        return Member;
    }
};
struct RgbaTag {
    using Type = uchar* vgui::Bitmap::*;
    friend Type Field(RgbaTag);
};
struct TextureTag {
    using Type = int vgui::Bitmap::*;
    friend Type Field(TextureTag);
};
struct UploadedTag {
    using Type = bool vgui::Bitmap::*;
    friend Type Field(UploadedTag);
};
template struct MemberAccess<RgbaTag, &vgui::Bitmap::_rgba>;
template struct MemberAccess<TextureTag, &vgui::Bitmap::_id>;
template struct MemberAccess<UploadedTag, &vgui::Bitmap::_uploaded>;

void Require(bool value, const char* description) {
    if (!value) {
        std::fprintf(stderr, "FAIL: %s\n", description);
        std::exit(1);
    }
}
}

extern "C" bool CheckPublicCursor(vgui::Cursor*, int, int, int, int);
extern "C" std::size_t PublicCursorBytes();
extern "C" std::size_t PublicBitmapBytes();
extern "C" std::size_t PublicImageBytes();

static void CheckCursor(vgui::Cursor::DefaultCursor kind) {
    vgui::Cursor cursor(kind);
    vgui::Bitmap* bitmap = cursor.getBitmap();
    Require(bitmap != nullptr && cursor.getDefaultCursor() == kind, "actual cursor identity and bitmap");
    int x = -1;
    int y = -1;
    cursor.getHotspot(x, y);
    const bool none = kind == vgui::Cursor::dc_none;
    Require(x == (none ? 0 : CursorCenter) && y == (none ? 0 : CursorCenter), "hotspot");
    int wide = -1;
    int tall = -1;
    bitmap->getSize(wide, tall);
    Require(wide == (none ? 0 : CursorSide) && tall == (none ? 0 : CursorSide), "image dimensions");
    bitmap->getPos(x, y);
    Require(x == 0 && y == 0, "Image constructor position");
    vgui::Color color;
    bitmap->getColor(color);
    int red = -1;
    int green = -1;
    int blue = -1;
    int alpha = -1;
    color.getColor(red, green, blue, alpha);
    Require(red == ChannelMaximum && green == ChannelMaximum && blue == ChannelMaximum && alpha == 0, "Image constructor color");
    Require(bitmap->*Field(TextureTag{}) == 0 && !(bitmap->*Field(UploadedTag{})), "no texture or upload");
    const uchar* rgba = bitmap->*Field(RgbaTag{});
    Require((rgba == nullptr) == none, "Bitmap constructor storage");
    int ink = 0;
    if (!none) {
        for (int row = 0; row < CursorSide; ++row) {
            for (int col = 0; col < CursorSide; ++col) {
                const std::size_t offset = static_cast<std::size_t>(row * CursorSide + col) * VGUI_RGBA_BYTES;
                const bool horizontal = row == CursorCenter;
                const bool vertical = col == CursorCenter;
                const uchar expectedGreen = horizontal && !vertical ? ChannelMaximum : 0;
                const uchar expectedBlue = vertical ? ChannelMaximum : 0;
                const uchar expectedAlpha = horizontal || vertical ? ChannelMaximum : 0;
                Require(rgba[offset] == 0 && rgba[offset + 1] == expectedGreen
                    && rgba[offset + 2] == expectedBlue && rgba[offset + 3] == expectedAlpha,
                    "complete ordinary cursor pixel bytes / blue vertical overwrites crossing");
                ink += expectedAlpha != 0;
            }
        }
        Require(ink == CursorSide * 2 - 1, "ordinary cross coverage");
    }
    Require(CheckPublicCursor(&cursor, static_cast<int>(kind), none ? 0 : CursorCenter,
        wide, tall), "real public declaring header typed calls");

    // FIXTURE ONLY: production Cursor/Bitmap/Image have no nested-storage cleanup.
    // The actual complete object is CursorBitmap; never delete it through a base
    // with a nonvirtual destructor. Its sole owned allocation is the rgba array.
    delete[] (bitmap->*Field(RgbaTag{}));
    bitmap->*Field(RgbaTag{}) = nullptr;
    auto* complete = static_cast<CursorBitmap*>(bitmap);
    complete->CursorBitmap::~CursorBitmap();
    ::operator delete(complete);
    std::printf("cursor=%d hotspot=%d,%d size=%d,%d nonzero=%d\n", static_cast<int>(kind),
        none ? 0 : CursorCenter, none ? 0 : CursorCenter, wide, tall, ink);
}

int main(int argc, char** argv) {
    Require(argc == 2, "one explicit mode required");
    Require(sizeof(vgui::Cursor) == PublicCursorBytes() && sizeof(vgui::Bitmap) == PublicBitmapBytes()
        && sizeof(vgui::Image) == PublicImageBytes(), "real public/private record size controls");
    const bool ordinary = std::strcmp(argv[1], "--ordinary") == 0;
    const bool none = std::strcmp(argv[1], "--none") == 0;
    const bool all = std::strcmp(argv[1], "--all") == 0;
    Require(ordinary || none || all, "known mode");
#if defined(_WIN32) || UINTPTR_MAX <= UINT32_MAX
    Require(ordinary, "unfixed dc_none is source-diagnostic only on legacy platforms");
#endif
    if (none || all) {
        CheckCursor(vgui::Cursor::dc_none);
    }
    int count = 0;
    if (ordinary || all) {
        for (int value = vgui::Cursor::dc_user; value < vgui::Cursor::dc_last; ++value) {
            if (value != vgui::Cursor::dc_none) {
                CheckCursor(static_cast<vgui::Cursor::DefaultCursor>(value));
                ++count;
            }
        }
        Require(count == ExpectedOrdinaryCount, "all ordinary default enum values");
    }
    return 0;
}
