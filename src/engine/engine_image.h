#ifndef CSE_ENGINE_IMAGE_H
#define CSE_ENGINE_IMAGE_H

#include "image_codec.h"

#include <cstring>
#include <string>

// Keep the filesystem adapter usable by both renderers without pulling in their model headers.
unsigned char* COM_LoadFile(char* path, int usehunk, int* length);
void COM_FreeFile(void* buffer);

namespace cse {
namespace engine_image {

inline bool HasExtension(const char* path) {
    const std::string name(path ? path : "");
    const auto slash = name.find_last_of("/\\");
    const auto dot = name.find_last_of('.');
    return dot != std::string::npos && (slash == std::string::npos || dot > slash);
}

inline std::string TargaPath(const char* path) {
    std::string name(path ? path : "");
    if (!name.empty() && !HasExtension(name.c_str())) {
        name += ".tga";
    }
    return name;
}

inline std::string BitmapPath(const char* path) {
    std::string name(path ? path : "");
    if (HasExtension(name.c_str())) {
        name.resize(name.find_last_of('.'));
    }
    name += ".bmp";
    return name;
}

inline unsigned char* LoadLegacyTarga(const char* filename, int* length) {
    constexpr int HeapFileAllocation = 5;
    if (!filename || !*filename) {
        return nullptr;
    }
    unsigned char* file = COM_LoadFile(const_cast<char*>(filename), HeapFileAllocation, length);
    if (file || HasExtension(filename)) {
        return file;
    }
    const std::string path = TargaPath(filename);
    return COM_LoadFile(const_cast<char*>(path.c_str()), HeapFileAllocation, length);
}

inline void CopyRGBA(const image::Image& decoded, unsigned char* buffer, int* width, int* height) {
    std::memcpy(buffer, decoded.rgba.data(), decoded.rgba.size());
    if (width) {
        *width = decoded.width;
    }
    if (height) {
        *height = decoded.height;
    }
}

inline bool LoadWebP(const char* filename, image::Image& decoded, std::size_t capacity) {
    constexpr int HeapFileAllocation = 5;
    const std::string path = image::WebPPath(filename);
    if (path.empty()) {
        return false;
    }
    int length = 0;
    unsigned char* file = COM_LoadFile(const_cast<char*>(path.c_str()), HeapFileAllocation, &length);
    if (!file) {
        return false;
    }
    const bool loaded = length > 0 && image::DecodeWebP(file, static_cast<std::size_t>(length), decoded, capacity);
    COM_FreeFile(file);
    return loaded;
}

}
}

#endif
