typedef float vec_t;
typedef float vec3_t[3];
#include "../../../src/common/const.h"
#include "../../../src/engine/progdefs.h"
#include "../../../src/engine/edict.h"
#include "../../../src/engine/eiface.h"

struct RawPointerStorage {
    void *pointer;
};
const TYPEDESCRIPTION invalidField[] = {
    DEFINE_FIELD(RawPointerStorage, pointer, FIELD_POINTER)
};
int main() {
    return invalidField[0].nativeStride == 0;
}
