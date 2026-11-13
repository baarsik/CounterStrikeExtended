// Private read projection; every file handle remains owned by the real backend.
class ReadView final : public IFileSystem {
public:
    CBaseFileSystem* owner = nullptr;
    std::vector<CBaseFileSystem::CSearchPath*> paths;
    FileHandle_t Open(const char* name, const char* options, const char* pathId) override {
        if (!name || !options || !std::strchr(options, 'r') || std::strpbrk(options, "w+a")) {
            return nullptr;
        }
        const CUtlSymbol id(pathId ? pathId : "");
        for (const auto* path : paths) {
            if (pathId && path->m_PathID != id) {
                continue;
            }
            if (FileHandle_t file = owner->FindFile(path, name, options, false)) {
                return file;
            }
        }
        return nullptr;
    }

    FileHandle_t OpenFromCacheForRead(const char* name, const char* options, const char* pathId) override {
        return Open(name, options, pathId);
    }

    bool FileExists(const char* name) override {
        FileHandle_t file = Open(name, "rb", nullptr);
        if (file) {
            owner->Close(file);
        }
        return file != nullptr;
    }

    unsigned int Size(const char* name) override {
        FileHandle_t file = Open(name, "rb", nullptr);
        if (!file) {
            return 0;
        }
        const unsigned int size = owner->Size(file);
        owner->Close(file);
        return size;
    }

    unsigned int Size(FileHandle_t file) override {
        return owner->Size(file);
    }

    void Mount() override {}
    void Unmount() override {}
    void RemoveAllSearchPaths() override {}
    void AddSearchPath(const char*, const char*) override {}
    bool RemoveSearchPath(const char*) override {
        return false;
    }

    void RemoveFile(const char*, const char*) override {}
    void CreateDirHierarchy(const char*, const char*) override {}
    bool IsDirectory( const char* pFileName ) override {
        return owner->IsDirectory(pFileName);
    }

    void Close( FileHandle_t file ) override {
        owner->Close(file);
    }

    void Seek( FileHandle_t file, int pos, FileSystemSeek_t whence ) override {
        owner->Seek(file, pos, whence);
    }

    unsigned int Tell( FileHandle_t file ) override {
        return owner->Tell(file);
    }

    int GetFileTime( const char* pFileName ) override {
        return owner->GetFileTime(pFileName);
    }

    void FileTimeToString( char* pStrip, int maxCharsIncludingTerminator, long fileTime ) override {
        owner->FileTimeToString(pStrip, maxCharsIncludingTerminator, fileTime);
    }

    bool IsOk( FileHandle_t file ) override {
        return owner->IsOk(file);
    }

    void Flush( FileHandle_t file ) override {
        owner->Flush(file);
    }

    bool EndOfFile( FileHandle_t file ) override {
        return owner->EndOfFile(file);
    }

    int Read( void* pOutput, int size, FileHandle_t file ) override {
        return owner->Read(pOutput, size, file);
    }

    int Write(const void*, int, FileHandle_t) override {
        return 0;
    }

    char* ReadLine( char* pOutput, int maxChars, FileHandle_t file ) override {
        return owner->ReadLine(pOutput, maxChars, file);
    }

    int FPrintf(FileHandle_t, char*, ...) override {
        return 0;
    }

    void* GetReadBuffer( FileHandle_t file, int* outBufferSize, bool failIfNotInCache ) override {
        return owner->GetReadBuffer(file, outBufferSize, failIfNotInCache);
    }

    void ReleaseReadBuffer( FileHandle_t file, void* readBuffer ) override {
        owner->ReleaseReadBuffer(file, readBuffer);
    }

    const char* FindFirst( const char* pWildCard, FileFindHandle_t* pHandle, const char* pPathID ) override {
        return owner->FindFirst(pWildCard, pHandle, pPathID);
    }

    const char* FindNext( FileFindHandle_t handle ) override {
        return owner->FindNext(handle);
    }

    bool FindIsDirectory( FileFindHandle_t handle ) override {
        return owner->FindIsDirectory(handle);
    }

    void FindClose( FileFindHandle_t handle ) override {
        owner->FindClose(handle);
    }

    void GetLocalCopy(const char*) override {}
    const char* GetLocalPath( const char* pFileName, char* pLocalPath, int localPathBufferSize ) override {
        return owner->GetLocalPath(pFileName, pLocalPath, localPathBufferSize);
    }

    char* ParseFile( char* pFileBytes, char* pToken, bool* pWasQuoted ) override {
        return owner->ParseFile(pFileBytes, pToken, pWasQuoted);
    }

    bool FullPathToRelativePath( const char* pFullpath, char* pRelative ) override {
        return owner->FullPathToRelativePath(pFullpath, pRelative);
    }

    bool GetCurrentDirectory( char* pDirectory, int maxlen ) override {
        return owner->GetCurrentDirectory(pDirectory, maxlen);
    }

    void PrintOpenedFiles() override {
        owner->PrintOpenedFiles();
    }

    void SetWarningFunc( void ( *pfnWarning )( const char*, ... ) ) override {
        owner->SetWarningFunc(pfnWarning);
    }

    void SetWarningLevel( FileWarningLevel_t level ) override {
        owner->SetWarningLevel(level);
    }

    void LogLevelLoadStarted( const char* name ) override {
        owner->LogLevelLoadStarted(name);
    }

    void LogLevelLoadFinished( const char* name ) override {
        owner->LogLevelLoadFinished(name);
    }

    int HintResourceNeed( const char* hintlist, int forgetEverything ) override {
        return owner->HintResourceNeed(hintlist, forgetEverything);
    }

    int PauseResourcePreloading() override {
        return owner->PauseResourcePreloading();
    }

    int ResumeResourcePreloading() override {
        return owner->ResumeResourcePreloading();
    }

    int SetVBuf( FileHandle_t stream, char* buffer, int mode, long size ) override {
        return owner->SetVBuf(stream, buffer, mode, size);
    }

    void GetInterfaceVersion( char* p, int maxlen ) override {
        owner->GetInterfaceVersion(p, maxlen);
    }

    bool IsFileImmediatelyAvailable( const char* pFileName ) override {
        return owner->IsFileImmediatelyAvailable(pFileName);
    }

    WaitForResourcesHandle_t WaitForResources( const char* resourcelist ) override {
        return owner->WaitForResources(resourcelist);
    }

    bool GetWaitForResourcesProgress( WaitForResourcesHandle_t handle, float* progress, bool* complete ) override {
        return owner->GetWaitForResourcesProgress(handle, progress, complete);
    }

    void CancelWaitForResources( WaitForResourcesHandle_t handle ) override {
        owner->CancelWaitForResources(handle);
    }

    bool IsAppReadyForOfflinePlay( int appID ) override {
        return owner->IsAppReadyForOfflinePlay(appID);
    }

    bool AddPackFile(const char*, const char*) override {
        return false;
    }

    void AddSearchPathNoWrite(const char*, const char*) override {}
    int GetFileModificationTime( const char* pFileName ) override {
        return owner->GetFileModificationTime(pFileName);
    }

    FILE* FS_fopen( const char* filename, const char* options, bool bFromCache ) override {
        if (!options || !std::strchr(options, 'r') || std::strpbrk(options, "w+a")) {
            return nullptr;
        }
        return owner->FS_fopen(filename, options, bFromCache);
    }

    void FS_fclose( FILE* fp ) override {
        owner->FS_fclose(fp);
    }

    void FS_fseek( FILE* fp, long long pos, int seekType ) override {
        owner->FS_fseek(fp, pos, seekType);
    }

    int FS_ftell( FILE* fp ) override {
        return owner->FS_ftell(fp);
    }

    int FS_feof( FILE* fp ) override {
        return owner->FS_feof(fp);
    }

    size_t FS_fread( void* dest, unsigned int count, unsigned int size, FILE* fp ) override {
        return owner->FS_fread(dest, count, size, fp);
    }

    size_t FS_fwrite(const void*, unsigned int, unsigned int, FILE*) override {
        return 0;
    }

    size_t FS_vfprintf(FILE*, const char*, va_list) override {
        return 0;
    }

    int FS_ferror( FILE* fp ) override {
        return owner->FS_ferror(fp);
    }

    int FS_fflush( FILE* fp ) override {
        return owner->FS_fflush(fp);
    }

    char* FS_fgets( char* dest, int destSize, FILE* fp ) override {
        return owner->FS_fgets(dest, destSize, fp);
    }

    int FS_stat( const char* path, struct stat* buf ) override {
        return owner->FS_stat(path, buf);
    }

    void* FS_FindFirstFile( char* findname, FIND_DATA* dat ) override {
        return owner->FS_FindFirstFile(findname, dat);
    }

    bool FS_FindNextFile( void* handle, FIND_DATA* dat ) override {
        return owner->FS_FindNextFile(handle, dat);
    }

    bool FS_FindClose( void* handle ) override {
        return owner->FS_FindClose(handle);
    }

    bool IsThreadSafe() override {
        return owner->IsThreadSafe();
    }
};
