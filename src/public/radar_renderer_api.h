#ifndef CSE_RADAR_RENDERER_API_H
#define CSE_RADAR_RENDERER_API_H

#include <stdint.h>

#ifndef CSE_ABI_CALL
#if defined(_WIN32)
#define CSE_ABI_CALL __cdecl
#else
#define CSE_ABI_CALL
#endif
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define CSE_RADAR_RENDERER_INTERFACE "CSERadarRenderer001"
#define CSE_RADAR_RENDERER_VERSION UINT32_C(1)
#define CSE_RADAR_MAP_IMAGE_SIZE UINT32_C(1024)
#define CSE_RADAR_TEXTURED_POLYGON_CAPACITY UINT32_C(10)

typedef int32_t CseRadarResult;
enum {
    CSE_RADAR_OK = 0,
    CSE_RADAR_WAITING = 1,
    CSE_RADAR_UNAVAILABLE = 2,
    CSE_RADAR_NO_DRAW_TARGET = 3,
    CSE_RADAR_INVALID_ARGUMENT = 4,
    CSE_RADAR_STALE_EPOCH = 5,
    CSE_RADAR_INVALID_TEXTURE = 6,
    CSE_RADAR_OUT_OF_MEMORY = 7,
    CSE_RADAR_DEVICE_LOST = 8
};

/* Enum constants do not determine struct field or function parameter widths. */
enum {
    CSE_RADAR_BACKEND_NONE = 0,
    CSE_RADAR_BACKEND_OPENGL = 1,
    CSE_RADAR_BACKEND_METAL = 2
};
enum {
    CSE_RADAR_CAP_MAP_IMAGE = 1u << 0,
    CSE_RADAR_CAP_PRIMITIVES = 1u << 1,
    CSE_RADAR_CAP_MAP_TEXTURE = 1u << 2,
    CSE_RADAR_CAP_MASK_RING = 1u << 3,
    CSE_RADAR_CAP_BACKDROP = 1u << 4,
    CSE_RADAR_CAP_ALL = 31
};
enum {
    CSE_RADAR_STATE_READY = 1u << 0,
    CSE_RADAR_STATE_DRAW_TARGET = 1u << 1
};
enum {
    CSE_RADAR_SHAPE_NONE = 0,
    CSE_RADAR_SHAPE_CIRCLE = 1,
    CSE_RADAR_SHAPE_SQUARE = 2
};
enum {
    CSE_RADAR_BLEND_STRAIGHT = 0,
    CSE_RADAR_BLEND_ADDITIVE_ALPHA = 1,
    CSE_RADAR_BLEND_PREMULTIPLIED = 2
};
enum {
    CSE_RADAR_WORLD_BASE_READY = 1u << 0,
    CSE_RADAR_WORLD_NEEDS_LIGHTMAP = 1u << 1,
    CSE_RADAR_WORLD_LIGHTMAP_READY = 1u << 2
};
enum {
    CSE_RADAR_MAP_USE_BASE = 1u << 0,
    CSE_RADAR_MAP_USE_LIGHTMAP = 1u << 1
};
enum {
    CSE_RADAR_DEPTH_TOPMOST = 0,
    CSE_RADAR_DEPTH_BOTTOMMOST = 1
};
enum {
    CSE_RADAR_TONE_FLAT = 0,
    CSE_RADAR_TONE_TEXTURED = 1
};
enum {
    CSE_RADAR_MAP_PLAIN_OVERVIEW = 0,
    CSE_RADAR_MAP_MASKED_OVERVIEW = 1,
    CSE_RADAR_MAP_PREMULTIPLIED_IMAGE = 2
};
enum {
    CSE_RADAR_PRIMITIVE_DISC = 0,
    CSE_RADAR_PRIMITIVE_CONE = 1,
    CSE_RADAR_PRIMITIVE_TRIANGLE = 2,
    CSE_RADAR_PRIMITIVE_CROSS = 3,
    CSE_RADAR_PRIMITIVE_RING = 4,
    CSE_RADAR_PRIMITIVE_SOLID = 5,
    CSE_RADAR_PRIMITIVE_GLYPH = 6,
    CSE_RADAR_PRIMITIVE_QUESTION = 7,
    CSE_RADAR_PRIMITIVE_BOMB = 8,
    CSE_RADAR_PRIMITIVE_DISCONNECTED = 9,
    CSE_RADAR_PRIMITIVE_PLAYER_FADE = 10
};

typedef struct CseRadarTexture {
    uint64_t rendererEpoch;
    uint64_t serial;
} CseRadarTexture;

typedef struct CseRadarState {
    uint32_t size;
    uint32_t version;
    uint32_t backend;
    uint32_t capabilities;
    uint64_t rendererEpoch;
    uint32_t flags;
    uint32_t reserved;
} CseRadarState;

typedef struct CseRadarHudSpace {
    float width;
    float height;
} CseRadarHudSpace;

typedef struct CseRadarFade {
    float rgb[3];
    float amount;
} CseRadarFade;

typedef struct CseRadarClip {
    float center[2];
    float radius;
    uint32_t shape;
} CseRadarClip;

typedef struct CseRadarMask {
    float center[2];
    float radius;
    float feather;
    uint32_t shape;
    uint32_t reserved;
} CseRadarMask;

typedef struct CseRadarWorldMaterial {
    uint32_t size;
    uint32_t flags;
    CseRadarTexture base;
    CseRadarTexture lightmap;
} CseRadarWorldMaterial;

typedef struct CseRadarGlyph {
    uint32_t size;
    uint32_t reserved;
    CseRadarTexture atlas;
    float uv[4]; /* u0, v0, u1, v1 */
    int32_t bearingX;
    int32_t width;
    int32_t height;
    int32_t advance;
} CseRadarGlyph;

typedef struct CseRadarMapVertex {
    float position[3];
    float baseUv[2];
    float lightmapUv[2];
    float color[4];
} CseRadarMapVertex;

typedef struct CseRadarMapDraw {
    uint32_t firstIndex;
    uint32_t indexCount;
    CseRadarTexture base;
    CseRadarTexture lightmap;
    uint32_t textureFlags;
    uint32_t reserved;
} CseRadarMapDraw;

typedef struct CseRadarMapPass {
    float minX;
    float minY;
    float maxX;
    float maxY;
    float minZ;
    float maxZ;
    uint32_t firstDraw;
    uint32_t drawCount;
    uint32_t depthOrder;
    uint32_t clearColor;
} CseRadarMapPass;

typedef struct CseRadarMapImageBuild {
    uint32_t size;
    uint32_t tone;
    uint64_t rendererEpoch;
    uint32_t vertexCount;
    uint32_t indexCount;
    uint32_t drawCount;
    uint32_t passCount;
    const CseRadarMapVertex *vertices;
    const uint32_t *indices;
    const CseRadarMapDraw *draws;
    const CseRadarMapPass *passes;
} CseRadarMapImageBuild;

/* Exactly the existing six-attribute, 22-float payload; no implicit padding. */
typedef struct CseRadarPrimitiveVertex {
    float position[2];
    float shape[4];
    float center[4];
    float color[4];
    float triangle[4];
    float triangle2[4];
} CseRadarPrimitiveVertex;

typedef struct CseRadarPrimitiveBatch {
    uint32_t size;
    uint32_t blend;
    uint64_t rendererEpoch;
    CseRadarHudSpace hud;
    CseRadarClip clip;
    CseRadarFade fade;
    CseRadarTexture atlas;
    uint32_t quadCount;
    uint32_t reserved;
    const CseRadarPrimitiveVertex *vertices; /* exactly quadCount * 4 */
} CseRadarPrimitiveBatch;

typedef struct CseRadarTexturedVertex {
    float position[2];
    float uv[2];
} CseRadarTexturedVertex;

typedef struct CseRadarMapTextureDraw {
    uint32_t size;
    uint32_t mode;
    uint64_t rendererEpoch;
    CseRadarHudSpace hud;
    CseRadarTexture texture;
    float modulation[4]; /* straight RGBA; service premultiplies for IMAGE mode */
    CseRadarMask mask;
    float gamma;
    uint32_t alphaAa;
    CseRadarFade fade;
    uint32_t vertexCount;
    uint32_t reserved;
    CseRadarTexturedVertex vertices[CSE_RADAR_TEXTURED_POLYGON_CAPACITY];
} CseRadarMapTextureDraw;

typedef struct CseRadarMaskRingDraw {
    uint32_t size;
    uint32_t reserved;
    uint64_t rendererEpoch;
    CseRadarHudSpace hud;
    CseRadarMask outer;
    float innerRadius;
    uint32_t reserved2;
    float color[4];
    CseRadarFade fade;
} CseRadarMaskRingDraw;

typedef struct CseRadarBackdropDraw {
    uint32_t size;
    uint32_t reserved;
    uint64_t rendererEpoch;
    CseRadarHudSpace hud;
    float x;
    float y;
    float extent;
    float darken;
    float opacity;
    uint32_t shape;
    CseRadarFade fade;
} CseRadarBackdropDraw;

typedef struct CseRadarRendererApi {
    uint32_t size;
    uint32_t version;
    uint32_t pointerBits;
    uint32_t reserved;

    CseRadarResult (CSE_ABI_CALL *GetState)(
        CseRadarState *outState, uint32_t outBytes);

    CseRadarResult (CSE_ABI_CALL *ResolveWorldMaterial)(
        uint64_t rendererEpoch, int32_t worldSurfaceIndex,
        CseRadarWorldMaterial *outMaterial, uint32_t outBytes);

    CseRadarResult (CSE_ABI_CALL *ResolveSpriteTexture)(
        uint64_t rendererEpoch, const void *spriteModel, int32_t frame,
        CseRadarTexture *outTexture);

    CseRadarResult (CSE_ABI_CALL *ResolveGlyph)(
        uint64_t rendererEpoch, uint32_t font, uint32_t character,
        CseRadarGlyph *outGlyph, uint32_t outBytes);

    CseRadarResult (CSE_ABI_CALL *BuildMapImage)(
        const CseRadarMapImageBuild *build, CseRadarTexture *outImage);

    CseRadarResult (CSE_ABI_CALL *ReleaseImage)(
        const CseRadarTexture *image);

    CseRadarResult (CSE_ABI_CALL *DrawPrimitiveQuads)(
        const CseRadarPrimitiveBatch *batch);

    CseRadarResult (CSE_ABI_CALL *DrawMapTexture)(
        const CseRadarMapTextureDraw *draw);

    CseRadarResult (CSE_ABI_CALL *DrawMaskRing)(
        const CseRadarMaskRingDraw *draw);

    CseRadarResult (CSE_ABI_CALL *DrawRadarBackdrop)(
        const CseRadarBackdropDraw *draw);
} CseRadarRendererApi;

#ifdef __cplusplus
}
#endif

#endif
