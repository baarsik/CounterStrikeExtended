#ifndef CSE_BROWSER_FILESYSTEM_FIXTURE_H
#define CSE_BROWSER_FILESYSTEM_FIXTURE_H
#include "FileSystem.h"
#include <map>
#include <stdexcept>
#include <string>
// Narrow mount behavior on the actual retained interface; unrelated operations are forbidden.
struct MountFilesystem final : IFileSystem {
    std::map<std::string, std::string> mounts{{"platform/config", "CONFIG"}};
    int additions = 0;
    int removals = 0;
    void AddSearchPath(const char* path, const char* id) override {
        ++additions;
        mounts[path] = id;
    }
    bool RemoveSearchPath(const char* path) override {
        ++removals;
        return mounts.erase(path) != 0;
    }
    void Mount() override {
        throw std::logic_error("unexpected VFS operation");
    }
    void Unmount() override {
        throw std::logic_error("unexpected VFS operation");
    }
    void RemoveAllSearchPaths() override {
        throw std::logic_error("unexpected VFS operation");
    }
    void RemoveFile([[maybe_unused]] const char *pRelativePath, [[maybe_unused]] const char *pathID) override {
        throw std::logic_error("unexpected VFS operation");
    }
    void CreateDirHierarchy([[maybe_unused]] const char *path, [[maybe_unused]] const char *pathID) override {
        throw std::logic_error("unexpected VFS operation");
    }
    bool FileExists([[maybe_unused]] const char *pFileName) override {
        throw std::logic_error("unexpected VFS operation");
    }
    bool IsDirectory([[maybe_unused]] const char *pFileName) override {
        throw std::logic_error("unexpected VFS operation");
    }
    FileHandle_t Open([[maybe_unused]] const char *pFileName, [[maybe_unused]] const char *pOptions, [[maybe_unused]] const char *pathID = 0L) override {
        throw std::logic_error("unexpected VFS operation");
    }
    void Close([[maybe_unused]] FileHandle_t file) override {
        throw std::logic_error("unexpected VFS operation");
    }
    void Seek([[maybe_unused]] FileHandle_t file, [[maybe_unused]] int pos, [[maybe_unused]] FileSystemSeek_t seekType) override {
        throw std::logic_error("unexpected VFS operation");
    }
    unsigned int Tell([[maybe_unused]] FileHandle_t file) override {
        throw std::logic_error("unexpected VFS operation");
    }
    unsigned int Size([[maybe_unused]] FileHandle_t file) override {
        throw std::logic_error("unexpected VFS operation");
    }
    unsigned int Size([[maybe_unused]] const char *pFileName) override {
        throw std::logic_error("unexpected VFS operation");
    }
    int GetFileTime([[maybe_unused]] const char *pFileName) override {
        throw std::logic_error("unexpected VFS operation");
    }
    void FileTimeToString([[maybe_unused]] char* pStrip, [[maybe_unused]] int maxCharsIncludingTerminator, [[maybe_unused]] long fileTime) override {
        throw std::logic_error("unexpected VFS operation");
    }
    bool IsOk([[maybe_unused]] FileHandle_t file) override {
        throw std::logic_error("unexpected VFS operation");
    }
    void Flush([[maybe_unused]] FileHandle_t file) override {
        throw std::logic_error("unexpected VFS operation");
    }
    bool EndOfFile([[maybe_unused]] FileHandle_t file) override {
        throw std::logic_error("unexpected VFS operation");
    }
    int Read([[maybe_unused]] void* pOutput, [[maybe_unused]] int size, [[maybe_unused]] FileHandle_t file) override {
        throw std::logic_error("unexpected VFS operation");
    }
    int Write([[maybe_unused]] void const* pInput, [[maybe_unused]] int size, [[maybe_unused]] FileHandle_t file) override {
        throw std::logic_error("unexpected VFS operation");
    }
    char *ReadLine([[maybe_unused]] char *pOutput, [[maybe_unused]] int maxChars, [[maybe_unused]] FileHandle_t file) override {
        throw std::logic_error("unexpected VFS operation");
    }
    int FPrintf([[maybe_unused]] FileHandle_t file, [[maybe_unused]] char *pFormat, ...) override {
        throw std::logic_error("unexpected VFS operation");
    }
    void *GetReadBuffer([[maybe_unused]] FileHandle_t file, [[maybe_unused]] int *outBufferSize, [[maybe_unused]] bool failIfNotInCache) override {
        throw std::logic_error("unexpected VFS operation");
    }
    void ReleaseReadBuffer([[maybe_unused]] FileHandle_t file, [[maybe_unused]] void *readBuffer) override {
        throw std::logic_error("unexpected VFS operation");
    }
    const char *FindFirst([[maybe_unused]] const char *pWildCard, [[maybe_unused]] FileFindHandle_t *pHandle, [[maybe_unused]] const char *pathID = 0L) override {
        throw std::logic_error("unexpected VFS operation");
    }
    const char *FindNext([[maybe_unused]] FileFindHandle_t handle) override {
        throw std::logic_error("unexpected VFS operation");
    }
    bool FindIsDirectory([[maybe_unused]] FileFindHandle_t handle) override {
        throw std::logic_error("unexpected VFS operation");
    }
    void FindClose([[maybe_unused]] FileFindHandle_t handle) override {
        throw std::logic_error("unexpected VFS operation");
    }
    void GetLocalCopy([[maybe_unused]] const char *pFileName) override {
        throw std::logic_error("unexpected VFS operation");
    }
    const char *GetLocalPath([[maybe_unused]] const char *pFileName, [[maybe_unused]] char *pLocalPath, [[maybe_unused]] int localPathBufferSize) override {
        throw std::logic_error("unexpected VFS operation");
    }
    char *ParseFile([[maybe_unused]] char* pFileBytes, [[maybe_unused]] char* pToken, [[maybe_unused]] bool* pWasQuoted) override {
        throw std::logic_error("unexpected VFS operation");
    }
    bool FullPathToRelativePath([[maybe_unused]] const char *pFullpath, [[maybe_unused]] char *pRelative) override {
        throw std::logic_error("unexpected VFS operation");
    }
    bool GetCurrentDirectory([[maybe_unused]] char* pDirectory, [[maybe_unused]] int maxlen) override {
        throw std::logic_error("unexpected VFS operation");
    }
    void PrintOpenedFiles() override {
        throw std::logic_error("unexpected VFS operation");
    }
    void SetWarningFunc(void (*)(const char*, ...)) override {
        throw std::logic_error("unexpected VFS operation");
    }
    void SetWarningLevel([[maybe_unused]] FileWarningLevel_t level) override {
        throw std::logic_error("unexpected VFS operation");
    }
    void LogLevelLoadStarted([[maybe_unused]] const char *name) override {
        throw std::logic_error("unexpected VFS operation");
    }
    void LogLevelLoadFinished([[maybe_unused]] const char *name) override {
        throw std::logic_error("unexpected VFS operation");
    }
    int HintResourceNeed([[maybe_unused]] const char *hintlist, [[maybe_unused]] int forgetEverything) override {
        throw std::logic_error("unexpected VFS operation");
    }
    int PauseResourcePreloading() override {
        throw std::logic_error("unexpected VFS operation");
    }
    int ResumeResourcePreloading() override {
        throw std::logic_error("unexpected VFS operation");
    }
    int SetVBuf([[maybe_unused]] FileHandle_t stream, [[maybe_unused]] char *buffer, [[maybe_unused]] int mode, [[maybe_unused]] long size) override {
        throw std::logic_error("unexpected VFS operation");
    }
    void GetInterfaceVersion([[maybe_unused]] char *p, [[maybe_unused]] int maxlen) override {
        throw std::logic_error("unexpected VFS operation");
    }
    bool IsFileImmediatelyAvailable([[maybe_unused]] const char *pFileName) override {
        throw std::logic_error("unexpected VFS operation");
    }
    WaitForResourcesHandle_t WaitForResources([[maybe_unused]] const char *resourcelist) override {
        throw std::logic_error("unexpected VFS operation");
    }
    bool GetWaitForResourcesProgress([[maybe_unused]] WaitForResourcesHandle_t handle, [[maybe_unused]] float *progress, [[maybe_unused]] bool *complete) override {
        throw std::logic_error("unexpected VFS operation");
    }
    void CancelWaitForResources([[maybe_unused]] WaitForResourcesHandle_t handle) override {
        throw std::logic_error("unexpected VFS operation");
    }
    bool IsAppReadyForOfflinePlay([[maybe_unused]] int appID) override {
        throw std::logic_error("unexpected VFS operation");
    }
    bool AddPackFile([[maybe_unused]] const char *fullpath, [[maybe_unused]] const char *pathID) override {
        throw std::logic_error("unexpected VFS operation");
    }
    FileHandle_t OpenFromCacheForRead([[maybe_unused]] const char *pFileName, [[maybe_unused]] const char *pOptions, [[maybe_unused]] const char *pathID = 0L) override {
        throw std::logic_error("unexpected VFS operation");
    }
    void AddSearchPathNoWrite([[maybe_unused]] const char *pPath, [[maybe_unused]] const char *pathID) override {
        throw std::logic_error("unexpected VFS operation");
    }
    int GetFileModificationTime([[maybe_unused]] const char* pFileName) override {
        throw std::logic_error("unexpected VFS operation");
    }
};
#endif
