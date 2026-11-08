#pragma once

#include <climits>
#include "../common/pe_exports.h"

static int GetExportNames(const char *filename, char **names) {
    if (!names) {
        return 0;
    }
    *names = nullptr;
    if (!filename) {
        return 0;
    }
    FS_GetLocalCopy(filename);
    const HANDLE file = CreateFileA(filename, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
                                   FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        return 0;
    }
    LARGE_INTEGER fileBytes{};
    if (!GetFileSizeEx(file, &fileBytes) || fileBytes.QuadPart <= 0
        || static_cast<std::uint64_t>(fileBytes.QuadPart) > SIZE_MAX) {
        CloseHandle(file);
        return 0;
    }
    const HANDLE mapping = CreateFileMappingA(file, nullptr, PAGE_READONLY, 0, 0, nullptr);
    if (!mapping) {
        CloseHandle(file);
        return 0;
    }
    const void *image = MapViewOfFile(mapping, FILE_MAP_READ, 0, 0, 0);
    if (!image) {
        CloseHandle(mapping);
        CloseHandle(file);
        return 0;
    }
    const auto imageBytes = static_cast<std::size_t>(fileBytes.QuadPart);
    constexpr std::size_t MaximumNameBytes = INT_MAX; // Hunk_TempAlloc and this legacy API use signed byte counts.
    std::size_t length = 0;
    bool valid = cse::pe::VisitExportNames(image, imageBytes, [&](cse::pe::ExportName name) {
        if (name.size >= MaximumNameBytes - length) {
            return false;
        }
        length += name.size + 1;
        return true;
    });
    char *buffer = nullptr;
    if (valid && length != 0) {
        buffer = static_cast<char *>(Hunk_TempAlloc(static_cast<int>(length)));
        if (buffer) {
            std::size_t written = 0;
            valid = cse::pe::VisitExportNames(image, imageBytes, [&](cse::pe::ExportName name) {
                if (name.size >= length - written) {
                    return false;
                }
                std::memcpy(buffer + written, name.data, name.size);
                buffer[written + name.size] = '\0';
                written += name.size + 1;
                return true;
            });
            valid = valid && written == length;
        } else {
            valid = false;
        }
    }
    UnmapViewOfFile(image);
    CloseHandle(mapping);
    CloseHandle(file);
    if (!valid) {
        Con_DPrintf("Invalid or oversized export names in %s\n", filename);
        return 0;
    }
    *names = buffer;
    return static_cast<int>(length);
}

