#include "hashpak.h"
#include "common.h"
#include "conprint.h"
#include "mem.h"
#include "sys.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iterator>
#include <vector>

namespace {
std::vector<byte> fileBytes;
std::size_t position = 0;
bool open = false;
constexpr int ExpectedPayloadBytes = 1024;
constexpr int HeaderBytes = 12;

void Require(bool condition) {
    if (!condition) {
        std::abort();
    }
}
}

FileHandle_t FS_Open(const char* path, const char* options) {
    Require(!open && std::strcmp(path, "fixture.hpk") == 0 && std::strcmp(options, "rb") == 0);
    open = true;
    position = 0;
    return &fileBytes;
}
void FS_Close(FileHandle_t file) {
    Require(open && file == &fileBytes);
    open = false;
}
int FS_Read(void* output, int size, int count, FileHandle_t file) {
    Require(open && file == &fileBytes && output && size > 0 && count > 0);
    const std::size_t requested = static_cast<std::size_t>(size) * count;
    const std::size_t copied = (std::min)(requested, fileBytes.size() - position);
    std::memcpy(output, fileBytes.data() + position, copied);
    position += copied;
    return static_cast<int>(copied);
}
void FS_Seek(FileHandle_t file, int offset, FileSystemSeek_t origin) {
    Require(open && file == &fileBytes && origin == FILESYSTEM_SEEK_HEAD && offset >= 0);
    Require(static_cast<std::size_t>(offset) <= fileBytes.size());
    position = static_cast<std::size_t>(offset);
}
unsigned int FS_Tell(FileHandle_t file) {
    Require(open && file == &fileBytes);
    return static_cast<unsigned int>(position);
}
unsigned int FS_Size(FileHandle_t file) {
    Require(open && file == &fileBytes);
    return static_cast<unsigned int>(fileBytes.size());
}
void* Mem_Malloc(std::size_t bytes) {
    return std::malloc(bytes);
}
void* Mem_ZeroMalloc(std::size_t bytes) {
    return std::calloc(1, bytes);
}
void Mem_Free(void* memory) {
    std::free(memory);
}
int Q_memcmp(const void* first, const void* second, int bytes) {
    Require(bytes >= 0);
    return std::memcmp(first, second, static_cast<std::size_t>(bytes));
}
void Q_memcpy(void* output, const void* input, int bytes) {
    Require(bytes >= 0);
    std::memcpy(output, input, static_cast<std::size_t>(bytes));
}
int Q_strncmp(const char* first, const char* second, int bytes) {
    Require(bytes >= 0);
    return std::strncmp(first, second, static_cast<std::size_t>(bytes));
}
int Q_strcasecmp(const char* first, const char* second) {
    return std::strcmp(first, second); // The authored queue is empty; this path is never called.
}
void COM_DefaultExtension(char* path, char* extension) {
    Require(std::strcmp(path, "fixture") == 0 && std::strcmp(extension, ".hpk") == 0);
    std::strcat(path, extension);
}
void Con_Printf(const char*, ...) {
}
[[noreturn]] void Sys_Error(const char*, ...) {
    std::abort();
}

int main(int argc, char** argv) {
    Require(argc == 2);
    std::ifstream input(argv[1], std::ios::binary);
    fileBytes.assign(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
    Require(fileBytes.size() == 1184);
    resource_t resource{};
    // The same authored digest is in the Win32 record after the 1024-byte payload.
    constexpr std::size_t DigestOffsetInRecord = 77;
    constexpr std::size_t DirectoryCountBytes = 4;
    const std::size_t digestOffset = HeaderBytes + ExpectedPayloadBytes + DirectoryCountBytes + DigestOffsetInRecord;
    std::memcpy(resource.rgucMD5_hash, fileBytes.data() + digestOffset, sizeof(resource.rgucMD5_hash));
    byte* result = nullptr;
    int bytes = 0;
    const qboolean found = HPAK_GetDataPointer("fixture", &resource, &result, &bytes);
    const bool valid = found && result && bytes == ExpectedPayloadBytes
        && std::memcmp(result, fileBytes.data() + HeaderBytes, ExpectedPayloadBytes) == 0 && !open;
    Mem_Free(result);
    std::printf("resource_storage=%zu, found=%d, payload=%d, exact=%d\n", sizeof(resource_t), found, bytes, valid);
    return valid ? 0 : 17;
}
