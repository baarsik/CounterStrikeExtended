typedef float vec_t;
typedef float vec3_t[3];
#include "../../../src/common/const.h"
#include "../../../src/engine/progdefs.h"
#include "../../../src/engine/edict.h"
#include "../../../src/engine/eiface.h"

struct WrongEntityStorage {
    int truncated;
};
const TYPEDESCRIPTION invalidField[] = {
    DEFINE_FIELD(WrongEntityStorage, truncated, FIELD_ENTITY)
};
int main() {
    return invalidField[0].nativeStride == 0;
}
