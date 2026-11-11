#include "public/FileSystem.h"
#include "public/cse_filesystem_abi.h"
#include <cstdio>
#include <cstdlib>
class Filesystem final : public IFileSystem { public:
    void Mount(void) override { if (const char* path = std::getenv("CSE_TEST_EVENTS")) { FILE* f = std::fopen(path, "a"); if (f) { std::fputs("mount\n", f); std::fclose(f); } } }
    void Unmount(void) override { if (const char* path = std::getenv("CSE_TEST_EVENTS")) { FILE* f = std::fopen(path, "a"); if (f) { std::fputs("unmount\n", f); std::fclose(f); } } }
    void RemoveAllSearchPaths( void ) override { std::abort(); }
    void AddSearchPath( const char *pPath, const char *pathID ) override { std::abort(); }
    bool RemoveSearchPath( const char *pPath ) override { std::abort(); }
    void RemoveFile( const char *pRelativePath, const char *pathID ) override { std::abort(); }
    void CreateDirHierarchy( const char *path, const char *pathID ) override { std::abort(); }
    bool FileExists( const char *pFileName ) override { std::abort(); }
    bool IsDirectory( const char *pFileName ) override { std::abort(); }
    FileHandle_t Open( const char *pFileName, const char *pOptions, const char *pathID = 0L ) override { std::abort(); }
    void Close( FileHandle_t file ) override { std::abort(); }
    void Seek( FileHandle_t file, int pos, FileSystemSeek_t seekType ) override { std::abort(); }
    unsigned int Tell( FileHandle_t file ) override { std::abort(); }
    unsigned int Size( FileHandle_t file ) override { std::abort(); }
    unsigned int Size( const char *pFileName ) override { std::abort(); }
    int GetFileTime( const char *pFileName ) override { std::abort(); }
    void FileTimeToString( char* pStrip, int maxCharsIncludingTerminator, long fileTime ) override { std::abort(); }
    bool IsOk( FileHandle_t file ) override { std::abort(); }
    void Flush( FileHandle_t file ) override { std::abort(); }
    bool EndOfFile( FileHandle_t file ) override { std::abort(); }
    int Read( void* pOutput, int size, FileHandle_t file ) override { std::abort(); }
    int Write( void const* pInput, int size, FileHandle_t file ) override { std::abort(); }
    char *ReadLine( char *pOutput, int maxChars, FileHandle_t file ) override { std::abort(); }
    int FPrintf( FileHandle_t file, char *pFormat, ... ) override { std::abort(); }
    void *GetReadBuffer( FileHandle_t file, int *outBufferSize, bool failIfNotInCache ) override { std::abort(); }
    void ReleaseReadBuffer( FileHandle_t file, void *readBuffer ) override { std::abort(); }
    const char *FindFirst( const char *pWildCard, FileFindHandle_t *pHandle, const char *pathID = 0L ) override { std::abort(); }
    const char *FindNext( FileFindHandle_t handle ) override { std::abort(); }
    bool FindIsDirectory( FileFindHandle_t handle ) override { std::abort(); }
    void FindClose( FileFindHandle_t handle ) override { std::abort(); }
    void GetLocalCopy( const char *pFileName ) override { std::abort(); }
    const char *GetLocalPath( const char *pFileName, char *pLocalPath, int localPathBufferSize ) override { std::abort(); }
    char *ParseFile( char* pFileBytes, char* pToken, bool* pWasQuoted ) override { std::abort(); }
    bool FullPathToRelativePath( const char *pFullpath, char *pRelative ) override { std::abort(); }
    bool GetCurrentDirectory( char* pDirectory, int maxlen ) override { std::abort(); }
    void PrintOpenedFiles( void ) override { std::abort(); }
    void SetWarningFunc( void (*pfnWarning)( const char *fmt, ... ) ) override { std::abort(); }
    void SetWarningLevel( FileWarningLevel_t level ) override { std::abort(); }
    void LogLevelLoadStarted( const char *name ) override { std::abort(); }
    void LogLevelLoadFinished( const char *name ) override { std::abort(); }
    int HintResourceNeed( const char *hintlist, int forgetEverything ) override { std::abort(); }
    int PauseResourcePreloading( void ) override { std::abort(); }
    int ResumeResourcePreloading( void ) override { std::abort(); }
    int SetVBuf( FileHandle_t stream, char *buffer, int mode, long size ) override { std::abort(); }
    void GetInterfaceVersion( char *p, int maxlen ) override { std::abort(); }
    bool IsFileImmediatelyAvailable(const char *pFileName) override { std::abort(); }
    WaitForResourcesHandle_t WaitForResources( const char *resourcelist ) override { std::abort(); }
    bool GetWaitForResourcesProgress( WaitForResourcesHandle_t handle, float *progress , bool *complete ) override { std::abort(); }
    void CancelWaitForResources( WaitForResourcesHandle_t handle ) override { std::abort(); }
    bool IsAppReadyForOfflinePlay( int appID ) override { std::abort(); }
    bool AddPackFile( const char *fullpath, const char *pathID ) override { std::abort(); }
    FileHandle_t OpenFromCacheForRead( const char *pFileName, const char *pOptions, const char *pathID = 0L ) override { std::abort(); }
    void AddSearchPathNoWrite( const char *pPath, const char *pathID ) override { std::abort(); }
    int GetFileModificationTime(const char* pFileName) override { std::abort(); }
};
static Filesystem filesystem;
extern "C" __attribute__((visibility("default"))) void* CreateInterface(const char*, int*) { return &filesystem; }
extern "C" __attribute__((visibility("default"))) const cse::abi::Descriptor* CseGetModuleAbi() { return &cse::filesystem_abi::Descriptor; }
