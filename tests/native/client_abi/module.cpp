#include "fixture_types.h"
#include "../../../src/public/interface.h"
#include "../../../src/common/client_module_entry.h"

#if defined(_WIN32)
#define FIXTURE_EXPORT __declspec(dllexport)
#else
#define FIXTURE_EXPORT __attribute__((visibility("default")))
#endif
#define CL_DLLEXPORT FIXTURE_EXPORT
#define EXPORT FIXTURE_EXPORT
#ifndef FALSE
#define FALSE 0
#endif
#define ASSERT_AT(expression, line) do { if (!(expression)) { std::abort(); } } while (false)

struct IFileSystem;
struct ICareerUI;
struct IGameUI;
struct IGameConsole;
struct IMusicManager;
struct IFriendsUser;
class CSysModule;
cl_enginefunc_t gEngfuncs = {};
modfuncs_t g_modfuncs = {};
cldll_func_dst_t* g_pcldstAddrs = nullptr;
CSysModule* g_hFileSystemModule = nullptr;
IFileSystem* g_pFileSystem = nullptr;
CSysModule* g_hGameUIModule = nullptr;
ICareerUI* g_pCareerUI = nullptr;
IGameUI* g_pGameUI = nullptr;
IGameConsole* g_pGameConsole = nullptr;
IMusicManager* g_pMusicManager = nullptr;
CSysModule* g_hTrackerModule = nullptr;
IFriendsUser* g_pFriendsUser = nullptr;
engine_studio_api_t IEngineStudio = {};
int events = 0;
int particles = 0;
int studioCalls = 0;
const char* observedString = nullptr;
player_info_t* observedPlayer = nullptr;

void EV_HookEvents() {
    ++events;
    if (gEngfuncs.pfnGetGameDirectory) {
        observedString = gEngfuncs.pfnGetGameDirectory();
    }
}
void CL_LoadParticleMan() {
    ++particles;
}
void R_StudioInit() {
    ++studioCalls;
    if (IEngineStudio.PlayerInfo) {
        observedPlayer = IEngineStudio.PlayerInfo(1);
        observedPlayer->gaitframe = 42.5f;
    }
}
void* FixtureFileSystem(const char*, int*) {
    return &gEngfuncs;
}
CSysModule* Sys_LoadModule(const char*) {
    return reinterpret_cast<CSysModule*>(&gEngfuncs);
}
CreateInterfaceFn Sys_GetFactory(CSysModule*) {
    return FixtureFileSystem;
}

template<typename Type>
struct FixtureDefaultValue {
    static Type Get() {
        return Type{};
    }
};
template<>
struct FixtureDefaultValue<void> {
    static void Get() {}
};
template<typename Type>
struct FixtureDefaultFunction;
template<typename Result, typename... Arguments>
struct FixtureDefaultFunction<Result (*)(Arguments...)> {
    static Result Call(Arguments...) {
        return FixtureDefaultValue<Result>::Get();
    }
};

extern "C" FIXTURE_EXPORT int Initialize(cl_enginefunc_t*, int);
extern "C" FIXTURE_EXPORT int HUD_GetStudioModelInterface(int, r_studio_interface_t**, engine_studio_api_t*);
r_studio_interface_t studio = {STUDIO_INTERFACE_VERSION,
    &FixtureDefaultFunction<decltype(r_studio_interface_t::StudioDrawModel)>::Call,
    &FixtureDefaultFunction<decltype(r_studio_interface_t::StudioDrawPlayer)>::Call};
#if defined(CSE_CLIENT_ABI_CS)
#include "cs_stubs.inc"
extern "C" {
#include "cs_entries.inc"
}
#else
#include "hl_stubs.inc"
extern "C" {
#include "hl_entries.inc"
}
#endif

#define CseGetModuleAbi CseProductionModuleAbi
#define CseClientConsumeFunctionAuthorization CseProductionConsumeFunctionAuthorization
#if CSE_CLIENT_FIXTURE_FAULT == 14
#define CseInitializeClientModule CseHiddenInitializeClientModule
#endif
#if CSE_CLIENT_FIXTURE_FAULT == 15
#define CseExchangeClientFunctions CseProductionExchangeClientFunctions
#endif
#include "../../../src/common/client_module_entry_impl.h"
#undef CseGetModuleAbi
#undef CseClientConsumeFunctionAuthorization
#if CSE_CLIENT_FIXTURE_FAULT == 14
#undef CseInitializeClientModule
#endif
#if CSE_CLIENT_FIXTURE_FAULT == 15
#undef CseExchangeClientFunctions
#endif
int functionExchanges = 0;
bool CseClientConsumeFunctionAuthorization() {
    const bool authorized = CseProductionConsumeFunctionAuthorization();
    if (authorized) {
        ++functionExchanges;
    }
    return authorized;
}
#if CSE_CLIENT_FIXTURE_FAULT == 15
extern "C" FIXTURE_EXPORT unsigned int CSE_ABI_CALL CseExchangeClientFunctions(
    const cse::abi::Descriptor* engine, cldll_func_t* functions) {
    const unsigned int result = CseProductionExchangeClientFunctions(engine, functions);
    if (result) {
        functions->pHudInitFunc = nullptr;
    }
    return result;
}
#endif

#if CSE_CLIENT_FIXTURE_FAULT != 8
extern "C" FIXTURE_EXPORT const cse::abi::Descriptor* CSE_ABI_CALL CseGetModuleAbi() {
    static const cse::abi::Descriptor descriptor = []() {
        auto result = cse::abi::ClientModuleDescriptor;
#if CSE_CLIENT_FIXTURE_FAULT == 1
        ++result.runtimeVersion;
#elif CSE_CLIENT_FIXTURE_FAULT == 2
        result.tables[0].signature = static_cast<std::uint64_t>(cse::abi::ClientRole::Engine);
#elif CSE_CLIENT_FIXTURE_FAULT == 3
        result.tables[1].signature ^= 1;
#elif CSE_CLIENT_FIXTURE_FAULT == 4
        result.tables[result.tableCount++] = result.tables[0];
#elif CSE_CLIENT_FIXTURE_FAULT == 5
        result.tables[result.tableCount++] = {0x7FFF, 1, 0};
#elif CSE_CLIENT_FIXTURE_FAULT == 6
        result.tableCount = static_cast<std::uint32_t>(cse::abi::MaxTables + 1);
#elif CSE_CLIENT_FIXTURE_FAULT == 7
        result.tables[result.tableCount++] = {0x7FFF, 0, 7};
#elif CSE_CLIENT_FIXTURE_FAULT == 9
        --result.descriptorVersion;
#elif CSE_CLIENT_FIXTURE_FAULT == 10
        result.pointerBits = 0;
#elif CSE_CLIENT_FIXTURE_FAULT == 11
        result.compilerAbi = 0;
#elif CSE_CLIENT_FIXTURE_FAULT == 12
        result.architecture = 0;
#elif CSE_CLIENT_FIXTURE_FAULT == 13
        --result.descriptorBytes;
#endif
        return result;
    }();
    return &descriptor;
}
#endif

extern "C" FIXTURE_EXPORT int CSE_ABI_CALL CseFixtureEffects() {
    return events + particles + studioCalls;
}
extern "C" FIXTURE_EXPORT const void* CSE_ABI_CALL CseFixtureString() {
    return observedString;
}
extern "C" FIXTURE_EXPORT const void* CSE_ABI_CALL CseFixturePlayer() {
    return observedPlayer;
}
extern "C" FIXTURE_EXPORT std::size_t CSE_ABI_CALL CseFixtureMovementSize() {
    return sizeof(playermove_t);
}
extern "C" FIXTURE_EXPORT std::size_t CSE_ABI_CALL CseFixturePlayerSize() {
    return sizeof(player_info_t);
}
extern "C" FIXTURE_EXPORT const void* CSE_ABI_CALL CseFixtureCopiedEngine() {
    return &gEngfuncs;
}
extern "C" FIXTURE_EXPORT int CSE_ABI_CALL CseFixtureMouse(int *x, int *y, int *centerX, int *centerY) {
    if (!CseClientEngineAbiBound() || !gEngfuncs.pfnGetClientMousePositionAndCenter) {
        return -1;
    }
    return static_cast<int>(gEngfuncs.pfnGetClientMousePositionAndCenter(x, y, centerX, centerY));
}
#include "movement_diagnostics.inc"
extern "C" FIXTURE_EXPORT unsigned long long CSE_ABI_CALL CseFixtureMovementFact(unsigned int index) {
    return index < sizeof(fixtureLayoutFacts) / sizeof(fixtureLayoutFacts[0]) ? fixtureLayoutFacts[index].value : 0;
}

const void* observedHull = nullptr;
extern "C" FIXTURE_EXPORT int CSE_ABI_CALL CseFixtureMovement(playermove_t* move) {
    if (!CseClientEngineAbiBound()) {
        return 0;
    }
    float start[3] = {};
    float end[3] = {1, 2, 3};
    const pmtrace_t trace = move->PM_PlayerTrace(start, end, 7, 11);
    move->origin[0] = trace.endpos[0];
    float offset[3] = {};
    const hull_t* hull = move->PM_HullForBsp(&move->physents[0], offset);
    observedHull = hull;
    move->origin[1] = hull->clip_mins[1];
    move->Con_DPrintf("owned %d %.1f", 42, 2.5);
    move->Con_Printf("owned %d %.1f", 42, 2.5);
    return trace.fraction == 0.5f && offset[2] == 9;
}
extern "C" FIXTURE_EXPORT const void* CSE_ABI_CALL CseFixtureHull() {
    return observedHull;
}
extern "C" FIXTURE_EXPORT const void* CSE_ABI_CALL CseFixtureDestination() {
    return g_pcldstAddrs;
}

extern "C" FIXTURE_EXPORT int CSE_ABI_CALL CseFixtureExchanges() {
    return functionExchanges;
}

extern "C" FIXTURE_EXPORT std::size_t CSE_ABI_CALL CseFixtureTableSize(unsigned int identifier) {
    switch (static_cast<cse::abi::ClientContract>(identifier)) {
    case cse::abi::ClientContract::EngineFunctions:
        return sizeof(cl_enginefunc_t);
    case cse::abi::ClientContract::NestedFunctions:
        return sizeof(event_api_t);
    case cse::abi::ClientContract::ExchangeFunctions:
        return sizeof(cldll_func_dst_t);
    default:
        return 0;
    }
}
extern "C" FIXTURE_EXPORT const void* CSE_ABI_CALL CseFixtureModifiers() {
    return &g_modfuncs;
}
extern "C" FIXTURE_EXPORT int CSE_ABI_CALL CseFixtureVariant() {
#if defined(CSE_CLIENT_ABI_CS)
    return 1;
#else
    return 0;
#endif
}
