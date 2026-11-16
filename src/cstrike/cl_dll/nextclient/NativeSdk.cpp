#include "main.h"
#include "../../../public/radar_engine_api.h"

float g_MainViewOrigin[3]{};
float g_MainViewAngles[3]{};

namespace {
const cse::radar::EngineApi* engineApi = nullptr;
}

void NativeRadar_SetEngineFactory(CreateInterfaceFn factory) {
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
