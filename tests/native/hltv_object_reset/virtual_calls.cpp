#include "HLTV/common/NetAddress.h"
#include "HLTV/common/Bitbuffer.h"

// Separate translation unit and no LTO: these are actual virtual destructor calls through real base types.
void DestroyAddressVirtually(NetAddress* address) {
    address->~NetAddress();
}

void DestroyBufferVirtually(BitBuffer* buffer) {
    buffer->~BitBuffer();
}
