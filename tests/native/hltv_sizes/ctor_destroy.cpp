#include "HLTV/common/Bitbuffer.h"

// A separate translation unit keeps virtual destruction independent of constructor devirtualization.
void DestroyThroughBase(BitBuffer* buffer) {
    delete buffer;
}
