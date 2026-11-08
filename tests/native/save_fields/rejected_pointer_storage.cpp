typedef float vec_t;
typedef float vec3_t[3];
#include "../../../GoldSrc-SDK/common/const.h"
#include "../../../GoldSrc-SDK/engine/progdefs.h"
#include "../../../GoldSrc-SDK/engine/edict.h"
#include "../../../GoldSrc-SDK/engine/eiface.h"

struct RawPointerStorage {
    void *pointer;
};
const TYPEDESCRIPTION invalidField[] = {
    DEFINE_FIELD(RawPointerStorage, pointer, FIELD_POINTER)
};
int main() {
    return invalidField[0].nativeStride == 0;
}
