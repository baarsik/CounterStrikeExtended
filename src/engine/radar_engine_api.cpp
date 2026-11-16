#include "quakedef.h"
#include "gl_model.h"
#include "client.h"
#include "../public/radar_engine_api.h"
#include "vgui2/BaseUISurface.h"
#include "../vgui2/vgui_surfacelib/FontTextureCache.h"

extern BaseUISurface g_BaseUISurface;
extern int currenttexture;

extern int lightmap_textures[cse::radar::LightmapCount];

namespace {
std::uint64_t graphicsGeneration = 1;
const int* CSE_ABI_CALL RadarLightmaps() {
    return lightmap_textures;
}

int CSE_ABI_CALL RadarMessageNumber() {
    return cl.parsecount;
}

std::uint64_t CSE_ABI_CALL RadarGraphicsGeneration() {
    return graphicsGeneration;
}

bool CSE_ABI_CALL RadarGlyph(std::uint32_t font, std::uint32_t character, cse::radar::Glyph* out) {
    constexpr std::uint32_t kMaximumBmpCharacter = 0xffff;
    if (!font || !out || character > kMaximumBmpCharacter) {
        return false;
    }
    int texture = 0;
    const int previousTexture = currenttexture;
    const bool ready = g_FontTextureCache.GetTextureForChar(font, static_cast<wchar_t>(character), &texture, out->uv);
    // Baking a new glyph binds its atlas through the engine surface. Keep its
    // cached texture identity and the actual binding synchronized for callers.
    g_BaseUISurface.DrawSetTexture(previousTexture);
    if (!ready) {
        return false;
    }
    int left = 0;
    int width = 0;
    int right = 0;
    g_BaseUISurface.GetCharABCwide(font, static_cast<int>(character), left, width, right);
    out->texture = static_cast<std::uint32_t>(texture);
    out->bearing_x = left;
    out->wide = width;
    out->tall = g_BaseUISurface.GetFontTall(font);
    out->advance = left + width + right;
    return true;
}

cse::radar::EngineApi radarApi{sizeof(radarApi),   cse::radar::EngineApiVersion, RadarLightmaps,
                               RadarMessageNumber, RadarGraphicsGeneration,      RadarGlyph};

void* CreateRadarEngine() {
    return &radarApi;
}

using cse::radar::EngineApi;
EXPOSE_INTERFACE_FN(CreateRadarEngine, EngineApi, cse::radar::EngineInterface);
} // namespace

void cse::radar::AdvanceGraphicsGeneration() {
    ++graphicsGeneration;
    // Zero denotes an engine without the optional generation service.
    if (graphicsGeneration == 0) {
        ++graphicsGeneration;
    }
}
