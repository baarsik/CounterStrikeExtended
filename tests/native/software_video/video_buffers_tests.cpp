// Exercises the selected owner, with only filesystem/hunk/audio services controlled.
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
void S_Init() {}
void Con_SafePrintf(const char *, ...) {}
void Sys_Error(const char *, ...) {
    throw std::runtime_error("engine error");
}
FileHandle_t FS_Open(const char *, const char *) {
    snapshot.clear();
    return reinterpret_cast<FileHandle_t>(1);
}
int FS_Write(const void *data, int size, int count, FileHandle_t) {
    const byte *p = static_cast<const byte *>(data);
    snapshot.insert(snapshot.end(), p, p + size * count);
    return size * count;
}
void FS_Close(FileHandle_t) {}
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
        is15bit = 0;
        VID_TakeSnapshot("test.bmp");
        const byte expected[] = {248, 252, 248, 0, 0,   0, 128, 128, 128, 0, 0, 0,
                                 0,   0,   248, 0, 252, 0, 248, 0,   0,   0, 0, 0};
        const byte expectedHeader[] = {0x42, 0x4d, 78, 0, 0, 0, 0, 0, 0, 0, 54, 0, 0, 0, 40, 0, 0, 0,
                                       3,    0,    0,  0, 2, 0, 0, 0, 1, 0, 24, 0, 0, 0, 0,  0, 0, 0,
                                       0,    0,    0,  0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0};
        static_assert(sizeof(expectedHeader) == 54);
        assert(std::memcmp(snapshot.data(), expectedHeader, sizeof(expectedHeader)) == 0);
        assert(snapshot.size() == 78);
        assert(snapshot[0] == 'B' && snapshot[1] == 'M' && snapshot[2] == 78 && snapshot[10] == 54);
        assert(snapshot[14] == 40 && snapshot[18] == 3 && snapshot[22] == 2 && snapshot[28] == 24);
        assert(std::memcmp(snapshot.data() + 54, expected, sizeof(expected)) == 0);
        pixels[0] = 0x7c00;
        pixels[1] = 0x03e0;
        pixels[2] = 0x001f;
        is15bit = 1;
        VID_TakeSnapshot("test.bmp");
        const byte corners[] = {0, 0, 248, 0, 248, 0, 248, 0, 0, 0, 0, 0};
        assert(std::memcmp(snapshot.data() + 66, corners, sizeof(corners)) == 0);
        std::vector<unsigned short> wide(5122, 0xffff);
        vid.width = 5120;
        vid.height = 1;
        vid.rowbytes = static_cast<unsigned int>(wide.size() * sizeof(unsigned short));
        vid.buffer = reinterpret_cast<byte *>(wide.data());
        VID_TakeSnapshot("wide.bmp");
        assert(snapshot.size() == 15414);
        assert(snapshot.back() == 248);
    }
}
