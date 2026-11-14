#pragma once
#include "VGUI_InputStream.h"
#include <vector>

// Drawing is outside this fixture. Keep the production BitmapTGA decoder and
// stream implementations; replace only Bitmap's allocation/drawing boundary.
namespace vgui {
class BitmapTGA {
public:
    BitmapTGA(InputStream* stream, bool invertAlpha);
    void getSize(int& wide, int& tall) { wide = width; tall = height; }
    int width = 0;
    int height = 0;
    std::vector<uchar> pixels;
private:
    bool loadTGA(InputStream* stream, bool invertAlpha);
    void setSize(int wide, int tall) {
        width = wide;
        height = tall;
        pixels.resize(static_cast<std::size_t>(wide) * tall * 4);
        _rgba = pixels.data();
    }
    uchar* _rgba = nullptr;
};
}
