#include <cstddef>
#include <cstdint>

typedef float vec_t;
typedef float vec3_t[3];
#include "../../../GoldSrc-SDK/common/const.h"
#include "../../../GoldSrc-SDK/engine/progdefs.h"
#include "../../../GoldSrc-SDK/engine/edict.h"
#include "../../../GoldSrc-SDK/engine/eiface.h"

struct Blob { int first; short second; };
struct Snapshot {
    bool defusing;
    bool planted;
    unsigned short event;
    std::int64_t wide;
    void *links[8];
    Blob blob;
    float origin[CSE_SAVE_VECTOR_COMPONENTS];
};

int main() {
    const TYPEDESCRIPTION fields[] = {
        DEFINE_FIELD(Snapshot, defusing, FIELD_BOOLEAN),
        DEFINE_FIELD(Snapshot, planted, FIELD_BOOLEAN),
        DEFINE_FIELD(Snapshot, event, FIELD_INTEGER),
        DEFINE_FIELD(Snapshot, wide, FIELD_INTEGER),
        DEFINE_ARRAY(Snapshot, links, FIELD_POINTER, 2),
        DEFINE_ARRAY(Snapshot, blob, FIELD_CHARACTER, sizeof(Blob)),
        DEFINE_FIELD(Snapshot, origin, FIELD_VECTOR)
    };
    using namespace cse::save;
    if (fields[0].nativeStride != sizeof(bool) || fields[0].nativeFlags != NativeBoolean
        || fields[0].nativeBytes != sizeof(bool)) {
        return 1;
    }
    if (fields[2].nativeStride != sizeof(unsigned short) || fields[2].nativeFlags != 0) {
        return 2;
    }
    if (fields[3].nativeStride != sizeof(std::int64_t) || fields[3].nativeFlags != NativeSigned) {
        return 3;
    }
    // Saving a prefix of an array must not divide the full array by that count.
    if (fields[4].nativeStride != sizeof(void *) || fields[4].nativeBytes != sizeof(Snapshot::links)
        || fields[4].fieldSize != 2) {
        return 4;
    }
    // FIELD_CHARACTER also describes opaque struct blobs, not just C arrays.
    if (fields[5].nativeStride != sizeof(char) || fields[5].nativeBytes != sizeof(Blob)
        || static_cast<size_t>(fields[5].fieldSize) != sizeof(Blob)) {
        return 5;
    }
    if (fields[6].nativeStride != sizeof(Snapshot::origin) || fields[6].fieldSize != 1) {
        return 6;
    }
    return 0;
}
