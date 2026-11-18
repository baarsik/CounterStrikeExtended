// Exercises the selected owner with controlled filesystem and hunk services.
#include "../../../src/engine/vid_win.cpp"
#include <cassert>
#include <vector>
#include <stdexcept>
#include <limits>
viddef_t vid{};
quakeparms_t host_parms{};
short *d_pzbuffer;
int is15bit;
int minimum_memory;
SDL_Window **pmainwindow;
static std::vector<byte> hunk;
static std::vector<byte> snapshot;
static int requestedCache = 1024;
static int cacheSize(int, int) {
    return requestedCache;
}
int (*D_SurfaceCacheForRes)(int, int) = cacheSize;
void D_FlushCaches() {}
void D_InitCaches() {}
void Hunk_FreeToHighMark(int) {}
int Hunk_HighMark() {
    return 0;
}
void *Hunk_HighAllocName(int bytes, const char *) {
    hunk.resize(bytes);
    return hunk.data();
}
void Con_SafePrintf(const char *, ...) {}
void Con_Printf(const char *, ...) {}
void Sys_Error(const char *, ...) {
    throw std::runtime_error("engine error");
}
FileHandle_t FS_OpenPathID(const char *filename, const char *mode, const char *pathID) {
    assert(cse::image::HasWebPExtension(filename));
    assert(std::strcmp(mode, "wb") == 0);
    assert(std::strcmp(pathID, "GAMECONFIG") == 0);
    snapshot.clear();
    return reinterpret_cast<FileHandle_t>(1);
}
int FS_Write(const void *data, int size, int count, FileHandle_t) {
    const byte *p = static_cast<const byte *>(data);
    snapshot.insert(snapshot.end(), p, p + size * count);
    return size * count;
}
int FS_IsOk(FileHandle_t) {
    return 1;
}
void FS_Flush(FileHandle_t) {}
void FS_Close(FileHandle_t) {}
void FS_RemoveFile(const char *, const char *) {
    assert(false && "a successful snapshot must be retained");
}
int main(int argc, char **argv) {
    assert(argc == 2);
    host_parms.memsize = 128 * 1024 * 1024;
    if (std::strcmp(argv[1], "software_checked_allocations") == 0) {
        for (unsigned width : {0u, 5121u, 0xffffffffu}) {
            vid.width = width;
            vid.height = 2;
            assert(VID_AllocBuffers() == 0);
        }
        vid.width = 3;
        for (unsigned height : {0u, 1601u, 0xffffffffu}) {
            vid.height = height;
            assert(VID_AllocBuffers() == 0);
        }
        vid.width = 5120;
        vid.height = 1600;
        assert(VID_AllocBuffers() == 1);
        assert(vid_surfcache == reinterpret_cast<byte *>(d_pzbuffer) + 16384000);
        requestedCache = -1;
        assert(VID_AllocBuffers() == 0);
        requestedCache = std::numeric_limits<int>::max();
        assert(VID_AllocBuffers() == 0);
    } else if (std::strcmp(argv[1], "software_pixels_and_palette") == 0) {
        unsigned short palette[256][4]{};
        palette[1][0] = 0x1234;
        palette[255][3] = 0xabcd;
        vid.width = 3;
        vid.height = 2;
        vid.is15bit = 1;
        assert(VID_Init(&palette[0][0]) == 1);
        assert(vid.colormap == reinterpret_cast<byte *>(palette));
        assert(is15bit == 1 && palette[255][3] == 0xabcd);
        VID_ReleaseBuffers();
        assert(!d_pzbuffer && !vid_surfcache && !gppal && vid_surfcachesize == 0);
        VID_ReleaseBuffers();
        assert(VID_Init(&palette[0][0]) == 1);
    } else {
        // Odd width, padded source pitch, distinguish all rows and quantization.
        unsigned short pixels[8] = {0xf800, 0x07e0, 0x001f, 0xdead, 0xffff, 0, 0x8410, 0xbeef};
        vid.width = 3;
        vid.height = 2;
        vid.rowbytes = 8;
        vid.buffer = reinterpret_cast<byte *>(pixels);
        vid.is15bit = 0;
        VID_TakeSnapshot("test.webp");
        cse::image::Image decoded;
        assert(cse::image::DecodeWebP(snapshot.data(), snapshot.size(), decoded));
        assert(decoded.width == 3 && decoded.height == 2);
        const std::vector<byte> expected565{248, 0, 0, 255, 0, 252, 0, 255, 0, 0, 248, 255,
            248, 252, 248, 255, 0, 0, 0, 255, 128, 128, 128, 255};
        assert(decoded.rgba == expected565);

        pixels[0] = 0x7c00;
        pixels[1] = 0x03e0;
        pixels[4] = 0x7fff;
        pixels[6] = 0x4210;
        vid.is15bit = 1;
        VID_TakeSnapshot("test.webp");
        assert(cse::image::DecodeWebP(snapshot.data(), snapshot.size(), decoded));
        const std::vector<byte> expected555{248, 0, 0, 255, 0, 248, 0, 255, 0, 0, 248, 255,
            248, 248, 248, 255, 0, 0, 0, 255, 128, 128, 128, 255};
        assert(decoded.rgba == expected555);

        std::vector<unsigned short> wide(5122, 0xffff);
        vid.width = 5120;
        vid.height = 1;
        vid.rowbytes = static_cast<unsigned int>(wide.size() * sizeof(unsigned short));
        vid.buffer = reinterpret_cast<byte *>(wide.data());
        VID_TakeSnapshot("wide.webp");
        assert(cse::image::DecodeWebP(snapshot.data(), snapshot.size(), decoded));
        assert(decoded.width == 5120 && decoded.height == 1);
        for (std::size_t index = 0; index < decoded.rgba.size(); index += cse::image::RgbaChannels) {
            assert(decoded.rgba[index] == 248 && decoded.rgba[index + 1] == 248 &&
                decoded.rgba[index + 2] == 248 && decoded.rgba[index + 3] == 255);
        }
    }
}
