#include "VGUI_Font.h"
#include "common/VGui1TextEncoding.h"
#include <cstring>

using namespace vgui;

void Font::getTextSize(const char* text, int& wide, int& tall) {
    wide = 0;
    tall = 0;
    if (text == nullptr) {
        return;
    }
    const std::size_t length = std::strlen(text);
    const cse_vgui1::text::Run run(text, length);
    tall = getTall();
    int lineWide = 0;
    for (std::size_t offset = 0; offset < length;) {
        const auto character = run.GlyphAt(offset);
        offset += character.bytes;
        if (character.value == '\n') {
            lineWide = 0;
            tall += getTall();
        } else {
            int a = 0;
            int b = 0;
            int c = 0;
            getCharABCwide(character.value, a, b, c);
            lineWide += a + b + c;
            if (lineWide > wide) {
                wide = lineWide;
            }
        }
    }
}


int Font::getId()
{
	return _id;
}

BaseFontPlat* Font::getPlat()
{
	return _plat;
}
