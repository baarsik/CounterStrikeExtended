#include "../../../GoldSrc-SDK/public/cse_module_abi.h"

typedef float vec_t;
typedef float vec3_t[3];
#include "../../../GoldSrc-SDK/common/const.h"
#include "../../../GoldSrc-SDK/engine/progdefs.h"
#include "../../../GoldSrc-SDK/engine/edict.h"
#include "../../../GoldSrc-SDK/engine/eiface.h"
#include "../../../GoldSrc-SDK/common/generated_game_module_layouts.h"

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
