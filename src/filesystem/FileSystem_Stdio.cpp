//========= Copyright © 1996-2001, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#include "basefilesystem.h"
#include "../public/cse_filesystem_abi.h"
#include "../public/native_module.h"

#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstring>

#ifndef _WIN32
#include <cerrno>
#include <limits>

// Unconditional warnings must use a declared enum value. All supported
// configured levels are >= QUIET; the legacy -1 cast is outside the enum range.
#undef FILESYSTEM_WARNING
#define FILESYSTEM_WARNING FILESYSTEM_WARNING_QUIET

namespace {
int LegacyFileOffset32(long long value) {
    const std::uint32_t bits = static_cast<std::uint32_t>(value);
    std::int32_t offset;
    static_assert(sizeof(offset) == sizeof(int), "Legacy filesystem offset width");
    std::memcpy(&offset, &bits, sizeof(offset));
    return static_cast<int>(offset);
}
}
#endif

#ifdef _WIN32
#include <io.h>

#define FILESYSTEM_STDIO_SOURCE	L"c:\\buildslave\\goldsrc_win32\\build\\GoldSrc\\filesystem\\FileSystem_Stdio\\FileSystem_Stdio.cpp"
#endif

CFileSystem_Stdio::~CFileSystem_Stdio()
{
    while (m_FindData.Count() > 0) {
        CBaseFileSystem::FindClose(m_FindData.Count() - 1);
    }
	CBaseFileSystem::RemoveAllSearchPaths();
	CBaseFileSystem::Trace_DumpUnclosedFiles();
#ifdef _WIN32
	if ( m_bMounted )
		_wassert( L"!m_bMounted", FILESYSTEM_STDIO_SOURCE, 84 );
#endif
}

void CFileSystem_Stdio::CancelWaitForResources( int )
{
}

bool CFileSystem_Stdio::IsAppReadyForOfflinePlay( int )
{
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: low-level filesystem wrapper
//-----------------------------------------------------------------------------
void CFileSystem_Stdio::FS_fclose( FILE* fp )
{
	fclose( fp );
}

void CFileSystem_Stdio::FS_fseek( FILE* fp, long long pos, int seekType )
{
#ifdef _WIN32
	int ret = fseek( fp, (long)pos, seekType );
#else
    const int legacyPosition = LegacyFileOffset32(pos);
    const int ret = fseek(fp, static_cast<long>(legacyPosition), seekType);
#endif
#ifdef _WIN32
	if ( ret != 0 )
		_wassert( L"ret == 0", FILESYSTEM_STDIO_SOURCE, 147 );
#else
	(void)ret;
#endif
}

int CFileSystem_Stdio::FS_ftell( FILE* fp )
{
#ifdef _WIN32
	return ftell( fp );
#else
    const long position = ftell(fp);
    constexpr long kMaximumLegacyPosition = (std::numeric_limits<int>::max)();
    constexpr long kFileError = -1;
    if (position < kFileError || position > kMaximumLegacyPosition) {
        errno = EINVAL;
        return -1;
    }
    return static_cast<int>(position);
#endif
}

//-----------------------------------------------------------------------------
// Purpose: low-level filesystem wrapper
//-----------------------------------------------------------------------------
int CFileSystem_Stdio::FS_feof( FILE* fp )
{
	return feof( fp );
}

size_t CFileSystem_Stdio::FS_fread( void* dest, unsigned int count, unsigned int size, FILE* fp )
{
#ifdef _WIN32
	return read( fileno( fp ), dest, size * count );
#else
	return fread( dest, count, size, fp );
#endif
}

//-----------------------------------------------------------------------------
// Purpose: low-level filesystem wrapper
//-----------------------------------------------------------------------------
size_t CFileSystem_Stdio::FS_fwrite(const void* src, unsigned int count, unsigned int size, FILE* fp) {
    return fwrite(src, count, size, fp);
}

//-----------------------------------------------------------------------------
// Purpose: low-level filesystem wrapper
//-----------------------------------------------------------------------------
size_t CFileSystem_Stdio::FS_vfprintf( FILE* fp, const char* fmt, va_list list )
{
	return vfprintf( fp, fmt, list );
}

//-----------------------------------------------------------------------------
// Purpose: low-level filesystem wrapper
//-----------------------------------------------------------------------------
int CFileSystem_Stdio::FS_ferror( FILE* fp )
{
	return ferror( fp );
}

//-----------------------------------------------------------------------------
// Purpose: low-level filesystem wrapper
//-----------------------------------------------------------------------------
int CFileSystem_Stdio::FS_fflush( FILE* fp )
{
	return fflush( fp );
}

//-----------------------------------------------------------------------------
// Purpose: low-level filesystem wrapper
//-----------------------------------------------------------------------------
char* CFileSystem_Stdio::FS_fgets( char* dest, int destSize, FILE* fp )
{
	return fgets( dest, destSize, fp );
}

int CFileSystem_Stdio::FS_stat( const char* path, struct stat* buf )
{
#ifdef _WIN32
	return stat( path, buf );
#else
    // The retained Windows stat API uses a signed32 size and clears output on
    // failure. Keep native off_t/time_t storage until the representability check.
    if (!buf) {
        errno = EINVAL;
        return -1;
    }
    std::memset(buf, 0, sizeof(*buf));
    struct stat nativeStat{};
    int status = __wrap___xstat(FS_LINUX_STAT_VERSION, path, &nativeStat);
    if (status == -1) {
        const int originalError = errno;
        const char* matched = findFileInDirCaseInsensitive(path);
        if (matched) {
            status = __wrap___xstat(FS_LINUX_STAT_VERSION, matched, &nativeStat);
        } else {
            errno = originalError;
        }
    }
    if (status == -1) {
        return -1;
    }
    constexpr auto kMaximumLegacySize = (std::numeric_limits<int>::max)();
    if (nativeStat.st_size < 0 || nativeStat.st_size > kMaximumLegacySize) {
        errno = EOVERFLOW;
        return -1;
    }
    *buf = nativeStat;
    return status;
#endif
}

HANDLE CFileSystem_Stdio::FS_FindFirstFile(char* findname, FIND_DATA* dat) {
#ifdef _WIN32
    return ::FindFirstFileA(findname, dat);
#else
    return reinterpret_cast<HANDLE>(::FindFirstFile(findname, dat));
#endif
}

bool CFileSystem_Stdio::FS_FindNextFile(HANDLE handle, FIND_DATA* dat) {
#ifdef _WIN32
    return ::FindNextFileA(handle, dat) != FALSE;
#else
    return ::FindNextFile(reinterpret_cast<std::intptr_t>(handle), dat);
#endif
}

bool CFileSystem_Stdio::FS_FindClose(HANDLE handle) {
#ifdef _WIN32
    return ::FindClose(handle) != FALSE;
#else
    return ::FindClose(reinterpret_cast<std::intptr_t>(handle));
#endif
}

void CFileSystem_Stdio::GetInterfaceVersion( char* p, int maxlen )
{
	strncpy( p, "Stdio", maxlen );
}

FILE* CFileSystem_Stdio::FS_fopen( const char* filename, const char* options, bool bFromCache )
{
#ifdef _WIN32
	(void)bFromCache;
	if ( strchr( filename, '\n' ) || strchr( filename, '\r' ) )
		_wassert( L"!strchr(filename, '\\n') && !strchr(filename, '\\r')", FILESYSTEM_STDIO_SOURCE, 115 );
	return fopen( filename, options );
#else
	FILE* opened;

	(void)bFromCache;
	opened = __wrap_fopen( filename, options );
	if ( opened != NULL || strchr( options, 'w' ) != NULL || strchr( options, '+' ) != NULL )
	{
		return opened;
	}

	return __wrap_fopen( findFileInDirCaseInsensitive( filename ), options );
#endif
}

int CFileSystem_Stdio::SetVBuf( FileHandle_t stream, char* buffer, int mode, long size )
{
	CBaseFileSystem::CFileHandle* opened;

	if ( stream != NULL )
	{
		opened = (CBaseFileSystem::CFileHandle*)stream;
		return setvbuf( opened->m_pFile, buffer, mode, size );
	}

	CBaseFileSystem::Warning( (FileWarningLevel_t)-1, "FS:  Tried to SetVBuf NULL file handle!\n" );
	return 0;
}

CFileSystem_Stdio::CFileSystem_Stdio()
	: CBaseFileSystem()
{
	m_bMounted = false;
}

void CFileSystem_Stdio::Mount()
{
	CBaseFileSystem::Mount();
#ifdef _WIN32
	if ( m_bMounted )
		_wassert( L"!m_bMounted", FILESYSTEM_STDIO_SOURCE, 93 );
#endif
	m_bMounted = true;
}

void CFileSystem_Stdio::Unmount()
{
#ifdef _WIN32
	if ( !m_bMounted )
		_wassert( L"m_bMounted", FILESYSTEM_STDIO_SOURCE, 102 );
#endif
	m_bMounted = false;
	CBaseFileSystem::Unmount();
}

#if defined(_WIN32)
#define CSE_FILESYSTEM_EXPORT __declspec(dllexport)
#else
#define CSE_FILESYSTEM_EXPORT __attribute__((visibility("default")))
#endif

extern "C" CSE_FILESYSTEM_EXPORT const cse::abi::Descriptor* CSE_ABI_CALL CseGetModuleAbi() {
    return &cse::filesystem_abi::Descriptor;
}

#undef CSE_FILESYSTEM_EXPORT

extern "C" void* CreateInterface( const char* pName, int* pReturnCode )
{
	return InterfaceReg::Create( pName, pReturnCode );
}

void* GetModuleHandle(const char* name) {
    return cse_platform::FindLoadedModule(name, cse_platform::ModuleNullPolicy::MainExecutable);
}

static void* GetFileSystemInstance()
{
	return &g_FileSystem_Stdio;
}

CFileSystem_Stdio	g_FileSystem_Stdio;
static InterfaceReg	g_FileSystemReg( GetFileSystemInstance, "VFileSystem009" );

bool CFileSystem_Stdio::GetWaitForResourcesProgress(
		WaitForResourcesHandle_t handle, float* progress, bool* complete )
{
	(void)handle;
	if ( progress != NULL )
	{
		*progress = 0.0f;
	}

	if ( complete != NULL )
	{
		*complete = true;
	}

	return false;
}

bool CFileSystem_Stdio::IsThreadSafe()
{
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: files are always immediately available on disk
//-----------------------------------------------------------------------------
bool CFileSystem_Stdio::IsFileImmediatelyAvailable( char const* )
{
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pFileName - 
//-----------------------------------------------------------------------------
void CFileSystem_Stdio::GetLocalCopy( char const* )
{
}

int CFileSystem_Stdio::HintResourceNeed( char const*, int )
{
	return 0;
}

int CFileSystem_Stdio::PauseResourcePreloading()
{
	return 0;
}

int CFileSystem_Stdio::WaitForResources( char const* )
{
	return 0;
}
