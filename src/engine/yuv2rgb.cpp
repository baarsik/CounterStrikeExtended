#include <cstddef>

#define YUV_OVERFLOW_BITS 0x40080100
#define YUV_CARRY_BITS 0x00400801

constexpr int kRgbBytesPerPixel = 4;
constexpr int kYuvTableSize = 256;

struct yuv_row_t
{
	unsigned int pairs;
	bool hasLastPixel;
	short remainingRows;
};

static yuv_row_t YUV_CountRow( int width, int remainingRows )
{
	unsigned int count = (unsigned int)remainingRows - ( (unsigned int)width << 16 ) + 0x10000;
	unsigned int pairs = 0;

	if ( (int)count < 0 )
	{
		pairs = ( ~count >> 17 ) + 1;
		count += pairs << 17;
	}

	return { pairs, ( count & 0xFFFF0000 ) == 0, (short)count };
}

static unsigned int PackYuvPixel( unsigned int value )
{
	unsigned int overflow = value & YUV_OVERFLOW_BITS;

	if ( overflow )
	{
		value |= overflow - ( overflow >> 8 );
		value += ( (int)~( value >> 1 ) >> 8 ) & YUV_CARRY_BITS;
	}

	return ( value & 0xFF ) | ( ( value >> 14 ) & 0xFF00 ) | ( ( value & 0x0007F800 ) << 5 );
}

static void YUV_WritePackedPixel( unsigned char* dst, unsigned int pixel )
{
	dst[0] = (unsigned char)pixel;
	dst[1] = (unsigned char)( pixel >> 8 );
	dst[2] = (unsigned char)( pixel >> 16 );
	dst[3] = 0;
}

void yuv444_2_rgb8888(unsigned char* dst, const unsigned char* y, const unsigned char* u, const unsigned char* v,
    int width, int height, int ySpan, int uvSpan, int dstSpan, const unsigned int* tables, int) {
    const unsigned int* uTable = tables + kYuvTableSize;
    const unsigned int* vTable = uTable + kYuvTableSize;

    for (std::ptrdiff_t row = 0; row < height; ++row) {
        auto* out = dst + row * dstSpan;
        const auto* rowY = y + row * ySpan;
        const auto* rowU = u + row * uvSpan;
        const auto* rowV = v + row * uvSpan;
        for (int column = 0; column < width; ++column) {
            const unsigned int pixel = tables[rowY[column]] + uTable[rowU[column]] + vTable[rowV[column]];
            YUV_WritePackedPixel(out + column * kRgbBytesPerPixel, PackYuvPixel(pixel));
        }
    }
}

void yuv420_2_rgb8888( unsigned char* dst, const unsigned char* y, const unsigned char* u, const unsigned char* v,
	int width, int height, int ySpan, int uvSpan, int dstSpan, const unsigned int* tables, int )
{
	const unsigned int* uTable = tables + 256;
	const unsigned int* vTable = tables + 512;
	unsigned int* out = (unsigned int*)dst;
	const int outSpan = dstSpan >> 2;
	int remainingRows = (int)( (unsigned int)height - 1 );
	unsigned int pair;
	unsigned int chroma;

	if ( remainingRows > 0 )
	{
		do
		{
			const yuv_row_t row = YUV_CountRow( width, remainingRows );

			for ( pair = 0; pair < row.pairs; pair++ )
			{
				chroma = uTable[*u++] + vTable[*v++];
				out[outSpan] = PackYuvPixel( chroma + tables[y[ySpan]] );
				out[0] = PackYuvPixel( chroma + tables[y[0]] );
				out[outSpan + 1] = PackYuvPixel( chroma + tables[y[ySpan + 1]] );
				out[1] = PackYuvPixel( chroma + tables[y[1]] );
				y += 2;
				out += 2;
			}

			if ( row.hasLastPixel )
			{
				unsigned int lower, upper;

				chroma = uTable[*u] + vTable[*v];
				lower = PackYuvPixel( chroma + tables[y[ySpan]] );
				upper = PackYuvPixel( chroma + tables[y[0]] );
				y++;

				out[outSpan] = upper;
				out[0] = lower;
				out++;
			}

			out += 2 * outSpan - width;
			y += 2 * ySpan - width;
			u += uvSpan - ( width >> 1 );
			v += uvSpan - ( width >> 1 );
			remainingRows = row.remainingRows - 2;
		} while ( remainingRows > 0 );
	}

	if ( remainingRows == 0 )
	{
		const yuv_row_t row = YUV_CountRow( width, remainingRows );

		for ( pair = 0; pair < row.pairs; pair++ )
		{
			chroma = uTable[*u++] + vTable[*v++];
			out[0] = PackYuvPixel( chroma + tables[y[0]] );
			out[1] = PackYuvPixel( chroma + tables[y[1]] );
			y += 2;
			out += 2;
		}

		if ( row.hasLastPixel )
		{
			chroma = uTable[*u] + vTable[*v];
			*out = PackYuvPixel( chroma + tables[*y] );
		}
	}
}

void yuv422_2_rgb8888(unsigned char* dst, const unsigned char* y, const unsigned char* u, const unsigned char* v,
    int width, int height, int ySpan, int uvSpan, int dstSpan, const unsigned int* tables, int) {
    const unsigned int* uTable = tables + kYuvTableSize;
    const unsigned int* vTable = uTable + kYuvTableSize;
    const int pairedWidth = width - width % 2;

    for (std::ptrdiff_t row = 0; row < height; ++row) {
        auto* out = dst + row * dstSpan;
        const auto* rowY = y + row * ySpan;
        const auto* rowU = u + row * uvSpan;
        const auto* rowV = v + row * uvSpan;
        for (int column = 0; column < pairedWidth; column += 2) {
            const unsigned int chroma = uTable[rowU[column / 2]] + vTable[rowV[column / 2]];
            auto* pixel = out + column * kRgbBytesPerPixel;
            YUV_WritePackedPixel(pixel, PackYuvPixel(chroma + tables[rowY[column]]));
            YUV_WritePackedPixel(pixel + kRgbBytesPerPixel, PackYuvPixel(chroma + tables[rowY[column + 1]]));
        }
        if (pairedWidth < width) {
            const unsigned int chroma = uTable[rowU[pairedWidth / 2]] + vTable[rowV[pairedWidth / 2]];
            YUV_WritePackedPixel(out + pairedWidth * kRgbBytesPerPixel, PackYuvPixel(chroma + tables[rowY[pairedWidth]]));
        }
    }
}
