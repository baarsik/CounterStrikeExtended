typedef float vec_t;
typedef float vec3_t[3];
#include "../../../GoldSrc-SDK/common/const.h"
#include "../../../GoldSrc-SDK/engine/progdefs.h"
#include "../../../GoldSrc-SDK/engine/edict.h"
#include "../../../GoldSrc-SDK/engine/eiface.h"

struct WrongEntityStorage {
    int truncated;
};
const TYPEDESCRIPTION invalidField[] = {
    DEFINE_FIELD(WrongEntityStorage, truncated, FIELD_ENTITY)
};
int main() {
    return invalidField[0].nativeStride == 0;
}
