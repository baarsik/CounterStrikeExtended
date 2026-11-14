#include "VGUI_FileInputStream.h"
#include "image_codec.h"

#include <stdio.h>
#include <cctype>
#include <cstring>
#include <string>

namespace {
bool IsLegacyImagePath(const char* filename) {
    const char* extension = std::strrchr(filename, '.');
    const char* slash = std::strrchr(filename, '/');
    const char* backslash = std::strrchr(filename, '\\');
    if (!extension || (slash && extension < slash) || (backslash && extension < backslash)) {
        return true;
    }
    return std::strlen(extension) == 4 && std::tolower(static_cast<unsigned char>(extension[1])) == 't'
        && std::tolower(static_cast<unsigned char>(extension[2])) == 'g'
        && std::tolower(static_cast<unsigned char>(extension[3])) == 'a';
}

FILE* OpenReplacement(const char* filename) {
    const std::string path = cse::image::WebPPath(filename);
    FILE* file = nullptr;
#if defined(_MSC_VER)
    if (fopen_s(&file, path.c_str(), "rb") != 0) {
        return nullptr;
    }
#else
    file = fopen(path.c_str(), "rb");
#endif
    if (!file) {
        return NULL;
    }
    bool valid = false;
    if (fseek(file, 0, SEEK_END) == 0) {
        const long size = ftell(file);
        if (size > 0 && static_cast<unsigned long>(size) <= cse::image::MaximumEncodedBytes
            && fseek(file, 0, SEEK_SET) == 0) {
            std::vector<unsigned char> bytes(static_cast<std::size_t>(size));
            cse::image::Image image;
            valid = fread(bytes.data(), 1, bytes.size(), file) == bytes.size()
                && cse::image::DecodeWebP(bytes.data(), bytes.size(), image);
        }
    }
    if (valid && fseek(file, 0, SEEK_SET) == 0) {
        return file;
    }
    fclose(file);
    return NULL;
}
}

using namespace vgui;

FileInputStream::FileInputStream( const char* fileName, bool textMode )
{
#if defined( _MSC_VER )
#pragma warning( push )
#pragma warning( disable : 4996 )
#endif
	if ( textMode )
	{
		_fp = fopen( fileName, "rt" );
	}
	else
	{
        _fp = IsLegacyImagePath(fileName) ? OpenReplacement(fileName) : NULL;
        if (!_fp) {
            _fp = fopen(fileName, "rb");
        }
        if (!_fp && IsLegacyImagePath(fileName)) {
            const std::string webpPath = cse::image::WebPPath(fileName);
            const std::string tgaPath = webpPath.substr(0, webpPath.size() - 5) + ".tga";
            _fp = fopen(tgaPath.c_str(), "rb");
        }
	}
#if defined( _MSC_VER )
#pragma warning( pop )
#endif
}

void FileInputStream::seekStart( bool& success )
{
	if ( _fp == NULL )
	{
		success = false;
		return;
	}

	success = fseek(_fp, 0, SEEK_SET) == 0;
}

void FileInputStream::seekRelative( int count, bool& success )
{
	if ( _fp == NULL )
	{
		success = false;
		return;
	}

	success = fseek(_fp, count, SEEK_CUR) == 0;
}

void FileInputStream::seekEnd( bool& success )
{
	if ( _fp == NULL )
	{
		success = false;
		return;
	}

	success = fseek(_fp, 0, SEEK_END) == 0;
}

int FileInputStream::getAvailable( bool& success )
{
	success = false;
	return 0;
}

uchar FileInputStream::readUChar( bool& success )
{
	if ( _fp == NULL )
	{
		success = false;
		return 0;
	}

	uchar ret;

	success = fread( &ret, 1, 1, _fp ) == 1;
	return ret;
}

void FileInputStream::readUChar( uchar* buf, int count, bool& success )
{
	if ( _fp == NULL )
	{
		success = false;
		return;
	}

	success = fread( buf, count, 1, _fp ) == 1;
}

void FileInputStream::close( bool& success )
{
	if ( _fp == NULL )
	{
		success = false;
		return;
	}

	success = fclose( _fp ) == 0;
}

void FileInputStream::close()
{
	bool success;

	close( success );
}
