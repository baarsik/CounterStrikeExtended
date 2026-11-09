#include <webm/mkvparser/mkvparser.h>

#include <algorithm>
#include <climits>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

namespace {

using Bytes = std::vector<unsigned char>;

void Append(Bytes& destination, const Bytes& source) {
    destination.insert(destination.end(), source.begin(), source.end());
}

Bytes UInt(std::uint64_t value) {
    Bytes result;
    do {
        result.insert(result.begin(), static_cast<unsigned char>(value & 0xFFu));
        value >>= CHAR_BIT;
    } while (value != 0);
    return result;
}

Bytes Vint(std::uint64_t value) {
    constexpr unsigned int payloadBitsPerByte = 7;
    unsigned int bytes = 1;
    while (value >= (std::uint64_t{1} << (payloadBitsPerByte * bytes)) - 1) {
        ++bytes;
    }
    Bytes result(bytes);
    for (unsigned int index = 0; index < bytes; ++index) {
        result[bytes - index - 1] = static_cast<unsigned char>(value & 0xFFu);
        value >>= CHAR_BIT;
    }
    result[0] |= static_cast<unsigned char>(1u << (CHAR_BIT - bytes));
    return result;
}

Bytes Element(std::uint64_t identifier, const Bytes& payload) {
    Bytes result = UInt(identifier);
    Append(result, Vint(payload.size()));
    Append(result, payload);
    return result;
}

Bytes UnsignedElement(std::uint64_t identifier, std::uint64_t value) {
    return Element(identifier, UInt(value));
}

class SparseReader final : public mkvparser::IMkvReader {
public:
    Bytes prefix;
    long long extent = 0;
    long largestRead = 0;
    unsigned int payloadReads = 0;

    int Read(long long pos, long len, unsigned char* buffer) override {
        if (pos < 0 || len < 0 || pos > extent || len > extent - pos) {
            return -1;
        }
        largestRead = std::max(largestRead, len);
        if (pos >= static_cast<long long>(prefix.size())) {
            ++payloadReads;
            constexpr long maximumFixturePayloadRead = 128;
            if (len > maximumFixturePayloadRead) {
                return -1;
            }
            std::memset(buffer, 0, static_cast<std::size_t>(len));
            return 0;
        }
        const auto available = prefix.size() - static_cast<std::size_t>(pos);
        const auto copied = std::min(available, static_cast<std::size_t>(len));
        std::memcpy(buffer, prefix.data() + static_cast<std::size_t>(pos), copied);
        if (copied < static_cast<std::size_t>(len)) {
            ++payloadReads;
            std::memset(buffer + copied, 0, static_cast<std::size_t>(len) - copied);
        }
        return 0;
    }

    int Length(long long* total, long long* available) override {
        *total = extent;
        *available = extent;
        return 0;
    }
};

SparseReader MakeContainer(unsigned int trackCount, unsigned int firstTrackNumber,
    std::uint64_t blockTrackNumber, std::uint64_t frameLength, const std::vector<unsigned int>& laterFrames = {}) {
    Bytes ebml;
    Append(ebml, UnsignedElement(0x4286, 1));
    Append(ebml, UnsignedElement(0x42F7, 1));
    Append(ebml, UnsignedElement(0x42F2, 4));
    Append(ebml, UnsignedElement(0x42F3, 8));
    Append(ebml, Element(0x4282, {'w', 'e', 'b', 'm'}));
    Append(ebml, UnsignedElement(0x4287, 2));
    Append(ebml, UnsignedElement(0x4285, 2));
    Bytes tracks;
    for (unsigned int index = 0; index < trackCount; ++index) {
        Bytes track;
        Append(track, UnsignedElement(0xD7, firstTrackNumber + index));
        Append(track, UnsignedElement(0x73C5, index + 1));
        Append(track, UnsignedElement(0x83, 1));
        Append(track, Element(0x86, {'V', '_', 'V', 'P', '8'}));
        Bytes video;
        Append(video, UnsignedElement(0xB0, 16));
        Append(video, UnsignedElement(0xBA, 16));
        Append(track, Element(0xE0, video));
        Append(tracks, Element(0xAE, track));
    }
    Bytes segment = Element(0x1549A966, UnsignedElement(0x2AD7B1, 1000000));
    Append(segment, Element(0x1654AE6B, tracks));
    Bytes block = Vint(blockTrackNumber);
    block.insert(block.end(), {0, 0, 0x80});
    Bytes cluster = UnsignedElement(0xE7, 0);
    Append(cluster, UInt(0xA3));
    Append(cluster, Vint(block.size() + frameLength));
    Append(cluster, block);
    if (!laterFrames.empty()) {
        // Small fully owned packets for the actual demuxer failure/retry fixture.
        cluster.insert(cluster.end(), static_cast<std::size_t>(frameLength), 0);
        for (unsigned int length : laterFrames) {
            Bytes packet = Vint(blockTrackNumber);
            packet.insert(packet.end(), {0, 0, 0x80});
            packet.insert(packet.end(), length, 0);
            Append(cluster, Element(0xA3, packet));
        }
        frameLength = 0;
    }
    Append(segment, UInt(0x1F43B675));
    Append(segment, Vint(cluster.size() + frameLength));
    Append(segment, cluster);
    SparseReader reader;
    reader.prefix = Element(0x1A45DFA3, ebml);
    Append(reader.prefix, UInt(0x18538067));
    Append(reader.prefix, Vint(segment.size() + frameLength));
    Append(reader.prefix, segment);
    reader.extent = static_cast<long long>(reader.prefix.size() + frameLength);
    return reader;
}

#if !defined(CSE_WEBM_NO_DOMAIN_MAIN)
void Probe(const char* name, unsigned int trackCount, unsigned int firstTrackNumber,
    std::uint64_t blockTrackNumber, std::uint64_t frameLength) {
    SparseReader reader = MakeContainer(trackCount, firstTrackNumber, blockTrackNumber, frameLength);
    mkvparser::EBMLHeader header;
    long long position = 0;
    const long long headerStatus = header.Parse(&reader, position);
    mkvparser::Segment* segment = nullptr;
    const long long createStatus = headerStatus == 0
        ? mkvparser::Segment::CreateInstance(&reader, position, segment) : -99;
    const long loadStatus = createStatus == 0 && segment != nullptr ? segment->Load() : -99;
    unsigned long storedTracks = 0;
    long storedTrack = 0;
    long firstStatus = -99;
    long long parsedBlockTrack = 0;
    long parsedFrameLength = 0;
    if (loadStatus == 0) {
        const auto* tracks = segment->GetTracks();
        storedTracks = tracks->GetTracksCount();
        if (storedTracks != 0) {
            storedTrack = tracks->GetTrackByIndex(0)->GetNumber();
        }
        const auto* cluster = segment->GetFirst();
        if (cluster != nullptr && !cluster->EOS()) {
            const mkvparser::BlockEntry* entry = nullptr;
            firstStatus = cluster->GetFirst(entry);
            if (firstStatus == 0 && entry != nullptr && !entry->EOS()) {
                const auto* block = entry->GetBlock();
                parsedBlockTrack = block->GetTrackNumber();
                parsedFrameLength = block->GetFrame(0).len;
            }
        }
    }
    std::printf("%s,%lld,%lld,%ld,%lu,%ld,%ld,%lld,%ld,%ld,%u\n", name, headerStatus,
        createStatus, loadStatus, storedTracks, storedTrack, firstStatus, parsedBlockTrack,
        parsedFrameLength, reader.largestRead, reader.payloadReads);
    delete segment;
}
#endif

}

#if !defined(CSE_WEBM_NO_DOMAIN_MAIN)
int main() {
    std::printf("pointer_bytes=%zu,long_bytes=%zu\n", sizeof(void*), sizeof(long));
    std::puts("case,header,create,load,tracks,first_track,first_block_status,block_track,frame_len,largest_read,payload_reads");
    Probe("normal", 1, 1, 1, 1);
    Probe("track-zero", 1, 0, 1, 1);
    Probe("track-127", 1, 127, 127, 1);
    Probe("track-128", 1, 128, 128, 1);
    Probe("tracks-127", 127, 1, 1, 1);
    Probe("tracks-128", 128, 1, 1, 1);
    Probe("block-low32-alias", 1, 1, 0x100000001ull, 1);
    Probe("frame-zero", 1, 1, 1, 0);
    Probe("frame-int-max", 1, 1, 1, INT_MAX);
    Probe("frame-int-max-plus-one", 1, 1, 1, static_cast<std::uint64_t>(INT_MAX) + 1);
    return 0;
}
#endif
