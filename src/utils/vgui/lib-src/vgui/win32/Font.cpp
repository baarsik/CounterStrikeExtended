#include "VGUI_Font.h"
#include "../common/VGui1TextEncoding.h"

#include "VGUI_Dar.h"
#include "fileimage.h"
#include "vfontdata.h"
#include "vgui_win32.h"

#include <cstring>
#include <stdlib.h>
#include <string.h>

#define GLYPH_COUNT		256

namespace vgui
{
namespace
{

Dar<BaseFontPlat*> fontPlatforms;
int nextFontId = VGUI_FIRST_FONT_ID;

}

class FontPlat : public BaseFontPlat
{
public:
	FontPlat( const char* name, int tall, int wide, float rotation, int weight,
		bool italic, bool underline, bool strikeout, bool symbol )
		: _name( _strdup( name ) ), _wide( wide ), _tall( tall ), _rotation( rotation ), _weight( weight ),
		_italic( italic ), _underline( underline ), _strikeout( strikeout ), _symbol( symbol )
	{
		const int tenthsOfDegrees = (int)( rotation * 10.0 );
		_font = CreateFontA( tall, wide, tenthsOfDegrees, tenthsOfDegrees, weight,
			italic, underline, strikeout, symbol ? SYMBOL_CHARSET : ANSI_CHARSET,
			OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH, name );
		_dc = CreateCompatibleDC( NULL );
		SelectObject( _dc, _font );
		SetTextAlign( _dc, TA_UPDATECP );
		GetTextMetricsA( _dc, &_metrics );
		_bufferWide = _metrics.tmMaxCharWidth;
		_bufferTall = _metrics.tmAscent + _metrics.tmDescent + _metrics.tmHeight;
		BITMAPINFO info = {};
		info.bmiHeader.biSize = sizeof( info.bmiHeader );
		info.bmiHeader.biWidth = _bufferWide;
		info.bmiHeader.biHeight = -_bufferTall;
		info.bmiHeader.biPlanes = 1;
		info.bmiHeader.biBitCount = VGUI_RGBA_BYTES * 8;
		info.bmiHeader.biCompression = BI_RGB;
		void* pixels = NULL;
		_bitmap = CreateDIBSection( _dc, &info, DIB_RGB_COLORS, &pixels, NULL, 0 );
		_pixels = (uchar*)pixels;
#if defined( _MSC_VER )
#pragma warning( suppress : 28183 )
#endif
		SelectObject( _dc, _bitmap );
		memset( _abc, 0, sizeof( _abc ) );
		if ( !GetCharABCWidthsA( _dc, 0, GLYPH_COUNT - 1, _abc ) )
		{
			for ( int i = 0; i < GLYPH_COUNT; ++i )
			{
				_abc[i].abcB = _metrics.tmAveCharWidth;
			}
		}
	}

	virtual ~FontPlat() override
	{
	}

	virtual bool equals( const char* name, int tall, int wide, float rotation, int weight,
		bool italic, bool underline, bool strikeout, bool symbol ) override
	{
		return _stricmp( name, _name ) == 0 && tall == _tall && wide == _wide
			&& rotation == _rotation && weight == _weight && italic == _italic
			&& underline == _underline && strikeout == _strikeout && symbol == _symbol;
	}

	virtual void getCharRGBA( int ch, int rgbaX, int rgbaY, int rgbaWide, int rgbaTall, uchar* rgba ) override
	{
		SelectObject( _dc, _font );
		SetBkColor( _dc, RGB( 0, 0, 0 ) );
		SetTextColor( _dc, RGB( 255, 255, 255 ) );
		SetBkMode( _dc, OPAQUE );
        const ABC abc = CharacterMetrics(ch);
        MoveToEx(_dc, -abc.abcA, 0, nullptr);
        if (ch >= GLYPH_COUNT) {
            std::memset(_pixels, 0, VGUI_RGBA_BYTES * _bufferWide * _bufferTall);
            const wchar_t character = static_cast<wchar_t>(cse_vgui1::text::UnicodeValue(ch));
            ExtTextOutW(_dc, 0, 0, 0, nullptr, &character, 1, nullptr);
        } else {
            const char character = static_cast<char>(ch);
            ExtTextOutA(_dc, 0, 0, 0, nullptr, &character, 1, nullptr);
        }
		SetBkMode( _dc, TRANSPARENT );
		const int width = (int)abc.abcB < _bufferWide ? (int)abc.abcB : _bufferWide;
		const int height = _metrics.tmHeight < _bufferTall ? _metrics.tmHeight : _bufferTall;
		const float byteToUnit = 1.0f / 255.0f;
		for ( int y = 0; y < height; ++y )
		{
			for ( int x = 0; x < width; ++x )
			{
				const int outX = rgbaX + x;
				const int outY = rgbaY + y;
				if ( outX >= rgbaWide || outY >= rgbaTall )
				{
					continue;
				}
				const uchar* pixel = _pixels + VGUI_RGBA_BYTES * ( x + y * _bufferWide );
				const float blue = ch == '\t' ? 0.0f : (float)( pixel[0] * (double)byteToUnit );
				const float green = ch == '\t' ? 0.0f : (float)( pixel[1] * (double)byteToUnit );
				const double red = ch == '\t' ? 0.0 : pixel[2] * (double)byteToUnit;
				uchar* output = rgba + VGUI_RGBA_BYTES * ( outX + outY * rgbaWide );
				output[0] = (uchar)(int)( blue * 255.0 );
				output[1] = (uchar)(int)( green * 255.0 );
				output[2] = (uchar)(int)( red * 255.0 );
				const double coverage = red * 0.11f + (double)green * 0.55f + (double)blue * 0.34f;
				output[3] = (uchar)(int)( coverage * 255.0 );
			}
		}
	}

	virtual void getCharABCwide( int ch, int& a, int& b, int& c ) override
	{
        const ABC abc = CharacterMetrics(ch);
        a = abc.abcA < 0 ? 0 : abc.abcA;
        b = static_cast<int>(abc.abcB);
        c = abc.abcC;
	}

	virtual int getTall() override { return _metrics.tmHeight; }

	virtual void drawSetTextFont( SurfacePlat* surface ) override
	{
		SelectObject( surface->drawDC, _font );
	}

private:
    ABC CharacterMetrics(int character) {
        const bool unicodeByte = cse_vgui1::text::IsUnicodeByteGlyph(character);
        character = cse_vgui1::text::UnicodeValue(character);
        if (character < 0) {
            character = static_cast<unsigned char>(character);
        }
        if (!unicodeByte && character < GLYPH_COUNT) {
            return _abc[character];
        }
        ABC abc{};
        constexpr int BmpMaximum = 0xffff;
        if (character <= BmpMaximum && GetCharABCWidthsW(_dc, character, character, &abc)) {
            return abc;
        }
        return _abc[static_cast<unsigned char>('?')];
    }

	HFONT		_font;
	HDC			_dc;
	HBITMAP		_bitmap;
	TEXTMETRICA	_metrics;
	ABC			_abc[GLYPH_COUNT];
	int			_bufferWide;
	int			_bufferTall;
	uchar*		_pixels;

	VFontData	_unusedFontData;
	int			_unused;

	char*		_name;
	int			_wide;
	int			_tall;
	float		_rotation;
	int			_weight;
	bool		_italic;
	bool		_underline;
	bool		_strikeout;
	bool		_symbol;
};

class FontPlat_Bitmap : public BaseFontPlat
{
public:
    FontPlat_Bitmap() : _name(nullptr), _unicodeFont(nullptr)
	{
	}

	virtual ~FontPlat_Bitmap() override
	{
        delete[] _name;
        delete _unicodeFont;
    }

    static FontPlat_Bitmap* Create(const char* name, FileImageStream* stream, int weight,
        bool italic, bool underline, bool strikeout)
	{
		FontPlat_Bitmap* font = new FontPlat_Bitmap();
		if ( !LoadVFontDataFrom32BitTGA( stream, &font->_data ) )
		{
			delete font;
			return NULL;
		}
        font->_name = vgui_strdup(name);
        font->_weight = weight;
        font->_italic = italic;
        font->_underline = underline;
        font->_strikeout = strikeout;
        return font;
	}

	virtual bool equals( const char*, int, int, float, int, bool, bool, bool, bool ) override
	{
		return false;
	}

	virtual void getCharRGBA( int ch, int rgbaX, int rgbaY, int rgbaWide, int rgbaTall, uchar* rgba ) override
	{
        if (ch >= VFONT_GLYPH_COUNT) {
            UnicodeFallback()->getCharRGBA(ch, rgbaX, rgbaY, rgbaWide, rgbaTall, rgba);
            return;
        }
		else if ( ch < 0 )
		{
			ch = 0;
		}
		for ( int y = 0; y < _data.m_BitmapCharHeight; ++y )
		{
			for ( int x = 0; x < _data.m_BitmapCharWidth; ++x )
			{
				const int outX = rgbaX + x;
				const int outY = rgbaY + y;
				if ( outX >= rgbaWide || outY >= rgbaTall )
				{
					continue;
				}
				const int offset = ( y * VFONT_GLYPH_COUNT + ch ) * _data.m_BitmapCharWidth + x;
				const uchar color = _data.m_pBitmap[offset] == 0 ? 0 : 255;
				uchar* output = rgba + VGUI_RGBA_BYTES * ( outX + outY * rgbaWide );
				output[0] = output[1] = output[2] = output[3] = color;
			}
		}
	}

	virtual void getCharABCwide( int ch, int& a, int& b, int& c ) override
	{
		if ( ch < 0 )
		{
			ch = 0;
		}
        if (ch >= VFONT_GLYPH_COUNT) {
            UnicodeFallback()->getCharABCwide(ch, a, b, c);
            return;
        }
		a = c = 0;
		b = _data.m_CharWidths[ch] + 1;
	}

	virtual int getTall() override { return _data.m_BitmapCharHeight; }
	virtual void drawSetTextFont( SurfacePlat* ) override
	{
	}

private:
    FontPlat* UnicodeFallback() {
        if (_unicodeFont == nullptr) {
            _unicodeFont = new FontPlat(_name, _data.m_BitmapCharHeight, 0, 0.0f, _weight,
                _italic, _underline, _strikeout, false);
        }
        return _unicodeFont;
    }

    FontPlat* _unicodeFont;
    int _weight = 0;
    bool _italic = false;
    bool _underline = false;
    bool _strikeout = false;
	VFontData	_data;
	char*		_name;
};

void Font_Reset()
{
	fontPlatforms.setCount( 0 );
}

Font::Font( const char* name, int tall, int wide, float rotation, int weight,
	bool italic, bool underline, bool strikeout, bool symbol )
{
	init( name, NULL, 0, tall, wide, rotation, weight, italic, underline, strikeout, symbol );
}

Font::Font( const char* name, void* data, int length, int tall, int wide, float rotation, int weight,
	bool italic, bool underline, bool strikeout, bool symbol )
{
	init( name, data, length, tall, wide, rotation, weight, italic, underline, strikeout, symbol );
}

void Font::init( const char* name, void* data, int length, int tall, int wide, float rotation, int weight,
	bool italic, bool underline, bool strikeout, bool symbol )
{
	_name = _strdup( name );
	_id = -1;
	if ( data != NULL )
	{
		FileImageStream_Memory stream( data, length );
		_plat = FontPlat_Bitmap::Create(name, &stream, weight, italic, underline, strikeout);
		if ( _plat == NULL )
		{
			return;
		}
	}
	else
	{
		_plat = NULL;
		for ( int i = 0; i < fontPlatforms.getCount(); ++i )
		{
			if ( fontPlatforms[i]->equals( name, tall, wide, rotation, weight, italic, underline, strikeout, symbol ) )
			{
				_plat = fontPlatforms[i];
				return;
			}
		}
		_plat = new FontPlat( name, tall, wide, rotation, weight, italic, underline, strikeout, symbol );
	}
	fontPlatforms.addElement( _plat );
	_id = nextFontId++;
}

void Font::getCharRGBA( int ch, int rgbaX, int rgbaY, int rgbaWide, int rgbaTall, uchar* rgba )
{
	_plat->getCharRGBA( ch, rgbaX, rgbaY, rgbaWide, rgbaTall, rgba );
}

void Font::getCharABCwide( int ch, int& a, int& b, int& c )
{
	_plat->getCharABCwide( ch, a, b, c );
}

int Font::getTall()
{
	return _plat->getTall();
}

}
