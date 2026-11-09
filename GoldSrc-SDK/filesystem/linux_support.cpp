//========= Copyright © 1996-2001, Valve LLC, All rights reserved. ============
//
// Purpose:
//
// $NoKeywords: $  
//=============================================================================

#include "basefilesystem.h"

#include <cstdlib>
#include <cstring>
#include <cstddef>
#include <cctype>
#ifndef _WIN32
#include <strings.h>
#include <alloca.h>
#endif
#ifndef _WIN32

struct stat;

static char selectBuf[FS_MAX_PATH_LENGTH];
char fileName[FS_MAX_PATH_LENGTH];

namespace {
constexpr int noFindMatches = -1;
constexpr std::intptr_t invalidNativeFindHandle = -1;

struct PosixFindCursor {
    PosixFindCursor() {
        data.numMatches = noFindMatches;
    }

    ~PosixFindCursor() {
        if (data.namelist) {
            for (int index = 0; index <= data.numMatches; ++index) {
                free(data.namelist[index]);
            }
            free(data.namelist);
        }
    }

    FIND_DATA data = {};
};
}


extern "C" const int** __ctype_toupper_loc();
extern "C" const int** __ctype_tolower_loc();
extern "C" char* __strcpy_chk( char* dest, const char* src, size_t destlen );
extern "C" char* __stpcpy_chk( char* dest, const char* src, size_t destlen );
extern "C" int __printf_chk( int flag, const char* format, ... );
extern "C" int __wrap_scandir( const char* dirp, dirent*** namelist,
		int ( *filter )( const dirent* ), int ( *compar )( const dirent**, const dirent** ) );
extern "C" int alphasort( const dirent** a, const dirent** b );

static int UpperByte( unsigned char value )
{
#if defined(__APPLE__)
    return std::toupper(value);
#else
	return ( *__ctype_toupper_loc() )[(signed char)value];
#endif
}

int FileSelect( const dirent* entry )
{
	const char* name = entry->d_name;

	if ( strcmp( name, "." ) == 0 || strcmp( name, ".." ) == 0 )
	{
		return 0;
	}

	if ( strcmp( selectBuf, "*.*" ) == 0 )
	{
		return 1;
	}

	const char* pattern = selectBuf;
	while ( *pattern != 0 )
	{
		if ( *name == 0 )
		{
			return 0;
		}

		if ( *pattern == '*' )
		{
			if ( *++pattern == 0 )
			{
				return 1;
			}

			while ( UpperByte( *name ) != UpperByte( *pattern ) )
			{ // while the two don't meet up again
				if ( *++name == 0 )
				{ // end of the name
					return 0;
				}
			}

			continue;
		}

		if ( *pattern != '?' && UpperByte( *name ) != UpperByte( *pattern ) )
		{ // mismatched!
			return 0;
		}

		++name;
		++pattern;
	}

	return *name == 0;
}

int CheckName( const dirent* entry )
{
	return strcasecmp( entry->d_name, fileName ) == 0;
}

int strlwr( char* text )
{
#if !defined(__APPLE__)
	const int* lowercase = *__ctype_tolower_loc();
#endif

    for (char* cursor = text; *cursor != 0; ++cursor) {
#if defined(__APPLE__)
        *cursor = static_cast<char>(std::tolower(static_cast<unsigned char>(*cursor)));
#else
        *cursor = (char)lowercase[(signed char)*cursor];
#endif
    }

	return 1;
}

int FillDataStruct( FIND_DATA* dat )
{
	int numMatches;
	char szFileName[FS_MAX_PATH_LENGTH];
	char* end;
	dirent* match;
	struct stat fileStat;
	int rc;
	unsigned int mode;

	numMatches = dat->numMatches;
	if ( numMatches < 0 )
	{
		return -1;
	}

	end = __stpcpy_chk( szFileName, dat->cDir, FS_MAX_PATH_LENGTH );
	*end = '/';
	match = dat->namelist[numMatches];

	__strcpy_chk( end + 1, match->d_name, FS_MAX_PATH_LENGTH );
	__strcpy_chk( dat->cFileName, match->d_name, FS_MAX_PATH_LENGTH );

	rc = __wrap___xstat( FS_LINUX_STAT_VERSION, szFileName, &fileStat );
	mode = 0;
	if ( rc == 0 )
	{
		mode = fileStat.st_mode;
	}

	dat->dwFileAttributes = mode;
	//printf("%s\n", dat->namelist[dat->numMatches]->d_name);
	free( dat->namelist[dat->numMatches] );
	--dat->numMatches;
	return 1;
}

static int FindFirstFileInternal( char* fileName, FIND_DATA* dat )
{
	char nameStore[FS_MAX_PATH_LENGTH];
	struct stat dirChk = {};
	char* slash;
	size_t dirLen;
	int count;
	char builtPath[FS_MAX_PATH_LENGTH];
	char* end;
	dirent* match;
	int rc;
	unsigned int mode;

	__strcpy_chk( nameStore, fileName, FS_MAX_PATH_LENGTH );
	slash = strrchr( nameStore, '/' );
	if ( slash == NULL )
	{
		return -1;
	}

	do
	{
		*slash = 0;

		__wrap___xstat( FS_LINUX_STAT_VERSION, nameStore, &dirChk );
		if ( ( dirChk.st_mode & S_IFMT ) == S_IFDIR )
		{
			break;
		}

		slash = strrchr( nameStore, '/' );
	}
	while ( slash != NULL );

	if ( nameStore[0] == 0 )
	{
		return -1;
	}

	dirLen = strlen( nameStore );
	__strcpy_chk( selectBuf, fileName + dirLen + 1, FS_MAX_PATH_LENGTH );
	strcpy( dat->cDir, nameStore );

	count = __wrap_scandir( nameStore, &dat->namelist, FileSelect, alphasort );
	if ( count < 0 )
	{
#if defined(__APPLE__)
        printf("scandir failed:%s\n", nameStore);
#else
		__printf_chk( 1, "scandir failed:%s\n", nameStore );
#endif
		return -1;
	}

	dat->numMatches = count - 1;
	if ( count == 0 )
	{
		free( dat->namelist );
        dat->namelist = nullptr;
		return -1;
	}

	end = __stpcpy_chk( builtPath, dat->cDir, FS_MAX_PATH_LENGTH );
	*end = '/';
	match = dat->namelist[dat->numMatches];
	__strcpy_chk( end + 1, match->d_name, FS_MAX_PATH_LENGTH );
	__strcpy_chk( dat->cFileName, match->d_name, FS_MAX_PATH_LENGTH );

	rc = __wrap___xstat( FS_LINUX_STAT_VERSION, builtPath, &dirChk );
	mode = 0;
	if ( rc == 0 )
	{
		mode = dirChk.st_mode;
	}

	dat->dwFileAttributes = mode;
	free( dat->namelist[dat->numMatches] );
	--dat->numMatches;
	return 1;
}

std::intptr_t FindFirstFile(char* filename, FIND_DATA* data) {
    PosixFindCursor* cursor = new PosixFindCursor;
    if (FindFirstFileInternal(filename, &cursor->data) == noFindMatches) {
        delete cursor;
        return invalidNativeFindHandle;
    }
    *data = cursor->data;
    return reinterpret_cast<std::intptr_t>(cursor);
}

bool FindNextFile(std::intptr_t handle, FIND_DATA* data) {
    PosixFindCursor* cursor = reinterpret_cast<PosixFindCursor*>(handle);
    if (cursor->data.numMatches < 0) {
        return false;
    }
    FillDataStruct(&cursor->data);
    *data = cursor->data;
    return true;
}

bool FindClose(std::intptr_t handle) {
    delete reinterpret_cast<PosixFindCursor*>(handle);
    return true;
}

const char* findFileInDirCaseInsensitive( const char* file )
{
	const char* dirSep;
	char* dirName;
	dirent** namelist = nullptr;
	int n;

	dirSep = strrchr( file, '/' );
	if ( dirSep == NULL )
	{
		dirSep = strrchr( file, '\\' );
		if ( dirSep == NULL )
		{
			return NULL;
		}
	}

	dirName = (char*)alloca( ( dirSep - file ) + 1 );
	strncpy( dirName, file, dirSep - file );
	dirName[dirSep - file] = 0;
	strncpy( fileName, dirSep + 1, FS_MAX_PATH_LENGTH );
	n = __wrap_scandir( dirName, &namelist, CheckName, alphasort );
	if ( n <= 0 )
	{
        free(namelist);
		strncpy( fileName, file, FS_MAX_PATH_LENGTH );
		strlwr( fileName );
		return fileName;
	}

    for (int index = 1; index < n; ++index) {
        free(namelist[index]);
    }

	snprintf( fileName, FS_MAX_PATH_LENGTH, "%s/%s", dirName, namelist[0]->d_name );
	free( namelist[0] );
    free(namelist);
	return fileName;
}

#endif
