//========= Copyright © 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#include "../cl_dll/wrect.h"
#include "../cl_dll/cl_dll.h"
#include "VGUI.h"
#include "vgui_loadtga.h"
#include "VGUI_InputStream.h"
#include "image_codec.h"


// ---------------------------------------------------------------------- //
// Helper class for loading tga files.
// ---------------------------------------------------------------------- //
class MemoryInputStream : public vgui::InputStream
{
public:
				MemoryInputStream()
				{
					m_pData = NULL;
					m_DataLen = m_ReadPos = 0;
				}
		
	virtual void  seekStart(bool& success)					{m_ReadPos=0; success=true;}
	virtual void  seekRelative(int count,bool& success)		{m_ReadPos+=count; success=true;}
	virtual void  seekEnd(bool& success)					{m_ReadPos=m_DataLen; success=true;}
	virtual int   getAvailable(bool& success)				{success=false; return 0;} // This is what vgui does for files...
	
	virtual uchar readUChar(bool& success)					
	{
		if(m_ReadPos>=0 && m_ReadPos<m_DataLen)
		{
			success=true;
			uchar ret = m_pData[m_ReadPos];
			++m_ReadPos;
			return ret;
		}
		else
		{
			success=false;
			return 0;
		}
	}

	virtual void  readUChar(uchar* buf,int count,bool& success)
	{
		for(int i=0; i < count; i++)
			buf[i] = readUChar(success);
	}

	virtual void  close(bool&) 
	{
		m_pData = NULL;
		m_DataLen = m_ReadPos = 0;
	}

	uchar		*m_pData;
	int			m_DataLen;
	int			m_ReadPos;
};

namespace {
vgui::BitmapTGA* LoadBitmap(const char* filename, bool invertAlpha) {
    MemoryInputStream stream;
    stream.m_pData = gEngfuncs.COM_LoadFile(const_cast<char*>(filename), 5, &stream.m_DataLen);
    if (!stream.m_pData) {
        return NULL;
    }
    vgui::BitmapTGA* bitmap = new vgui::BitmapTGA(&stream, invertAlpha);
    gEngfuncs.COM_FreeFile(stream.m_pData);
    int width = 0;
    int height = 0;
    bitmap->getSize(width, height);
    if (width <= 0 || height <= 0) {
        delete bitmap;
        return NULL;
    }
    return bitmap;
}

vgui::BitmapTGA* LoadBitmapWithReplacement(const char* filename, bool invertAlpha) {
    if (!filename || !filename[0]) {
        return NULL;
    }
    if (cse::image::HasWebPExtension(filename)) {
        return LoadBitmap(filename, invertAlpha);
    }
    const std::string webpPath = cse::image::WebPPath(filename);
    vgui::BitmapTGA* bitmap = LoadBitmap(webpPath.c_str(), invertAlpha);
    if (bitmap) {
        return bitmap;
    }
    bitmap = LoadBitmap(filename, invertAlpha);
    if (bitmap) {
        return bitmap;
    }
    // Legacy callers also pass extensionless image names.
    const std::string tgaPath = webpPath.substr(0, webpPath.size() - 5) + ".tga";
    return tgaPath == filename ? NULL : LoadBitmap(tgaPath.c_str(), invertAlpha);
}
}

vgui::BitmapTGA* vgui_LoadTGA(const char* filename) {
    return LoadBitmapWithReplacement(filename, true);
}

vgui::BitmapTGA* vgui_LoadTGANoInvertAlpha(const char* filename) {
    return LoadBitmapWithReplacement(filename, false);
}
