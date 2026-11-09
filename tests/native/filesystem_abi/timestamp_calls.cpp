#if defined(CSE_FS_PROVIDER_VIEW)
#include "basefilesystem.h"
#define CSE_TIME_CALL ProviderTime
#define CSE_MODIFIED_CALL ProviderModificationTime
#else
#include "public/FileSystem.h"
#define CSE_TIME_CALL ConsumerTime
#define CSE_MODIFIED_CALL ConsumerModificationTime

extern "C" void MountForProbe(void* object, const char* directory) {
    auto* filesystem = static_cast<IFileSystem*>(object);
    filesystem->Mount();
    filesystem->AddSearchPath(directory, "ROOT");
}
extern "C" void UnmountForProbe(void* object) {
    static_cast<IFileSystem*>(object)->Unmount();
}
#endif

extern "C" long long CSE_TIME_CALL(void* object) {
    return static_cast<IFileSystem*>(object)->GetFileTime("time.dat");
}
extern "C" long long CSE_MODIFIED_CALL(void* object) {
    return static_cast<IFileSystem*>(object)->GetFileModificationTime("time.dat");
}
