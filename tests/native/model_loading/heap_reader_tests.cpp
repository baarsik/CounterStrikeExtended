#include <cassert>
#include <climits>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
using byte = unsigned char;
using FileHandle_t = void*;
struct cache_user_t {
    void* data;
};
static unsigned int fileSize;
static int readBytes;
static int allocations;
static int frees;
static int closes;
static int interceptions;
static cache_user_t* loadcache;
static int loadsize;
static byte* loadbuf;
static void Intercept(char**, int*, int**) {
    ++interceptions;
}
static struct {
    void (*COM_LoadFile)(char**, int*, int**);
} g_engdstAddrs{Intercept};
static FileHandle_t FS_Open(char*, const char*) {
    return reinterpret_cast<void*>(1);
}
static unsigned int FS_Size(FileHandle_t) {
    return fileSize;
}
static void FS_Close(FileHandle_t) {
    ++closes;
}
static int FS_Read(void* out, int bytes, int, FileHandle_t) {
    assert(bytes == static_cast<int>(fileSize));
    std::memset(out, 0x73, readBytes);
    return readBytes;
}
static bool COM_FileBase_s(char*, char* out, int) {
    std::strcpy(out, "fixture");
    return true;
}
static void Sys_Error(const char*, ...) {
    throw std::runtime_error("fatal");
}
static void* Mem_Malloc(int bytes) {
    ++allocations;
    return std::malloc(bytes);
}
static void Mem_Free(void* buffer) {
    ++frees;
    std::free(buffer);
}
static void* Z_Malloc(int bytes) {
    return Mem_Malloc(bytes);
}
static void* Hunk_AllocName(int bytes, const char*) {
    return Mem_Malloc(bytes);
}
static void* Hunk_TempAlloc(int bytes) {
    return Mem_Malloc(bytes);
}
static void* Cache_Alloc(cache_user_t*, int bytes, const char*) {
    return Mem_Malloc(bytes);
}
namespace cse {
    unsigned char* LoadCustomizationFile(char*, int*);
}
#include "heap_reader.inc"
int main() {
    char path[] = "fixture";
    int length = -1;
    fileSize = 4;
    readBytes = 4;
    byte* bytes = COM_LoadFile(path, LOADFILE_HEAP, &length);
    assert(bytes && length == 4 && bytes[3] == 0x73 && bytes[4] == 0);
    assert(interceptions == 1 && allocations == 1 && closes == 1);
    Mem_Free(bytes);
    readBytes = 3;
    length = -1;
    bytes = COM_LoadFile(path, LOADFILE_HEAP, &length);
    assert(bytes == nullptr && length == 0);
    assert(allocations == 2 && frees == 2 && closes == 2 && interceptions == 2);
    fileSize = INT_MAX;
    length = -1;
    bytes = COM_LoadFile(path, LOADFILE_HEAP, &length);
    assert(bytes == nullptr && length == 0 && allocations == 2 && closes == 3);
    fileSize = UINT_MAX;
    bytes = COM_LoadFile(path, LOADFILE_HEAP, &length);
    assert(bytes == nullptr && allocations == 2 && closes == 4);
    std::puts("PASS actual heap reader size, short-read cleanup and interception");
}
