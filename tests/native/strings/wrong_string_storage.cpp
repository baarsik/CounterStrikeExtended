// This source must fail to compile on native 64-bit targets.
// Keep the diagnostic specific so unrelated compiler failures cannot satisfy the fixture.
typedef float vec_t;
typedef float vec3_t[3];
#include "../../../src/common/const.h"
#include "../../../src/engine/progdefs.h"
#include "../../../src/engine/edict.h"
#include "../../../src/engine/eiface.h"

struct WrongStringStorage {
    unsigned int truncated;
};
const TYPEDESCRIPTION invalidField[] = {
    DEFINE_FIELD(WrongStringStorage, truncated, FIELD_STRING)
};
int main() {
    return invalidField[0].nativeStride == 0;
}
