#pragma once
#include <cstdio>
using FileHandle_t = FILE*;
constexpr int FILESYSTEM_SEEK_CURRENT = SEEK_CUR;
struct DiskFileSystem {
    FILE* Open(const char* path, const char*, const char* = nullptr) { return std::fopen(path, "rb"); }
    unsigned int Size(FILE* file) {
        const long position = std::ftell(file);
        std::fseek(file, 0, SEEK_END);
        const long size = std::ftell(file);
        std::fseek(file, position, SEEK_SET);
        return static_cast<unsigned int>(size);
    }
    int Read(void* data, int bytes, FILE* file) { return static_cast<int>(std::fread(data, 1, bytes, file)); }
    void Close(FILE* file) { std::fclose(file); }
    void Seek(FILE* file, int offset, int origin) { std::fseek(file, offset, origin); }
};
