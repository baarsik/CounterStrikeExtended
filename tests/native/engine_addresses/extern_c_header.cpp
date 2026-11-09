// cdll_int.h reaches progs.h through the C-linkage SDK declarations before any C++ <cstddef> include.
#include "mathlib.h"
#include "../../../GoldSrc-SDK/common/const.h"
extern "C" {
#include "selected_progs.h"
}

static_assert(offsetof(edict_t, area) == 2 * sizeof(int), "Edict link follows free and serial fields");

int main() {
    return 0;
}
