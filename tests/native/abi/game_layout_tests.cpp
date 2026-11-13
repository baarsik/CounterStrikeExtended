#include "../../../src/public/cse_module_abi.h"

typedef float vec_t;
typedef float vec3_t[3];
#include "../../../src/common/const.h"
#include "../../../src/engine/progdefs.h"
#include "../../../src/engine/edict.h"
#include "../../../src/engine/eiface.h"
#include "../../../src/common/generated_game_module_layouts.h"

int main() {
    using namespace cse::abi;
    const auto expected = GameModuleDescriptor;
    if (Check(expected, &expected) != Compatibility::Compatible) {
        return 1;
    }
    auto stale = expected;
    // A same-sized type with changed field layout must fail before exchange.
    stale.tables[static_cast<unsigned>(GameTable::SaveField) - 1].signature ^= 1;
    if (Check(expected, &stale) != Compatibility::TableLayout) {
        return 2;
    }
    return 0;
}
