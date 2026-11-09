#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>

#include "HLTV/common/Bitbuffer.h"

extern "C" void* Mem_ZeroMalloc(unsigned int bytes) {
    return std::calloc(bytes, 1);
}

#include "actual_ctor_impl.inc"

void DestroyThroughBase(BitBuffer* buffer);

int main(int argc, char** argv) {
    try {
        if (argc != 2) {
            throw std::runtime_error("Expected a constructor scenario");
        }
        constexpr unsigned int PayloadBytes = 16;
        std::array<unsigned char, PayloadBytes> external{};
        BitBuffer* buffer = nullptr;
        if (std::strcmp(argv[1], "default") == 0) {
            buffer = new BitBuffer;
            if (buffer->sizeError || buffer->data != nullptr || buffer->currentByte != nullptr
                || buffer->currentBit != 0 || buffer->maxSize != 0) {
                throw std::runtime_error("Default constructor field state changed");
            }
        } else if (std::strcmp(argv[1], "owned") == 0) {
            buffer = new BitBuffer(PayloadBytes);
            if (buffer->sizeError || buffer->data == nullptr || buffer->currentByte != buffer->data
                || buffer->currentBit != 0 || buffer->maxSize != static_cast<int>(PayloadBytes)) {
                throw std::runtime_error("Owned constructor field state changed");
            }
        } else if (std::strcmp(argv[1], "external") == 0) {
            buffer = new BitBuffer(external.data(), PayloadBytes);
            if (buffer->sizeError || buffer->data != external.data() || buffer->currentByte != external.data()
                || buffer->currentBit != 0 || buffer->maxSize != static_cast<int>(PayloadBytes)) {
                throw std::runtime_error("External constructor field state changed");
            }
        } else {
            throw std::runtime_error("Unknown constructor scenario");
        }
        std::printf("Destroying actual %s BitBuffer through base pointer %p size=%zu alignment=%zu\n",
            argv[1], static_cast<void*>(buffer), sizeof(BitBuffer), alignof(BitBuffer));
        std::fflush(stdout);
        DestroyThroughBase(buffer);
        std::puts("PASS actual constructor and virtual destruction");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "FAIL: %s\n", error.what());
        return 1;
    }
}
