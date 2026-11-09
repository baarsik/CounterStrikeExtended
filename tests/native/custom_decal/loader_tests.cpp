#include "common.h"
#if INTPTR_MAX > INT32_MAX
#include "custom_decal_loader.h"
#endif
#include "engine_apiproxy.h"
#include "zone.h"
#include "mem.h"
#include "sys.h"
#include <array>
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace {
constexpr int Failure = 17;
constexpr int HeapMode = 5;
std::vector<byte> sourceBytes;
std::array<byte, 64> otherStorage{};
std::string trace;
std::string expectedPath;
bool fileOpen = false;
bool shortRead = false;
bool openFails = false;
bool expectNullLength = false;
bool rewritePath = false;
bool rewriteLength = false;
bool lengthAliasesMode = false;
int rewrittenMode = -1;
int shadowLength = 0;
unsigned logicalBytes = 0;
unsigned hooks = 0;
unsigned frees = 0;
unsigned heapAllocations = 0;
int allocationMode = -1;

void Check(bool value, const char* message) {
    if (!value) {
        std::fprintf(stderr, "custom-loader:%s\n", message);
        std::exit(Failure);
    }
}

void Reset() {
    Check(!fileOpen, "prior file closed");
    sourceBytes = {1, 2, 3, 4, 5, 6, 7, 8};
    otherStorage.fill(0xa5);
    trace.clear();
    expectedPath = "tempdecal.wad";
    shortRead = false;
    openFails = false;
    expectNullLength = false;
    rewritePath = false;
    rewriteLength = false;
    lengthAliasesMode = false;
    rewrittenMode = -1;
    shadowLength = 77;
    logicalBytes = static_cast<unsigned>(sourceBytes.size());
    hooks = 0;
    frees = 0;
    heapAllocations = 0;
    allocationMode = -1;
}

void Hook(char** path, int* mode, int** length) {
    Check(std::strcmp(*path, "tempdecal.wad") == 0, "hook receives original path");
    Check(expectNullLength ? *length == nullptr : *length != nullptr && **length == -93,
          "original length argument observed before reset");
    ++hooks;
    trace += 'H';
    if (rewritePath) {
        *path = const_cast<char*>("redirected.wad");
    }
    if (rewrittenMode >= 0) {
        *mode = rewrittenMode;
    }
    if (rewriteLength) {
        *length = &shadowLength;
    }
    if (lengthAliasesMode) {
        *length = mode;
    }
}

void* OtherAllocation(int mode, int size) {
    Check(size > 0 && size <= static_cast<int>(otherStorage.size()), "bounded nonheap fixture allocation");
    allocationMode = mode;
    trace += 'A';
    return otherStorage.data();
}
} // namespace

decltype(g_engdstAddrs) g_engdstAddrs{};
cache_user_t* loadcache = nullptr;
byte* loadbuf = nullptr;
int loadsize = 0;

FileHandle_t FS_Open(const char* name, const char* mode) {
    Check(!fileOpen && name == expectedPath && std::strcmp(mode, "rb") == 0, "open rewritten path once");
    Check(!rewriteLength || shadowLength == 0, "redirected length reset after hook before open");
    trace += 'O';
    fileOpen = !openFails;
    return fileOpen ? &sourceBytes : nullptr;
}

unsigned int FS_Size(FileHandle_t file) {
    Check(fileOpen && file == &sourceBytes, "size owns handle");
    trace += 'S';
    return logicalBytes;
}

int FS_Read(void* output, int size, int count, FileHandle_t file) {
    Check(fileOpen && file == &sourceBytes && count == 1 && size == static_cast<int>(sourceBytes.size()),
          "read complete file byte request");
    trace += 'R';
    const int bytes = size - (shortRead && size ? 1 : 0);
    if (bytes) {
        std::memcpy(output, sourceBytes.data(), static_cast<std::size_t>(bytes));
    }
    return bytes;
}

void FS_Close(FileHandle_t file) {
    Check(fileOpen && file == &sourceBytes, "close owned file once");
    trace += 'C';
    fileOpen = false;
}

void* Mem_Malloc(std::size_t size) {
    ++heapAllocations;
    allocationMode = HeapMode;
    trace += 'A';
    return std::malloc(size);
}

void Mem_Free(void* memory) {
    Check(memory != otherStorage.data(), "never invent ownership of a nonheap result");
    ++frees;
    std::free(memory);
}

char* COM_FileBase_s(const char*, char* output, int capacity) {
    Check(capacity >= 11, "bounded tag buffer");
    trace += 'B';
    std::strcpy(output, "tempdecal");
    return output;
}

void* Z_Malloc(int size) {
    return OtherAllocation(0, size);
}

void* Hunk_AllocName(int size, const char*) {
    return OtherAllocation(1, size);
}

void* Hunk_TempAlloc(int size) {
    return OtherAllocation(2, size);
}

void* Cache_Alloc(cache_user_t*, int size, const char*) {
    return OtherAllocation(3, size);
}

[[noreturn]] void Sys_Error(const char*, ...) {
    std::abort();
}

#include "loader.inc"

int main() {
    g_engdstAddrs.COM_LoadFile = Hook;
    char path[] = "tempdecal.wad";
    Reset();
    shortRead = true;
    int length = -93;
    byte* output = COM_LoadFile(path, HeapMode, &length);
    const bool rejected = output == nullptr;
    if (output) {
        Mem_Free(output);
    }
#if INTPTR_MAX > INT32_MAX
    Check(rejected, "short native heap read must not return uninitialized file tail");
    Check(length == 0 && !fileOpen && hooks == 1 && frees == 1, "failed read cleanup and callback order");
#else
    // Characterize the retained Win32 status without reading its uninitialized tail.
    Check(!rejected && length == 8 && !fileOpen && hooks == 1 && frees == 1,
          "legacy Win32 short-read result remains unchanged");
#endif
    Check(trace == "HOSBARC", "ordinary loader successful-stage ordering retained");

    Reset();
    length = -93;
    output = COM_LoadFile(path, HeapMode, &length);
    Check(output && length == 8 && std::memcmp(output, sourceBytes.data(), sourceBytes.size()) == 0 && output[8] == 0,
          "full heap load retains successful bytes and trailing NUL");
    Mem_Free(output);
    Check(trace == "HOSBARC" && hooks == 1 && frees == 1, "full read callback order");

#if INTPTR_MAX > INT32_MAX
    for (bool redirectLength : {false, true}) {
        Reset();
        expectNullLength = true;
        rewriteLength = redirectLength;
        rewritePath = true;
        expectedPath = "redirected.wad";
        int actual = -93;
        output = cse::LoadCustomizationFile(path, &actual);
        Check(output && actual == 8 && (!redirectLength || shadowLength == 8),
              "independent actual extent survives hook length redirection");
        Check(std::strcmp(path, "tempdecal.wad") == 0 && hooks == 1 && trace == "HOSBARC",
              "strict callback runs once with original null length and rewritten path");
        Mem_Free(output);
    }
    for (int mode = 0; mode < HeapMode; ++mode) {
        Reset();
        expectNullLength = true;
        rewrittenMode = mode;
        int actual = -93;
        output = cse::LoadCustomizationFile(path, &actual);
        Check(!output && actual == 0 && trace == "H" && hooks == 1 && heapAllocations == 0 && frees == 0,
              "strict customization refuses nonheap rewrite before open or allocation");
    }
    Reset();
    expectNullLength = true;
    lengthAliasesMode = true;
    int actual = -93;
    Check(!cse::LoadCustomizationFile(path, &actual) && actual == 0 && trace == "H",
          "length output alias changing mode is rechecked before ownership admission");
    for (unsigned size : {static_cast<unsigned>(INT_MAX), UINT_MAX}) {
        Reset();
        logicalBytes = size;
        length = -93;
        Check(!COM_LoadFile(path, HeapMode, &length) && length == 0 && trace == "HOSC" && heapAllocations == 0,
              "unrepresentable heap length rejects before narrowing/allocation");
    }
    Reset();
    expectNullLength = true;
    rewriteLength = true;
    shortRead = true;
    actual = -93;
    Check(!cse::LoadCustomizationFile(path, &actual) && actual == 0 && shadowLength == 0 && frees == 1,
          "redirected lengths cannot hide failed actual read");
    Reset();
    Check(!cse::LoadCustomizationFile(path, nullptr) && hooks == 0, "invalid actual-output pointer rejected");
#endif

    for (int mode = 0; mode < HeapMode; ++mode) {
        Reset();
        rewrittenMode = mode;
        shortRead = true;
        length = -93;
        loadbuf = otherStorage.data();
        loadsize = static_cast<int>(otherStorage.size());
        output = COM_LoadFile(path, HeapMode, &length);
        Check(output == otherStorage.data() && length == 8 && frees == 0 && heapAllocations == 0 && hooks == 1,
              "ordinary hook-selected nonheap mode behavior retained");
        Check(mode == 4 ? allocationMode == -1 : allocationMode == mode, "correct original allocator selected");
    }
    Reset();
    openFails = true;
    length = -93;
    Check(!COM_LoadFile(path, HeapMode, &length) && length == 0 && hooks == 1 && trace == "HO",
          "open failure output and hook order");
    std::puts("loader: heap extent, hook path/mode/length, nonheap ownership and byte failure cases pass");
    return 0;
}
