#include "launcher/public/filesystem.h"
#include <cstdlib>

// Only search-path setup is used; all real filesystem operations fail the fixture.
class FixtureFileSystem : public IFileSystem {
public:
    void Mount() override {
        std::abort();
    }
    void Unmount() override {
        std::abort();
    }
    void RemoveAllSearchPaths() override {
    }
    void AddSearchPath(const char* path, const char* pathID) override {
    }
    bool RemoveSearchPath(const char* path) override {
        std::abort();
    }
    void RemoveFile(const char* filename, const char* pathID) override {
        std::abort();
    }
    void CreateDirHierarchy(const char* path, const char* pathID) override {
        std::abort();
    }
    bool FileExists(const char* filename) override {
        std::abort();
    }
    bool IsDirectory(const char* filename) override {
        std::abort();
    }
    FileHandle_t Open(const char* filename, const char* options, const char* pathID = NULL) override {
        std::abort();
    }
    void Close(FileHandle_t file) override {
        std::abort();
    }
    void Seek(FileHandle_t file, int position, FileSystemSeek_t origin) override {
        std::abort();
    }
    unsigned int Tell(FileHandle_t file) override {
        std::abort();
    }
    unsigned int Size(FileHandle_t file) override {
        std::abort();
    }
    unsigned int Size(const char* filename) override {
        std::abort();
    }
    int GetFileTime(const char* filename) override {
        std::abort();
    }
    void FileTimeToString(char* buffer, int bufferSize, long fileTime) override {
        std::abort();
    }
    bool IsOk(FileHandle_t file) override {
        std::abort();
    }
    void Flush(FileHandle_t file) override {
        std::abort();
    }
    bool EndOfFile(FileHandle_t file) override {
        std::abort();
    }
    int Read(void* buffer, int size, FileHandle_t file) override {
        std::abort();
    }
    int Write(const void* buffer, int size, FileHandle_t file) override {
        std::abort();
    }
    char* ReadLine(char* buffer, int bufferSize, FileHandle_t file) override {
        std::abort();
    }
    int FPrintf(FileHandle_t file, char* format, ...) override {
        std::abort();
    }
    void* GetReadBuffer(FileHandle_t file, int* bufferSize, bool failIfNotInCache) override {
        std::abort();
    }
    void ReleaseReadBuffer(FileHandle_t file, void* buffer) override {
        std::abort();
    }
    const char* FindFirst(const char* wildcard, FileFindHandle_t* handle, const char* pathID = NULL) override {
        std::abort();
    }
    const char* FindNext(FileFindHandle_t handle) override {
        std::abort();
    }
    bool FindIsDirectory(FileFindHandle_t handle) override {
        std::abort();
    }
    void FindClose(FileFindHandle_t handle) override {
        std::abort();
    }
    void GetLocalCopy(const char* filename) override {
        std::abort();
    }
    const char* GetLocalPath(const char* filename, char* localPath, int bufferSize) override {
        std::abort();
    }
    char* ParseFile(char* fileBytes, char* token, bool* wasQuoted) override {
        std::abort();
    }
    bool FullPathToRelativePath(const char* fullPath, char* relativePath) override {
        std::abort();
    }
    bool GetCurrentDirectory(char* directory, int bufferSize) override {
        std::abort();
    }
    void PrintOpenedFiles() override {
        std::abort();
    }
    void SetWarningFunc(void (*warning)(const char* format, ...)) override {
        std::abort();
    }
    void SetWarningLevel(FileWarningLevel_t level) override {
        std::abort();
    }
    void LogLevelLoadStarted(const char* name) override {
        std::abort();
    }
    void LogLevelLoadFinished(const char* name) override {
        std::abort();
    }
    int HintResourceNeed(const char* resources, int forgetEverything) override {
        std::abort();
    }
    int PauseResourcePreloading() override {
        std::abort();
    }
    int ResumeResourcePreloading() override {
        std::abort();
    }
    int SetVBuf(FileHandle_t file, char* buffer, int mode, long size) override {
        std::abort();
    }
    void GetInterfaceVersion(char* buffer, int bufferSize) override {
        std::abort();
    }
    bool IsFileImmediatelyAvailable(const char* filename) override {
        std::abort();
    }
    WaitForResourcesHandle_t WaitForResources(const char* resources) override {
        std::abort();
    }
    bool GetWaitForResourcesProgress(WaitForResourcesHandle_t handle, float* progress, bool* complete) override {
        std::abort();
    }
    void CancelWaitForResources(WaitForResourcesHandle_t handle) override {
        std::abort();
    }
    bool IsAppReadyForOfflinePlay(int appID) override {
        std::abort();
    }
    bool AddPackFile(const char* fullPath, const char* pathID) override {
        std::abort();
    }
    FileHandle_t OpenFromCacheForRead(const char* filename, const char* options, const char* pathID = NULL) override {
        std::abort();
    }
    void AddSearchPathNoWrite(const char* path, const char* pathID) override {
        std::abort();
    }
    int GetFileModificationTime(const char* filename) override {
        std::abort();
    }
};

IFileSystem *ConnectedFixtureFileSystem() {
    static FixtureFileSystem filesystem;
    return &filesystem;
}
