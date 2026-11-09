#include <VGUI_Cursor.h>
#include <VGUI_Bitmap.h>
#include <VGUI_Image.h>
#include <cstddef>
#include <type_traits>

static_assert(std::is_same<decltype(&vgui::Cursor::getHotspot), void (vgui::Cursor::*)(int&, int&)>::value,
    "The real public getter has the expected typed signature");
static_assert(!std::has_virtual_destructor<vgui::Cursor>::value
    && !std::has_virtual_destructor<vgui::Bitmap>::value && !std::has_virtual_destructor<vgui::Image>::value,
    "Do not invent a public virtual destructor for fixture cleanup");
extern "C" std::size_t PublicCursorBytes() {
    return sizeof(vgui::Cursor);
}

extern "C" std::size_t PublicBitmapBytes() {
    return sizeof(vgui::Bitmap);
}

extern "C" std::size_t PublicImageBytes() {
    return sizeof(vgui::Image);
}

extern "C" bool CheckPublicCursor(vgui::Cursor* cursor, int kind, int hotspot, int wide, int tall) {
    int x = -1;
    int y = -1;
    cursor->getHotspot(x, y);
    if (x != hotspot || y != hotspot || static_cast<int>(cursor->getDefaultCursor()) != kind) {
        return false;
    }
    vgui::Bitmap* bitmap = cursor->getBitmap();
    bitmap->getSize(x, y);
    return x == wide && y == tall;
}
