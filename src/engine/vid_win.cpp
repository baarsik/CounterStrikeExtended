#include "vid.h"
#include "d_local.h"
#include "conprint.h"
#include "host.h"
#include "zone.h"
#include "common.h"
#include "sys.h"
#include "filesystem.h"

#include <SDL3/SDL.h>

#if defined(_WIN32)
#include <windows.h>
#else
#include "../common/cse_rect.h"
#endif
#include "../common/legacy_bmp_records.h"
#include <cstdint>
#include <limits>
#include <vector>
#include <cstdio>

extern int (*D_SurfaceCacheForRes)(int width, int height);
extern int minimum_memory;
extern SDL_Window **pmainwindow;

void S_Init();

byte *vid_surfcache;
int vid_surfcachesize;
int VID_highhunkmark;
byte *gppal;

int window_center_x;
int window_center_y;
RECT window_rect;
int gD3DMode;

extern "C" float GetXMouseAspectRatioAdjustment() {
    return 1.0f;
}

extern "C" float GetYMouseAspectRatioAdjustment() {
    return 1.0f;
}

/*
================
VID_AllocBuffers
================
*/
int VID_AllocBuffers() {
    vid.maxwarpwidth = 320;
    vid.maxwarpheight = 200;
    vid.colormap = gppal;
    vid.fullbright = 256;
    is15bit = vid.is15bit;

    constexpr unsigned int kMaximumSoftwareWidth = 5120;
    constexpr std::int64_t kLegacyVideoMemoryAllowance = 1245184;
    if (vid.width == 0 || vid.height == 0 || vid.width > kMaximumSoftwareWidth || vid.height > MAXHEIGHT) {
        return 0;
    }
    const std::uint64_t zbufferBytes = sizeof(short) * std::uint64_t{vid.height} * vid.width;
    const int cacheBytes = D_SurfaceCacheForRes(vid.width, vid.height);
    if (cacheBytes <= 0 || zbufferBytes + static_cast<unsigned int>(cacheBytes) > INT_MAX) {
        return 0;
    }
    const int bufferBytes = static_cast<int>(zbufferBytes + cacheBytes);
    if (static_cast<std::int64_t>(host_parms.memsize) - bufferBytes + kLegacyVideoMemoryAllowance < minimum_memory) {
        Con_SafePrintf("Not enough memory for video mode\n");
        return 0;
    }

    vid_surfcachesize = cacheBytes;
    if (d_pzbuffer) {
        D_FlushCaches();
        Hunk_FreeToHighMark(VID_highhunkmark);
        d_pzbuffer = NULL;
    }
    VID_highhunkmark = Hunk_HighMark();
    d_pzbuffer = (short *)Hunk_HighAllocName(bufferBytes, "video");
    if (!d_pzbuffer) {
        vid_surfcache = nullptr;
        return 0;
    }
    vid_surfcache = reinterpret_cast<byte *>(d_pzbuffer) + zbufferBytes;
    D_InitCaches();
    return 1;
}

// Retire aliases before the engine releases the high hunk's owning heap.
void VID_ReleaseBuffers() {
    if (d_pzbuffer) {
        D_FlushCaches();
    }
    d_pzbuffer = nullptr;
    vid_surfcache = nullptr;
    vid_surfcachesize = 0;
    VID_highhunkmark = 0;
    gppal = nullptr;
    vid.colormap = nullptr;
}

extern "C" int VID_Init(unsigned short *palette) {
    gppal = (byte *)palette;
    if (!VID_AllocBuffers()) {
        return 0;
    }
    S_Init();
    return 1;
}

void VID_TakeSnapshot(const char *pFilename) {
    cse_bmp::FileHeader hdr{};
    cse_bmp::InfoHeader bi{};
    constexpr unsigned int kMaximumSoftwareWidth = 5120;
    constexpr unsigned int kBitmapChannels = 3;
    constexpr unsigned int kBitmapRowAlignment = 4;
    if (!vid.buffer || vid.width == 0 || vid.height == 0 || vid.width > kMaximumSoftwareWidth ||
        vid.height > MAXHEIGHT || vid.rowbytes < sizeof(unsigned short) * vid.width) {
        Sys_Error("Invalid software snapshot buffer\n");
        return;
    }
    const unsigned int rowBytes = kBitmapChannels * vid.width;
    const unsigned int padded = (rowBytes + kBitmapRowAlignment - 1) & ~(kBitmapRowAlignment - 1);
    std::vector<byte> rowbuffer(padded, 0);
    const unsigned int imageBytes = padded * vid.height;
    FileHandle_t file = FS_Open(pFilename, "wb");
    if (!file) {
        Sys_Error("Couldn't create file for snapshot.\n");
    }

    hdr.bfType = 0x4D42; // 'BM'
    hdr.bfSize = imageBytes + sizeof(hdr) + sizeof(bi);
    hdr.bfReserved1 = 0;
    hdr.bfReserved2 = 0;
    hdr.bfOffBits = sizeof(hdr) + sizeof(bi);
    if (FS_Write(&hdr, sizeof(hdr), 1, file) != (int)sizeof(hdr)) {
        Sys_Error("Couldn't write file header to snapshot.\n");
    }

    bi.biSize = sizeof(bi);
    bi.biWidth = vid.width;
    bi.biHeight = vid.height;
    bi.biPlanes = 1;
    bi.biBitCount = 24;
    bi.biCompression = cse_bmp::RgbCompression;
    bi.biSizeImage = 0;
    bi.biXPelsPerMeter = 0;
    bi.biYPelsPerMeter = 0;
    bi.biClrUsed = 0;
    bi.biClrImportant = 0;
    if (FS_Write(&bi, sizeof(bi), 1, file) != (int)sizeof(bi)) {
        Sys_Error("Couldn't write bitmap header to snapshot.\n");
    }

    byte *pSource = vid.buffer + static_cast<std::size_t>(vid.rowbytes) * vid.height;
    for (unsigned int y = 0; y < vid.height; ++y) {
        pSource -= vid.rowbytes;
        unsigned short *pPixel = (unsigned short *)pSource;
        byte *pDest = rowbuffer.data();
        for (unsigned int x = 0; x < vid.width; ++x) {
            unsigned short color = *pPixel++;
            pDest[0] = (byte)(color << 3);
            if (is15bit) {
                pDest[1] = (byte)(color >> 2) & 0xF8;
                pDest[2] = (byte)(color >> 7) & 0xF8;
            } else {
                pDest[1] = (byte)(color >> 3) & 0xFC;
                pDest[2] = (byte)(color >> 8) & 0xF8;
            }
            pDest += 3;
        }

        if (FS_Write(rowbuffer.data(), padded, 1, file) != static_cast<int>(padded)) {
            Sys_Error("Couldn't write bitmap data snapshot.\n");
        }
    }

    FS_Close(file);
}

void VID_WriteBuffer(const char *pFilename) {
    static char basefilename[MAX_PATH];
    static int filenumber;
    char filename[MAX_PATH];
    int number;

    if (pFilename) {
        Q_strncpy(basefilename, pFilename, sizeof(basefilename) - 1);
        number = 1;
        filenumber = 1;
    } else {
        number = filenumber;
    }

    _snprintf(filename, sizeof(filename) - 1, "%s%05d.bmp", basefilename, number);
    filename[sizeof(filename) - 1] = '\0';
    VID_TakeSnapshot(filename);
    filenumber++;
}

extern "C" void VID_UpdateWindowVars(RECT *rect, int x, int y) {
    if (pmainwindow) {
        int width, height;
        int left, top;

        SDL_GetWindowSizeInPixels(*pmainwindow, &width, &height);
        SDL_GetWindowPosition(*pmainwindow, &left, &top);
        rect->left = left;
        rect->right = left + width;
        rect->top = top;
        rect->bottom = top + height;
        window_center_x = left + width / 2;
        window_center_y = top + height / 2;
    } else {
        window_center_x = x;
        window_center_y = y;
    }
    window_rect = *rect;
}
