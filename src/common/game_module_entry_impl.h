// Included after the actual engine/game shared declarations.
#include "generated_game_module_layouts.h"
#include "game_module_entry.h"

#if defined(_WIN32)
#define CSE_GAME_ENTRY_EXPORT __declspec(dllexport)
#else
#define CSE_GAME_ENTRY_EXPORT __attribute__((visibility("default")))
#endif

namespace {
bool gameEngineAbiBound = false;
bool engineExchangeAuthorized = false;
}

bool CseGameEngineAbiBound() {
    return gameEngineAbiBound;
}

bool CseGameConsumeEngineExchangeAuthorization() {
    const bool authorized = engineExchangeAuthorized;
    engineExchangeAuthorized = false;
    return authorized;
}

extern "C" void CSE_GAME_GIVE_CALL GiveFnptrsToDll(enginefuncs_t *, globalvars_t *);

extern "C" CSE_GAME_ENTRY_EXPORT const cse::abi::Descriptor *CSE_GAME_ENTRY_CALL CseGetModuleAbi() {
    return &cse::abi::GameModuleDescriptor;
}

extern "C" CSE_GAME_ENTRY_EXPORT unsigned int CSE_GAME_ENTRY_CALL CseInitializeGameModule(
    const cse::abi::Descriptor *engineAbi, enginefuncs_t *functions, globalvars_t *globals) {
    gameEngineAbiBound = false;
    engineExchangeAuthorized = false;
    if (!functions || !globals
        || cse::abi::Check(cse::abi::GameModuleDescriptor, engineAbi) != cse::abi::Compatibility::Compatible) {
        return 0;
    }
    // The compatibility entry point and all API table exports require this
    // handshake. No engine callback or shared native record is read before it.
    engineExchangeAuthorized = true;
    GiveFnptrsToDll(functions, globals);
    if (engineExchangeAuthorized) {
        engineExchangeAuthorized = false;
        return 0;
    }
    gameEngineAbiBound = true;
    return 1;
}

#undef CSE_GAME_ENTRY_EXPORT
