#include "VGUI_BitmapTGA.h"

#include "VGUI.h"
#include "VGUI_DataInputStream.h"
#include "image_codec.h"

#include <limits.h>
#include <cstring>

#define TGA_TRUE_COLOR		2
#define TGA_RLE_TRUE_COLOR	10
#define TGA_RGB_BITS		24
#define TGA_RGBA_BITS		32
#define TGA_RUN_MASK		0x7F

namespace vgui
{

BitmapTGA::BitmapTGA( InputStream* is, bool invertAlpha )
{
	loadTGA( is, invertAlpha );
}

bool BitmapTGA::loadTGA( InputStream* is, bool invertAlpha )
{
	if ( is == NULL )
		return false;

    constexpr std::size_t RiffHeaderBytes = 12;
    constexpr unsigned int BitsPerByte = 8;
    constexpr unsigned int OpaqueAlpha = 255;
    unsigned char signature[RiffHeaderBytes] = {};
    bool read = false;
    is->readUChar(signature, sizeof(signature), read);
    if (!read) {
        return false;
    }
    if (cse::image::IsWebP(signature, sizeof(signature))) {
        const std::size_t riffBytes = static_cast<std::size_t>(signature[4])
            | (static_cast<std::size_t>(signature[5]) << BitsPerByte)
            | (static_cast<std::size_t>(signature[6]) << (BitsPerByte * 2))
            | (static_cast<std::size_t>(signature[7]) << (BitsPerByte * 3));
        constexpr std::size_t RiffPrefixBytes = 8;
        if (riffBytes < RiffHeaderBytes - RiffPrefixBytes
            || riffBytes > cse::image::MaximumEncodedBytes - RiffPrefixBytes) {
            return false;
        }
        std::vector<unsigned char> encoded(riffBytes + RiffPrefixBytes);
        std::memcpy(encoded.data(), signature, sizeof(signature));
        is->readUChar(encoded.data() + RiffHeaderBytes, static_cast<int>(encoded.size() - RiffHeaderBytes), read);
        cse::image::Image image;
        if (!read || !cse::image::DecodeWebP(encoded.data(), encoded.size(), image)) {
            return false;
        }
        setSize(image.width, image.height);
        std::memcpy(_rgba, image.rgba.data(), image.rgba.size());
        if (!invertAlpha) {
            for (std::size_t alpha = VGUI_RGBA_BYTES - 1; alpha < image.rgba.size(); alpha += VGUI_RGBA_BYTES) {
                _rgba[alpha] = static_cast<uchar>(OpaqueAlpha - _rgba[alpha]);
            }
        }
        return true;
    }
    is->seekStart(read);
    if (!read) {
        return false;
    }

	DataInputStream dis( is );
	bool success = false;

	uchar idLength = dis.readUChar( success );
	if ( !success )
		return false;

	uchar colorMapType = dis.readUChar( success );
	if ( !success )
		return false;

	uchar imageType = dis.readUChar( success );
	if ( !success )
		return false;

	dis.readUShort( success );
	if ( !success )
		return false;

	dis.readUShort( success );
	if ( !success )
		return false;

	dis.readUChar( success );
	if ( !success )
		return false;

	dis.readUShort( success );
	if ( !success )
		return false;

	dis.readUShort( success );
	if ( !success )
		return false;

	int wide = dis.readUShort( success );
	if ( !success )
		return false;

	int tall = dis.readUShort( success );
	if ( !success )
		return false;

	uchar pixelSize = dis.readUChar( success );
	if ( !success )
		return false;

	dis.readUChar( success );
	if ( !success )
		return false;

	if ( imageType != TGA_TRUE_COLOR && imageType != TGA_RLE_TRUE_COLOR )
		return false;

	if ( colorMapType != 0 )
		return false;

	if ( pixelSize != TGA_RGB_BITS && pixelSize != TGA_RGBA_BITS )
		return false;

	if ( wide * tall > INT_MAX / VGUI_RGBA_BYTES )
		return false;

	setSize( wide, tall );
	if ( _rgba == NULL )
		return false;

	if ( idLength != 0 )
		dis.seekRelative( idLength, success );

	if ( imageType == TGA_TRUE_COLOR )
	{
		int row = tall;
		int offset = VGUI_RGBA_BYTES * wide * ( tall - 1 );

		while ( row != 0 )
		{
			uchar* dest = &_rgba[offset];

			for ( int column = 0; column < wide; ++column )
			{
				if ( pixelSize == TGA_RGB_BITS )
				{
					dest[2] = dis.readUChar( success );
					if ( !success )
						return false;

					dest[1] = dis.readUChar( success );
					if ( !success )
						return false;

					dest[0] = dis.readUChar( success );
					if ( !success )
						return false;

					dest[3] = 255;
					dest += VGUI_RGBA_BYTES;
				}
				else if ( pixelSize == TGA_RGBA_BITS )
				{
					dest[2] = dis.readUChar( success );
					if ( !success )
						return false;

					dest[1] = dis.readUChar( success );
					if ( !success )
						return false;

					dest[0] = dis.readUChar( success );
					if ( !success )
						return false;

					dest[3] = (uchar)( 255 - dis.readUChar( success ) );
					if ( !success )
						return false;

					if ( invertAlpha )
						dest[3] = (uchar)( 255 - dest[3] );

					dest += VGUI_RGBA_BYTES;
				}
			}

			--row;
			offset += -VGUI_RGBA_BYTES * wide;
		}

		return true;
	}

	int row = tall - 1;

	while ( row != -1 )
	{
		int column = 0;
		uchar* dest = &_rgba[VGUI_RGBA_BYTES * row * wide];

		while ( column < wide )
		{
			signed char packet = (signed char)dis.readUChar( success );
			if ( !success )
				return false;

			int count = ( packet & TGA_RUN_MASK ) + 1;

			if ( packet >= 0 )
			{
				for ( int i = 0; i < count; ++i )
				{
					if ( pixelSize == TGA_RGB_BITS )
					{
						dest[2] = dis.readUChar( success );
						if ( !success )
							return false;

						dest[1] = dis.readUChar( success );
						if ( !success )
							return false;

						dest[0] = dis.readUChar( success );
						if ( !success )
							return false;

						dest[3] = 255;
						if ( invertAlpha )
							dest[3] = 0;

						dest += VGUI_RGBA_BYTES;
					}
					else if ( pixelSize == TGA_RGBA_BITS )
					{
						dest[2] = dis.readUChar( success );
						if ( !success )
							return false;

						dest[1] = dis.readUChar( success );
						if ( !success )
							return false;

						dest[0] = dis.readUChar( success );
						if ( !success )
							return false;

						dest[3] = dis.readUChar( success );
						if ( !success )
							return false;

						if ( invertAlpha )
							dest[3] = (uchar)( 255 - dest[3] );

						dest += VGUI_RGBA_BYTES;
					}

					++column;
					if ( column == wide )
					{
						if ( row == 0 )
							return true;

						--row;
						column = 0;
						dest = &_rgba[VGUI_RGBA_BYTES * row * wide];
					}
				}
			}
			else
			{
				uchar blue = 0;
				uchar green = 0;
				uchar red = 0;
				uchar alpha = 0;

				if ( pixelSize == TGA_RGB_BITS )
				{
					blue = dis.readUChar( success );
					if ( !success )
						return false;

					green = dis.readUChar( success );
					if ( !success )
						return false;

					red = dis.readUChar( success );
					if ( !success )
						return false;

					alpha = 255;
					if ( invertAlpha )
						alpha = 0;
				}
				else if ( pixelSize == TGA_RGBA_BITS )
				{
					blue = dis.readUChar( success );
					if ( !success )
						return false;

					green = dis.readUChar( success );
					if ( !success )
						return false;

					red = dis.readUChar( success );
					if ( !success )
						return false;

					alpha = dis.readUChar( success );
					if ( !success )
						return false;

					if ( invertAlpha )
						alpha = (uchar)( 255 - alpha );
				}

				for ( int i = 0; i < count; ++i )
				{
					dest[0] = red;
					dest[1] = green;
					dest[2] = blue;
					dest[3] = alpha;
					dest += VGUI_RGBA_BYTES;

					++column;
					if ( column == wide )
					{
						if ( row == 0 )
							return true;

						--row;
						column = 0;
						dest = &_rgba[VGUI_RGBA_BYTES * row * wide];
					}
				}
			}
		}

		--row;
	}

	return true;
}

}
