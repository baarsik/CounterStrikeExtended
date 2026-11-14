#include "gl_model.h"
#include "common.h"
#include "engine_studio.h"
#include "zone.h"
#include "model_loading.h"

#include <cassert>
#include <climits>
#include <cstdio>
#include <vector>
#include <cstring>

#if defined(SW_RENDERER)
constexpr int SoftwarePixelBytes = sizeof(unsigned short);
int r_pixbytes = SoftwarePixelBytes;
#endif

static int Identity(int value) {
    return value;
}

static float IdentityFloat(float value) {
    return value;
}

int (*LittleLong)(int) = Identity;
float (*LittleFloat)(float) = IdentityFloat;

#include "allocator_expressions.inc"

int main() {
    constexpr std::size_t AlignmentMask = HUNK_ALIGNMENT - 1;
    constexpr std::size_t LastHunkPayload = (INT_MAX - sizeof(hunk_t)) & ~AlignmentMask;
    constexpr std::size_t LastCachePayload = (INT_MAX & ~AlignmentMask) - sizeof(cache_system_t);
    assert(ActualHunkTotal(static_cast<int>(LastHunkPayload)) > 0);
    assert(ActualCacheTotal(static_cast<int>(LastCachePayload)) > 0);
    assert(ActualHunkTotal(static_cast<int>(LastHunkPayload + 1)) < 0);
    assert(ActualCacheTotal(static_cast<int>(LastCachePayload + 1)) < 0);
    assert(cse::memory::MaximumHunkPayloadBytes == LastHunkPayload);
    assert(cse::memory::MaximumCachePayloadBytes == LastCachePayload);
    constexpr int TrailingByteReservation = 1;
    constexpr int LastWritableFileBytes = LastHunkPayload - TrailingByteReservation;
    assert(ModelLoading::WritableModelFileSize(LastWritableFileBytes));
    assert(!ModelLoading::WritableModelFileSize(LastWritableFileBytes + 1));
    assert(!ModelLoading::WritableModelFileSize(INT_MAX - 1));
    assert(ActualHunkTotal(ActualTemporaryHunkPayload(LastWritableFileBytes + TrailingByteReservation)) > 0);
    assert(ActualHunkTotal(ActualTemporaryHunkPayload(LastWritableFileBytes + 1 + TrailingByteReservation)) < 0);
    const std::size_t nativePaletteBytes = ModelLoading::PaletteEntries *
#if defined(SW_RENDERER)
        sizeof(PackedColorVec);
#else
        ModelLoading::PaletteChannels;
#endif
    const std::size_t bases[] = {
        sizeof(msprite_t) + sizeof(unsigned short) + nativePaletteBytes,
        sizeof(mspritegroup_t), 0
    };
    const std::size_t strides[] = {sizeof(mspriteframedesc_t), sizeof(mspriteframe_t*), sizeof(float)};
    for (std::size_t i = 0; i < sizeof(bases) / sizeof(bases[0]); ++i) {
        const int lastCount = static_cast<int>((LastHunkPayload - bases[i]) / strides[i]);
        assert(ModelLoading::NativeAllocation(bases[i], lastCount, strides[i]));
        assert(!ModelLoading::NativeAllocation(bases[i], lastCount + 1, strides[i]));
        assert(ActualHunkTotal(static_cast<int>(bases[i] + lastCount * strides[i])) > 0);
        assert(ActualHunkTotal(static_cast<int>(bases[i] + (lastCount + 1) * strides[i])) < 0);
    }
    assert(ModelLoading::NativeAllocation(0, LastHunkPayload, 1));
    assert(!ModelLoading::NativeAllocation(0, LastHunkPayload + 1, 1));

    studiohdr_t header{};
    header.version = STUDIO_VERSION;
    header.length = LastCachePayload;
    studiohdr_t checked;
    int allocationBytes = 0;
    assert(ModelLoading::Studio(&header, header.length, checked, allocationBytes));
    assert(allocationBytes == header.length);
    header.length = LastCachePayload + 1;
    assert(!ModelLoading::Studio(&header, header.length, checked, allocationBytes));
    header.version = STUDIO_VERSION + 1;
    assert(ModelLoading::Studio(&header, sizeof(header), checked, allocationBytes));
    assert(allocationBytes == sizeof(header));
    header.version = STUDIO_VERSION;
    constexpr int ReviewedStudioLength = INT_MAX - (HUNK_ALIGNMENT - 1);
    header.length = ReviewedStudioLength;
    assert(!ModelLoading::Studio(&header, header.length, checked, allocationBytes));

    // Large authoritative extents are arithmetic-only: validation reads just these backed metadata records.
    std::vector<byte> textured(sizeof(studiohdr_t) + sizeof(mstudiotexture_t));
    header.textureindex = sizeof(header);
    header.numtextures = 1;
    mstudiotexture_t texture{};
    texture.width = 1;
    texture.height = 1;
    texture.index = static_cast<int>(textured.size());
#if defined(SW_RENDERER)
    constexpr std::size_t PaletteExpansion = ModelLoading::ExpandedPaletteBytes - ModelLoading::PaletteBytes;
    header.texturedataindex = static_cast<int>(textured.size());
    header.length = LastCachePayload - PaletteExpansion;
#else
    header.texturedataindex = LastCachePayload;
    header.length = LastCachePayload;
#endif
    std::memcpy(textured.data(), &header, sizeof(header));
    std::memcpy(textured.data() + header.textureindex, &texture, sizeof(texture));
    assert(ModelLoading::Studio(textured.data(), header.length, checked, allocationBytes));
    assert(allocationBytes == LastCachePayload);
    ++header.length;
#if !defined(SW_RENDERER)
    ++header.texturedataindex;
#endif
    std::memcpy(textured.data(), &header, sizeof(header));
    assert(!ModelLoading::Studio(textured.data(), header.length, checked, allocationBytes));

#if defined(SW_RENDERER)
    dspriteframe_t frame{};
    frame.height = 1;
    constexpr int LastFrameWidth = (LastHunkPayload - sizeof(mspriteframe_t)) / SoftwarePixelBytes;
    frame.width = LastFrameWidth;
    ModelLoading::SpriteCursor lastFrame(&frame, INT_MAX);
    assert(lastFrame.Frame());
    frame.width = LastFrameWidth + 1;
    ModelLoading::SpriteCursor firstRejectedFrame(&frame, INT_MAX);
    assert(!firstRejectedFrame.Frame());
    frame.width = (INT_MAX - sizeof(mspriteframe_t)) / r_pixbytes;
    ModelLoading::SpriteCursor reviewedFrame(&frame, INT_MAX);
    assert(!reviewedFrame.Frame());
#endif
    std::printf("PASS allocator boundaries: hunk=%zu cache=%zu\n", LastHunkPayload, LastCachePayload);
}
