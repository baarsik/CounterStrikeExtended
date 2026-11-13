#pragma once

#include <algorithm>
#include <array>

namespace cse {

// The engine DMA cursor counts mono 16-bit samples, even for stereo output.
class AudioRing {
public:
    enum class PlaybackPolicy {
        LegacyByteZero,
        PreserveSamples
    };

    static constexpr int BufferBytes = 64 * 1024;
    static constexpr int BytesPerMonoSample = 2;

    void Reset() {
        bytes_.fill(0);
        position_ = 0;
    }

    unsigned char *Data() {
        return bytes_.data();
    }

    int MonoSamplePosition() const {
        return position_ / BytesPerMonoSample;
    }

    template <typename Sink>
    bool Drain(int requestedBytes, Sink sink, PlaybackPolicy policy = PlaybackPolicy::LegacyByteZero) {
        while (requestedBytes > 0) {
            const int segmentBytes = std::min(requestedBytes, BufferBytes - position_);
            // Retained legacy stream fixtures keep their original byte-zero contract.
            // Native effects preserve every byte produced by the shared mixer.
            if (policy == PlaybackPolicy::LegacyByteZero) {
                bytes_[0] = 0;
            }
            if (!sink(bytes_.data() + position_, segmentBytes)) {
                return false;
            }
            position_ = (position_ + segmentBytes) % BufferBytes;
            requestedBytes -= segmentBytes;
        }
        return true;
    }

private:
    std::array<unsigned char, BufferBytes> bytes_{};
    int position_ = 0;
};

}
