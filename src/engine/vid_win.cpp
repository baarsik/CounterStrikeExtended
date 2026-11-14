#include "screenshot.h"
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

void VID_TakeSnapshot(const char* filename) {
    std::vector<unsigned char> encoded;
    if (!cse::capture::EncodeSoftware(vid.buffer, vid.width, vid.height, vid.rowbytes, vid.is15bit != 0, encoded) ||
        !cse::capture::WriteWebP(filename, encoded)) {
        Con_Printf("Couldn't write WebP snapshot %s\n", filename);
    }
}

void VID_WriteBuffer(const char* filenameBase) {
    static char baseFilename[MAX_PATH];
    static int fileNumber;
    char filename[MAX_PATH];
    int number;

    if (filenameBase) {
        Q_strncpy(baseFilename, filenameBase, sizeof(baseFilename) - 1);
        number = 1;
        fileNumber = 1;
    } else {
        number = fileNumber;
    }

    const int filenameLength = snprintf(filename, sizeof(filename), "%s%05d.webp", baseFilename, number);
    if (filenameLength < 0 || static_cast<std::size_t>(filenameLength) >= sizeof(filename)) {
        Con_Printf("Couldn't write WebP movie frame: filename is too long\n");
        return;
    }
    VID_TakeSnapshot(filename);
    ++fileNumber;
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
