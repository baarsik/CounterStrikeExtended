#include <algorithm>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>
#include <webp/decode.h>
#include "legacy_bmp_records.h"
#if defined(_WIN32)
#include <windows.h>
#else
#include "cse_rect.h"
#endif

using byte = unsigned char;
using word = unsigned short;
using FileHandle_t = void*;
#if !defined(_WIN32)
using BITMAPFILEHEADER = cse_bmp::FileHeader;
using BITMAPINFOHEADER = cse_bmp::InfoHeader;
#endif
constexpr int GL_PACK_ALIGNMENT = 0x0d05;
constexpr int GL_RGB = 0x1907;
constexpr int GL_UNSIGNED_BYTE = 0x1401;

struct Video {
    byte* buffer = nullptr;
    unsigned int width = 3;
    unsigned int height = 2;
    unsigned int rowbytes = 8;
    int is15bit = 0;
} vid;
struct Rectangle {
    int left = 7;
    int top = 11;
    int right = 10;
    int bottom = 13;
} window_rect;
int bDoScaledFBO = 1;
bool windowed = true;
bool openFailure = false;
bool shortWrite = false;
bool unhealthyWrite = false;
int readX = 0;
int readY = 0;
int readWidth = 0;
int readHeight = 0;
int readCount = 0;
std::map<std::string, std::vector<byte>> files;
std::vector<std::string> messages;
struct CaptureFile { std::string name; };
int VideoMode_IsWindowed() { return windowed; }
void Q_strcpy(char* to, const char* from) { std::strcpy(to, from); }
void Q_strncpy(char* to, const char* from, std::size_t size) { std::strncpy(to, from, size); }
void Con_Printf(const char* format, ...) {
    char message[512];
    va_list args;
    va_start(args, format);
    std::vsnprintf(message, sizeof(message), format, args);
    va_end(args);
    messages.emplace_back(message);
}
void Sys_Error(const char* text) { throw std::runtime_error(text); }
int FS_FileExists(const char* name) { return files.count(name) != 0; }
FileHandle_t FS_OpenPathID(const char* name, const char* mode, const char*) {
    if (openFailure || (mode[0] == 'r' && !FS_FileExists(name))) {
        return nullptr;
    }
    if (mode[0] == 'w') {
        files[name].clear();
    }
    return new CaptureFile{name};
}
FileHandle_t FS_Open(const char* name, const char* mode) { return FS_OpenPathID(name, mode, nullptr); }
int FS_Write(const void* data, int size, int, FileHandle_t handle) {
    const int written = shortWrite ? size / 2 : size;
    auto& bytes = files[static_cast<CaptureFile*>(handle)->name];
    const auto* begin = static_cast<const byte*>(data);
    bytes.insert(bytes.end(), begin, begin + written);
    return written;
}
void FS_Flush(FileHandle_t) {}
int FS_IsOk(FileHandle_t) { return !unhealthyWrite; }
void FS_Close(FileHandle_t handle) { delete static_cast<CaptureFile*>(handle); }
void FS_RemoveFile(const char* name, const char*) { files.erase(name); }
void FS_Unlink(const char* name) { files.erase(name); }
void* Mem_Malloc(std::size_t bytes) { return std::malloc(bytes); }
void Mem_Free(void* pointer) { std::free(pointer); }
void qglPixelStorei(int, int) {}
void qglReadPixels(int x, int y, int width, int height, int, int, void* output) {
    // OpenGL supplies the bottom row first. Each fixture has an odd width.
    const byte fixture[] = {255, 255, 0, 255, 0, 255, 0, 255, 255, 255, 0, 0, 0, 255, 0, 0, 0, 255};
    readX = x;
    readY = y;
    readWidth = width;
    readHeight = height;
    ++readCount;
    const std::size_t bytes = static_cast<std::size_t>(width) * height * 3;
    if (bytes > sizeof(fixture)) {
        throw std::runtime_error("unexpected capture size");
    }
    std::memcpy(output, fixture, bytes);
}
// Controlled renderer seam for the actual extracted capture entry points.
// GPU target-size/readback behavior is exercised by renderer-backend-lifecycle.
namespace cse {
namespace render {
bool metal = false;
bool presented = true;
bool UsingMetal() {
    return metal;
}
bool BackendPresentedSize(int& width, int& height) {
    width = presented ? 3 : 0;
    height = presented ? 2 : 0;
    return presented;
}
bool BackendReadPixels(bool finalColor, int x, int y, int width, int height, unsigned components,
    unsigned alignment, void* output) {
    if (!finalColor || components != 3 || alignment != 1) {
        throw std::runtime_error("unexpected Metal capture target or packing");
    }
    qglReadPixels(x, y, width, height, GL_RGB, GL_UNSIGNED_BYTE, output);
    return true;
}
}
}

struct Model { char name[64] = "maps/de_capture.bsp"; } model;
struct Entity { Model* model = &::model; } entity;
struct Client { int num_entities = 1; } cl;
Entity* cl_entities = &entity;
std::string mapBase = "de_capture";
void COM_FileBase(const char*, char* base) { std::strcpy(base, mapBase.c_str()); }

#if __has_include("screenshot.h")
#include "screenshot.h"
#endif
#ifndef _snprintf
#define _snprintf snprintf
#endif
#include "hardware.inc"
#include "software.inc"
#include "client.inc"

void Require(bool condition, const char* explanation) {
    if (!condition) {
        throw std::runtime_error(explanation);
    }
}
void CheckImage(const char* name, const std::vector<byte>& expected) {
    Require(files.count(name) == 1, "capture did not use the expected .webp filename");
    const auto& encoded = files.at(name);
    Require(encoded.size() >= 12 && std::memcmp(encoded.data(), "RIFF", 4) == 0 &&
        std::memcmp(encoded.data() + 8, "WEBP", 4) == 0, "capture bytes are not WebP");
    int width = 0;
    int height = 0;
    byte* pixels = WebPDecodeRGB(encoded.data(), encoded.size(), &width, &height);
    Require(pixels != nullptr, "capture WebP cannot be decoded");
    const std::vector<byte> actual(pixels, pixels + width * height * 3);
    WebPFree(pixels);
    Require(width == 3 && height == 2, "capture geometry changed");
    Require(actual == expected, "capture changed colors, channel order, or vertical orientation");
}
int main(int argc, char** argv) {
    try {
        Require(argc == 2, "expected one test scenario");
        const std::string scenario = argv[1];
        const std::vector<byte> glExpected = {255, 0, 0, 0, 255, 0, 0, 0, 255,
            255, 255, 0, 255, 0, 255, 0, 255, 255};
        // Padding is deliberately bright to catch rowbytes being mistaken for width * 2.
        std::vector<word> softwarePixels = {0xf800, 0x07e0, 0x001f, 0xffff, 0xffe0, 0xf81f, 0x07ff, 0xffff};
        vid.buffer = reinterpret_cast<byte*>(softwarePixels.data());
        const std::vector<byte> softwareExpected = {248, 0, 0, 0, 252, 0, 0, 0, 248,
            248, 252, 0, 248, 0, 248, 0, 252, 248};
        if (scenario == "hardware-screenshot") {
            files["HalfLife00.webp"] = {42};
            hardware::SCR_ScreenShot_f();
            CheckImage("HalfLife01.webp", glExpected);
            Require(files["HalfLife00.webp"] == std::vector<byte>{42}, "screenshot overwrote an earlier image");
        } else if (scenario == "metal-retained-size") {
            cse::render::metal = true;
            window_rect.right = 100;
            window_rect.bottom = 100;
            hardware::SCR_ScreenShot_f();
            hardware::VID_TakeSnapshot("retained.webp");
            CheckImage("HalfLife00.webp", glExpected);
            CheckImage("retained.webp", glExpected);
            Require(readCount == 2, "both capture paths must read the retained presentation");
        } else if (scenario == "metal-no-presentation") {
            cse::render::metal = true;
            cse::render::presented = false;
            hardware::SCR_ScreenShot_f();
            hardware::VID_TakeSnapshot("absent.webp");
            Require(files.empty() && readCount == 0, "absent presentation reached readback or created a file");
            Require(messages.size() == 2, "absent presentation was not reported");
        } else if (scenario == "hardware-snapshot") {
            hardware::VID_TakeSnapshot("snapshot.webp");
            CheckImage("snapshot.webp", glExpected);
        } else if (scenario == "hardware-rect") {
            char name[] = "rect.webp";
            hardware::VID_TakeSnapshotRect(name, 17, 23, 3, 2);
            CheckImage(name, glExpected);
            Require(readX == 17 && readY == 23, "rectangle origin changed");
        } else if (scenario == "hardware-fullscreen") {
            windowed = false;
            bDoScaledFBO = 0;
            window_rect.right = 123;
            hardware::VID_TakeSnapshot("fullscreen.webp");
            CheckImage("fullscreen.webp", glExpected);
        } else if (scenario == "hardware-movie") {
            hardware::VID_WriteBuffer("movie");
            hardware::VID_WriteBuffer(nullptr);
            CheckImage("movie00001.webp", glExpected);
            CheckImage("movie00002.webp", glExpected);
        } else if (scenario == "software-movie") {
            software::VID_WriteBuffer("movie");
            software::VID_WriteBuffer(nullptr);
            CheckImage("movie00001.webp", softwareExpected);
            CheckImage("movie00002.webp", softwareExpected);
        } else if (scenario == "software-screenshot") {
            files["HalfLife00.webp"] = {42};
            software::SCR_ScreenShot_f();
            CheckImage("HalfLife01.webp", softwareExpected);
        } else if (scenario == "software-565") {
            software::VID_TakeSnapshot("565.webp");
            CheckImage("565.webp", softwareExpected);
        } else if (scenario == "software-555") {
            vid.is15bit = 1;
            softwarePixels = {0x7c00, 0x03e0, 0x001f, 0x7fff, 0x7fe0, 0x7c1f, 0x03ff, 0x7fff};
            vid.buffer = reinterpret_cast<byte*>(softwarePixels.data());
            software::VID_TakeSnapshot("555.webp");
            CheckImage("555.webp", {248, 0, 0, 0, 248, 0, 0, 0, 248,
                248, 248, 0, 248, 0, 248, 0, 248, 248});
        } else if (scenario == "client-numbering") {
            files["de_capture0000.webp"] = {42};
            client::CL_TakeSnapshot_f();
            CheckImage("de_capture0001.webp", glExpected);
            Require(files["de_capture0000.webp"] == std::vector<byte>{42}, "snapshot overwrote an earlier image");
        } else if (scenario == "client-long-name") {
            mapBase.assign(63, 'm');
            client::CL_TakeSnapshot_f();
            CheckImage((mapBase + "0000.webp").c_str(), glExpected);
        } else if (scenario == "movie-long-name") {
            const std::string longBase(MAX_PATH - 1, 'm');
            hardware::VID_WriteBuffer(longBase.c_str());
            software::VID_WriteBuffer(longBase.c_str());
            Require(files.empty(), "long movie basename produced a truncated filename without .webp");
            Require(messages.size() == 2, "long movie filename failure was silent");
        } else if (scenario == "failed-write") {
            shortWrite = true;
            hardware::VID_TakeSnapshot("failed.webp");
            Require(files.count("failed.webp") == 0, "failed capture left a misleading partial file");
            Require(!messages.empty(), "failed capture was silent");
        } else if (scenario == "failed-flush") {
            unhealthyWrite = true;
            software::VID_TakeSnapshot("failed.webp");
            Require(files.count("failed.webp") == 0, "failed flush left a misleading file");
            Require(!messages.empty(), "failed flush was silent");
        } else if (scenario == "failed-open") {
            openFailure = true;
            hardware::SCR_ScreenShot_f();
            Require(files.empty(), "failed open created a file");
            Require(!messages.empty() && messages.back().find("Wrote ") == std::string::npos,
                "failed open reported success or was silent");
        } else if (scenario == "software-invalid-stride") {
            vid.rowbytes = 5;
            software::VID_TakeSnapshot("invalid.webp");
            Require(files.empty(), "invalid software stride created a file");
            Require(!messages.empty(), "invalid software stride was silent");
        } else if (scenario == "numbering-exhausted") {
            for (int i = 0; i < 100; ++i) {
                char name[32];
                std::snprintf(name, sizeof(name), "HalfLife%02d.webp", i);
                files[name] = {42};
            }
            hardware::SCR_ScreenShot_f();
            software::SCR_ScreenShot_f();
            Require(files.size() == 100 && readCount == 0, "exhausted screenshot numbers created a file");
            for (const auto& entry : files) {
                Require(entry.second == std::vector<byte>{42}, "exhausted numbers overwrote an image");
            }
        } else if (scenario == "invalid-dimensions") {
            window_rect.right = window_rect.left;
            hardware::VID_TakeSnapshot("invalid.webp");
            Require(files.empty() && readCount == 0, "invalid dimensions reached capture or created an empty file");
        } else {
            throw std::runtime_error("unknown scenario");
        }
        std::printf("PASS %s\n", scenario.c_str());
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "FAIL %s: %s\n", argc > 1 ? argv[1] : "", error.what());
        return 1;
    }
}
