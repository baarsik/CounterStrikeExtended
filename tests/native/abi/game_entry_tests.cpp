#include "../../../src/public/cse_module_abi.h"
#include <cstdio>
#include <cstdlib>

typedef float vec_t;
typedef float vec3_t[3];
#include "../../../src/common/const.h"
#include "../../../src/engine/progdefs.h"
#include "../../../src/engine/edict.h"
#include "../../../src/engine/eiface.h"
#include "../../../src/common/game_module_entry.h"

static int exchanges = 0;
extern "C" void CSE_GAME_GIVE_CALL GiveFnptrsToDll(enginefuncs_t *, globalvars_t *) {
    if (!CseGameConsumeEngineExchangeAuthorization()) {
        return;
    }
    if (CseGameEngineAbiBound()) {
        std::abort();
    }
    ++exchanges;
}

#include "../../../src/common/game_module_entry_impl.h"

int main() {
    using namespace cse::abi;
    if (CseGameEngineAbiBound() || exchanges != 0) {
        return 1;
    }
    enginefuncs_t functions{};
    globalvars_t globals{};
    auto stale = GameModuleDescriptor;
    stale.tables[static_cast<unsigned>(GameTable::SaveField) - 1].signature ^= 1;
    if (CseInitializeGameModule(&stale, &functions, &globals) || CseGameEngineAbiBound() || exchanges != 0) {
        return 2;
    }
    if (CseInitializeGameModule(nullptr, &functions, &globals) || exchanges != 0) {
        return 3;
    }
    if (CseInitializeGameModule(&GameModuleDescriptor, nullptr, &globals) || exchanges != 0) {
        return 4;
    }
    if (!CseInitializeGameModule(&GameModuleDescriptor, &functions, &globals)
        || !CseGameEngineAbiBound() || exchanges != 1) {
        return 5;
    }
    GiveFnptrsToDll(&functions, &globals);
    if (exchanges != 1 || !CseGameEngineAbiBound()) {
        return 6;
    }
    return 0;
}
