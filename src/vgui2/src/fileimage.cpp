//========= Copyright © 1996-2003, Valve LLC, All rights reserved. ============
//
// The copyright to the contents herein is the property of Valve, L.L.C.
// The contents may be used and/or copied only with the written permission of
// Valve, L.L.C., or in accordance with the terms and conditions stipulated in
// the agreement/contract under which the contents have been supplied.
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#include "fileimage.h"
#include "image_codec.h"
#include "vgui_internal.h"
#include "FileSystem.h"

#include <string.h>

// TGA header.
#pragma pack( 1 )
class TGAFileHeader
{
public:
	unsigned char	m_IDLength;
	unsigned char	m_ColorMapType;
	unsigned char	m_ImageType;
	unsigned short	m_CMapStart;
	unsigned short	m_CMapLength;
	unsigned char	m_CMapDepth;
	unsigned short	m_XOffset;
	unsigned short	m_YOffset;
	unsigned short	m_Width;
	unsigned short	m_Height;
	unsigned char	m_PixelDepth;
	unsigned char	m_ImageDescriptor;
};
#pragma pack()

// ---------------------------------------------------------------------------------------- //
// FileImageStream_Memory.
// ---------------------------------------------------------------------------------------- //
FileImageStream_Memory::FileImageStream_Memory( void* pData, int dataLen )
{
	m_pData = (unsigned char*)pData;
	m_DataLen = dataLen;
	m_CurPos = 0;
	m_bError = false;
}

void FileImageStream_Memory::Read( void* pData, int len )
{
	unsigned char* pOut;
	int i;

	pOut = (unsigned char*)pData;
	for ( i = 0; i < len; i++ )
	{
		if ( m_CurPos < m_DataLen )
		{
			pOut[i] = m_pData[m_CurPos];
			++m_CurPos;
		}
		else
		{
			pOut[i] = 0;
			m_bError = true;
		}
	}
}

bool FileImageStream_Memory::ErrorStatus()
{
	bool ret = m_bError;
	m_bError = false;
	return ret;
}

// ---------------------------------------------------------------------------------------- //
// Encode/decode functions.
// ---------------------------------------------------------------------------------------- //
static void WriteRun( unsigned char* pColor, FileHandle_t fp, int runLength )
{
	unsigned char runCount;

	runCount = (unsigned char)( runLength - 1 );
	runCount |= ( 1 << 7 );
	vgui2::g_pFileSystem->Write( &runCount, 1, fp );
	vgui2::g_pFileSystem->Write( pColor, 4, fp );
}

// Load in a 32-bit TGA file.
bool Load32BitTGA( FileImageStream* fp, FileImage* pImage )
{
	TGAFileHeader hdr;
	char dummyChar;
	int i, x, y;
    unsigned char color[cse::image::RgbaChannels];
	int runLength, curOut;
	unsigned char* pLine;
	unsigned char packetHeader;

	pImage->Term();

    constexpr std::size_t RiffHeaderBytes = 12;
    constexpr std::size_t RiffPrefixBytes = 8;
    constexpr unsigned int BitsPerByte = 8;
    constexpr std::size_t PixelChannels = cse::image::RgbaChannels;
    unsigned char signature[RiffHeaderBytes] = {};
    fp->Read(signature, sizeof(signature));
    if (fp->ErrorStatus()) {
        return false;
    }
    if (cse::image::IsWebP(signature, sizeof(signature))) {
        const std::size_t riffBytes = static_cast<std::size_t>(signature[4])
            | (static_cast<std::size_t>(signature[5]) << BitsPerByte)
            | (static_cast<std::size_t>(signature[6]) << (BitsPerByte * 2))
            | (static_cast<std::size_t>(signature[7]) << (BitsPerByte * 3));
        if (riffBytes < RiffHeaderBytes - RiffPrefixBytes
            || riffBytes > cse::image::MaximumEncodedBytes - RiffPrefixBytes) {
            return false;
        }
        std::vector<unsigned char> encoded(riffBytes + RiffPrefixBytes);
        memcpy(encoded.data(), signature, sizeof(signature));
        fp->Read(encoded.data() + RiffHeaderBytes, static_cast<int>(encoded.size() - RiffHeaderBytes));
        cse::image::Image decoded;
        if (fp->ErrorStatus() || !cse::image::DecodeWebP(encoded.data(), encoded.size(), decoded)) {
            return false;
        }
        pImage->m_Width = decoded.width;
        pImage->m_Height = decoded.height;
        pImage->m_pData = new unsigned char[decoded.rgba.size()];
        // Existing font-atlas consumers expect top-down BGRA bytes.
        for (std::size_t pixel = 0; pixel < decoded.rgba.size(); pixel += PixelChannels) {
            pImage->m_pData[pixel] = decoded.rgba[pixel + 2];
            pImage->m_pData[pixel + 1] = decoded.rgba[pixel + 1];
            pImage->m_pData[pixel + 2] = decoded.rgba[pixel];
            pImage->m_pData[pixel + 3] = decoded.rgba[pixel + 3];
        }
        return true;
    }

    memcpy(&hdr, signature, sizeof(signature));
    fp->Read(reinterpret_cast<unsigned char*>(&hdr) + RiffHeaderBytes, sizeof(hdr) - RiffHeaderBytes);
    if (fp->ErrorStatus()) {
        return false;
    }
	if ( hdr.m_PixelDepth != 32 || hdr.m_ImageType != 10 )
	{
		return false;
	}

	for ( i = 0; i < hdr.m_IDLength; i++ )
	{
		fp->Read( &dummyChar, 1 );
	}

	pImage->m_Width = hdr.m_Width;
	pImage->m_Height = hdr.m_Height;
	pImage->m_pData = new unsigned char[hdr.m_Width * hdr.m_Height * 4];
	if ( !pImage->m_pData )
	{
		return false;
	}

	// Read in the data..
	for ( y = pImage->m_Height - 1; y >= 0; y-- )
	{
		pLine = &pImage->m_pData[y * pImage->m_Width * 4];

		curOut = 0;
		while ( curOut < pImage->m_Width )
		{
			fp->Read( &packetHeader, 1 );

			runLength = (int)( packetHeader & ~( 1 << 7 ) ) + 1;
			if ( curOut + runLength > pImage->m_Width )
			{
				return false;
			}

			if ( packetHeader & ( 1 << 7 ) )
			{
				fp->Read(color, sizeof(color));
				for ( x = 0; x < runLength; x++ )
				{
					memcpy(pLine, color, sizeof(color));
					pLine += 4;
				}
			}
			else
			{
				for ( x = 0; x < runLength; x++ )
				{
					fp->Read(color, sizeof(color));
					memcpy(pLine, color, sizeof(color));
					pLine += 4;
				}
			}

			curOut += runLength;
		}
	}

	return true;
}

// Write a 32-bit TGA file.
void Save32BitTGA( FileHandle_t fp, FileImage* pImage )
{
	TGAFileHeader hdr;
	int y, runStart, x;
	unsigned char* pLine;

	memset( &hdr, 0, sizeof( hdr ) );
	hdr.m_PixelDepth = 32;
	hdr.m_ImageType = 10;	// Run-length encoded RGB.
	hdr.m_Width = (unsigned short)pImage->m_Width;
	hdr.m_Height = (unsigned short)pImage->m_Height;

	vgui2::g_pFileSystem->Write( &hdr, sizeof( hdr ), fp );

	// Lines are written bottom-up.
	for ( y = pImage->m_Height - 1; y >= 0; y-- )
	{
		pLine = &pImage->m_pData[y * pImage->m_Width * 4];

		runStart = 0;
		for ( x = 0; x < pImage->m_Width; x++ )
		{
			if ( ( x - runStart ) >= 128
				|| memcmp(&pLine[runStart * 4], &pLine[x * 4], cse::image::RgbaChannels) != 0 )
			{
				// Encode this  Run.
				WriteRun( &pLine[runStart * 4], fp, x - runStart );
				runStart = x;
			}
		}

		// Encode the last Run.
		if ( x - runStart > 0 )
		{
			WriteRun( &pLine[runStart * 4], fp, x - runStart );
		}
	}
}
