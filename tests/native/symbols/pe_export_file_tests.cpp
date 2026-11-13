#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <algorithm>
#include <climits>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include "pe_test_image.h"

namespace {
std::vector<char> allocation;
bool failAllocation = false;
unsigned int allocations = 0;
unsigned int localCopies = 0;
unsigned int diagnostics = 0;

void FS_GetLocalCopy(const char *) {
    ++localCopies;
}

void *Hunk_TempAlloc(int bytes) {
    ++allocations;
    if (failAllocation) {
        return nullptr;
    }
    allocation.assign(static_cast<std::size_t>(bytes), 'x');
    return allocation.data();
}

void Con_DPrintf(const char *, ...) {
    ++diagnostics;
}
} // namespace

#include "../../../src/engine/pe_export_file_impl.h"

namespace {
bool WriteImage(const char *path, const std::vector<unsigned char> &image) {
    const HANDLE file = CreateFileA(path, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        return false;
    }
    DWORD written = 0;
    const bool ok = WriteFile(file, image.data(), static_cast<DWORD>(image.size()), &written, nullptr)
        && written == image.size();
    CloseHandle(file);
    return ok;
}

bool CheckFileBoundary(const char *path) {
    const char expected[] = "?Think@OwnedEntity@@QEAAXXZ";
    for (const bool pe64 : {false, true}) {
        if (!WriteImage(path, fixture::MakeImage(pe64))) {
            return false;
        }
        char *names = nullptr;
        if (GetExportNames(path, &names) != sizeof(expected) || !names
            || std::memcmp(names, expected, sizeof(expected)) != 0) {
            return false;
        }
        failAllocation = true;
        names = reinterpret_cast<char *>(static_cast<std::uintptr_t>(1));
        const auto failedBytes = GetExportNames(path, &names);
        failAllocation = false;
        if (failedBytes != 0 || names != nullptr) {
            return false;
        }
    }
    const auto previousAllocations = allocations;
    auto malformed = fixture::MakeImage(true);
    std::fill(malformed.begin() + fixture::RawOffset + fixture::TextOffset, malformed.end(), 'x');
    if (!WriteImage(path, malformed)) {
        return false;
    }
    char *names = reinterpret_cast<char *>(static_cast<std::uintptr_t>(1));
    if (GetExportNames(path, &names) != 0 || names != nullptr || allocations != previousAllocations) {
        return false;
    }
    auto noExports = fixture::MakeImage(false);
    fixture::Put32(noExports, fixture::RawOffset + fixture::ExportNameCountOffset, 0);
    if (!WriteImage(path, noExports) || GetExportNames(path, &names) != 0 || names != nullptr
        || allocations != previousAllocations) {
        return false;
    }
    if (!WriteImage(path, {}) || GetExportNames(path, &names) != 0 || names != nullptr) {
        return false;
    }
    if (!DeleteFileA(path) || GetExportNames(path, &names) != 0 || names != nullptr
        || GetExportNames(nullptr, &names) != 0 || GetExportNames(path, nullptr) != 0) {
        return false;
    }
    return diagnostics == 3 && localCopies == 8;
}
} // namespace

int main(int argc, char **argv) {
    if (argc == 2) {
        char *names = nullptr;
        const int bytes = GetExportNames(argv[1], &names);
        if (bytes == 0 || !names) {
            return 1;
        }
        for (std::size_t offset = 0; offset < static_cast<std::size_t>(bytes);
             offset += std::strlen(names + offset) + 1) {
            std::puts(names + offset);
        }
        return 0;
    }
    char directory[MAX_PATH]{};
    char path[MAX_PATH]{};
    if (!GetTempPathA(sizeof(directory), directory) || !GetTempFileNameA(directory, "cse", 0, path)) {
        return 1;
    }
    DWORD handlesBefore = 0;
    DWORD handlesAfter = 0;
    if (!GetProcessHandleCount(GetCurrentProcess(), &handlesBefore)) {
        DeleteFileA(path);
        return 1;
    }
    const bool valid = CheckFileBoundary(path);
    DeleteFileA(path);
    if (!GetProcessHandleCount(GetCurrentProcess(), &handlesAfter) || handlesAfter != handlesBefore || !valid) {
        std::fprintf(stderr, "Export file boundary failed (allocations %u, copies %u, diagnostics %u)\n",
            allocations, localCopies, diagnostics);
        return 1;
    }
    std::puts("Export file mapping, allocation failure and handle lifecycle passed");
    return 0;
}
