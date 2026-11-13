#include "VGUI_TextImage.h"

#include "VGUI_App.h"
#include "VGUI_Font.h"
#include "VGUI_Scheme.h"

#include "common/VGui1TextEncoding.h"
#include <algorithm>
#include <new>
#include <string.h>

namespace vgui
{

TextImage::TextImage( int textBufferLen, const char* text )
	: Image()
{
	init( textBufferLen, text );
}

TextImage::TextImage( const char* text )
	: Image()
{
	init( (int)strlen( text ) + 1, text );
}

Font* TextImage::getFont()
{
	if ( _font != NULL )
		return _font;

	return App::getInstance()->getScheme()->getFont( _schemeFont );
}

void TextImage::getTextSize( int& wide, int& tall )
{
	wide = 0;
	tall = 0;
	if ( _text != NULL )
	{
		Font* font = getFont();
		if ( font != NULL )
			font->getTextSize( _text, wide, tall );
	}
}

namespace {
struct TextExtent {
    int wide = 0;
    int tall = 0;
};

template<typename Paint>
TextExtent LayoutText(Font& font, const char* text, int availableWide, Paint paint) {
    TextExtent extent;
    const std::size_t length = strlen(text);
    const cse_vgui1::text::Run run(text, length);
    const int lineTall = font.getTall();
    int x = 0;
    int y = 0;
    const auto advance = [&](int character) {
        int a = 0;
        int b = 0;
        int c = 0;
        font.getCharABCwide(character, a, b, c);
        return a + b + c;
    };
    for (std::size_t offset = 0; offset < length;) {
        const auto character = run.At(offset);
        if (character.value == '\r') {
            offset += character.bytes;
        } else if (character.value == '\n') {
            x = 0;
            y += lineTall;
            offset += character.bytes;
        } else if (character.value == ' ') {
            offset += character.bytes;
            if (offset < length) {
                const int next = run.At(offset).value;
                if (next != '\n' && next != '\r') {
                    x += advance(' ');
                    if (x > availableWide) {
                        x = 0;
                        y += lineTall;
                    }
                }
            }
        } else {
            std::size_t end = offset;
            int wordWide = 0;
            while (end < length) {
                const auto next = run.At(end);
                if (next.value == ' ' || next.value == '\n' || next.value == '\r') {
                    break;
                }
                wordWide += advance(run.GlyphAt(end).value);
                end += next.bytes;
            }
            if (x + wordWide > availableWide) {
                x = 0;
                y += lineTall;
            }
            while (offset < end) {
                const auto next = run.At(offset);
                paint(x, y, next.value, text + offset, next.bytes, run.IsUtf8());
                x += advance(run.GlyphAt(offset).value);
                extent.wide = std::max(extent.wide, x);
                extent.tall = std::max(extent.tall, y + lineTall);
                offset += next.bytes;
            }
        }
    }
    return extent;
}
}

void TextImage::getTextSizeWrapped(int& wide, int& tall) {
    wide = 0;
    tall = 0;
    if (_text == nullptr) {
        return;
    }
    int availableWide = 0;
    int ignoredTall = 0;
    getSize(availableWide, ignoredTall);
    const auto extent = LayoutText(*getFont(), _text, availableWide,
        [](int, int, int, const char*, std::size_t, bool) {});
    wide = extent.wide;
    tall = extent.tall;
}

void TextImage::paint(Panel*) {
    if (_text == nullptr) {
        return;
    }
    int availableWide = 0;
    int ignoredTall = 0;
    getSize(availableWide, ignoredTall);
    Color color;
    getColor(color);
    int red = 0;
    int green = 0;
    int blue = 0;
    int alpha = 0;
    color.getColor(red, green, blue, alpha);
    drawSetTextColor(red, green, blue, alpha);
    Font* font = getFont();
    drawSetTextFont(font);
    LayoutText(*font, _text, availableWide,
        [&](int x, int y, int character, const char* bytes, std::size_t count, bool utf8) {
            if (utf8) {
                // Existing char-only drawPrintChar would truncate a Unicode
                // scalar. The surface receives its complete original UTF8 run.
                drawPrintText(x, y, bytes, static_cast<int>(count));
            } else {
                drawPrintChar(x, y, static_cast<char>(character));
            }
        });
}

void TextImage::setFont( Scheme::SchemeFont schemeFont )
{
	_schemeFont = schemeFont;
}

void TextImage::setFont( Font* font )
{
	_font = font;
}

void TextImage::setSize( int wide, int tall )
{
	Image::setSize( wide, tall );
}

void TextImage::setText( int textBufferLen, const char* text )
{
	if ( textBufferLen > _textBufferLen )
	{
		::operator delete( _text );
		_textBufferLen = textBufferLen;
		_text = new char[textBufferLen];
		if ( _text == NULL )
			_textBufferLen = 0;
	}

	if ( _text != NULL )
		vgui_strcpy( _text, _textBufferLen, text );

	int wide;
	int tall;
	getTextSize( wide, tall );
	Image::setSize( wide, tall );
}

void TextImage::setText( const char* text )
{
	setText( (int)( strlen( text ) + 1 ), text );
}

void TextImage::init( int, const char* text )
{
	int wide;
	int tall;

	_schemeFont = Scheme::SchemeFont::sf_primary1;
	_text = NULL;
	_font = NULL;
	_textBufferLen = 0;
	setText( text );

	getTextSize( wide, tall );
	setSize( wide, tall );
}

}
