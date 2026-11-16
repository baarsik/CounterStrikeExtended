#include "main.h"
#include "../../../public/radar_engine_api.h"
#include "../../../public/radar_renderer_api.h"

float g_MainViewOrigin[3]{};
float g_MainViewAngles[3]{};

namespace {
const cse::radar::EngineApi* engineApi = nullptr;
const CseRadarRendererApi* rendererApi = nullptr;
}

void NativeRadar_SetEngineFactory(CreateInterfaceFn factory) {
    const auto* renderer = factory ? static_cast<const CseRadarRendererApi*>(
        factory(CSE_RADAR_RENDERER_INTERFACE, nullptr)) : nullptr;
    rendererApi = renderer && renderer->size >= sizeof(*renderer) &&
        renderer->version == CSE_RADAR_RENDERER_VERSION && renderer->pointerBits == sizeof(void*) * 8 &&
        renderer->reserved == 0 && renderer->GetState && renderer->ResolveWorldMaterial &&
        renderer->ResolveSpriteTexture && renderer->ResolveGlyph && renderer->BuildMapImage &&
        renderer->ReleaseImage && renderer->DrawPrimitiveQuads && renderer->DrawMapTexture &&
        renderer->DrawMaskRing && renderer->DrawRadarBackdrop ? renderer : nullptr;

    const auto* candidate =
        factory ? static_cast<const cse::radar::EngineApi*>(factory(cse::radar::EngineInterface, nullptr)) : nullptr;
    engineApi = candidate && candidate->size >= sizeof(*candidate) &&
                        candidate->version == cse::radar::EngineApiVersion && candidate->Lightmaps &&
                        candidate->MessageNumber && candidate->GraphicsGeneration
                    ? candidate
                    : nullptr;
}

int NativeRadar_MessageNumber() {
    const cl_entity_t* local = gEngfuncs.GetLocalPlayer();
    return engineApi ? engineApi->MessageNumber() : (local ? local->curstate.messagenum : 0);
}

const int (*NativeRadar_Lightmaps())[64] {
    return engineApi ? reinterpret_cast<const int (*)[64]>(engineApi->Lightmaps()) : nullptr;
}

std::uint64_t NativeRadar_GraphicsGeneration() {
    return engineApi ? engineApi->GraphicsGeneration() : 0;
}

bool NativeRadar_GetGlyph(std::uint32_t font, std::uint32_t character, cse::radar::Glyph& out) {
    return engineApi && engineApi->GetGlyph && engineApi->GetGlyph(font, character, &out);
}

CseRadarState NativeRadar_RendererState() {
    CseRadarState state{};
    if (rendererApi) {
        rendererApi->GetState(&state, sizeof(state));
    }
    return state;
}

const CseRadarRendererApi* NativeRadar_Renderer() {
    return NativeRadar_RendererState().backend == CSE_RADAR_BACKEND_METAL ? rendererApi : nullptr;
}

void NativeRadar_ReportRenderResult(const char* operation, CseRadarResult result) {
    if (result != CSE_RADAR_OK) {
        gEngfuncs.Con_DPrintf("radar renderer %s failed: %d\n", operation, result);
    }
}

CseRadarHudSpace NativeRadar_HudSpace() {
    return {float(gHUD.m_scrinfo.iWidth), float(gHUD.m_scrinfo.iHeight)};
}
