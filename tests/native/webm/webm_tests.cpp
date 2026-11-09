#include "webm_video.h"
#include CSE_WEBM_SOURCE

#include <cstdio>
#include <algorithm>
#include <climits>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <string>
#include <vector>

#if defined(_WIN32)
#include "unused_presentation_bindings.h"
#endif

namespace {

constexpr int fixtureWidth = 16;
constexpr int fixtureHeight = 16;
constexpr int expectedVideoFrames = 8;
constexpr int expectedAudioPackets = 23;
constexpr int expectedChannels = 2;
constexpr double expectedSampleRate = 48000;
constexpr double videoFrameSeconds = 0.04;
constexpr double timestampTolerance = 0.000001;
constexpr double minimumFixtureDuration = 0.27;
constexpr double maximumFixtureDuration = 0.34;

void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "WebM fixture failure: %s\n", message);
        std::exit(EXIT_FAILURE);
    }
}

std::vector<unsigned char> ReadBytes(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    Require(input.good(), "open known owned fixture bytes");
    return std::vector<unsigned char>(std::istreambuf_iterator<char>(input), {});
}

std::uint64_t Hash(const std::vector<unsigned char>& bytes) {
    constexpr std::uint64_t offset = 14695981039346656037ull;
    constexpr std::uint64_t prime = 1099511628211ull;
    std::uint64_t value = offset;
    for (unsigned char byte : bytes) {
        value = (value ^ byte) * prime;
    }
    return value;
}

void TestSizeBoundary() {
    constexpr unsigned int sentinelSize = 17;
    unsigned int size = sentinelSize;
    Require(StoreAudioPrivateSize(0, size) && size == 0, "empty codec-private size retained");
    Require(StoreAudioPrivateSize(UINT_MAX, size) && size == UINT_MAX, "largest representable size retained");
#if SIZE_MAX > UINT_MAX
    Require(!StoreAudioPrivateSize(static_cast<std::size_t>(UINT_MAX) + 1, size) && size == 0,
        "oversized native codec-private data rejected before narrowing");
    Require(!StoreAudioPrivateSize(std::numeric_limits<std::size_t>::max(), size) && size == 0,
        "maximum native codec-private size cannot wrap");
#endif
}

void AppendImage(VPXDecoder& decoder, std::vector<unsigned char>& pixels, int& images) {
    VPXDecoder::Image image = {};
    for (;;) {
        const auto result = decoder.GetImage(image);
        if (result == VPXDecoder::VPX_NO_FRAME) {
            return;
        }
        Require(result == VPXDecoder::VPX_NO_ERROR && image.w == fixtureWidth && image.h == fixtureHeight,
            "actual decoder returns the known native image dimensions");
        Require(image.chromaShiftW == 1 && image.chromaShiftH == 1, "known I420 chroma subsampling retained");
        for (int plane = 0; plane < 3; ++plane) {
            Require(image.GetWidth(plane) == (plane == 0 ? fixtureWidth : fixtureWidth / 2)
                && image.GetHeight(plane) == (plane == 0 ? fixtureHeight : fixtureHeight / 2),
                "production image accessors retain each plane's dimensions");
            for (int y = 0; y < image.GetHeight(plane); ++y) {
                const auto* row = image.planes[plane] + y * image.linesize[plane];
                pixels.insert(pixels.end(), row, row + image.GetWidth(plane));
            }
        }
        ++images;
    }
}

void ComparePCM(const std::vector<unsigned char>& actual, const std::vector<unsigned char>& reference) {
    Require(actual.size() == reference.size() && !actual.empty(), "decoded PCM frame count matches known reference");
    constexpr int maximumSampleDifference = 1;
    int largestDifference = 0;
    int differingChannels = 0;
    for (std::size_t index = 0; index < actual.size(); index += sizeof(std::int16_t)) {
        std::int16_t actualSample = 0;
        std::int16_t expectedSample = 0;
        std::memcpy(&actualSample, actual.data() + index, sizeof(actualSample));
        std::memcpy(&expectedSample, reference.data() + index, sizeof(expectedSample));
        largestDifference = std::max(largestDifference, std::abs(static_cast<int>(actualSample) - expectedSample));
        if (index % (expectedChannels * sizeof(std::int16_t)) == 0
            && std::memcmp(actual.data() + index, actual.data() + index + sizeof(std::int16_t),
                sizeof(std::int16_t)) != 0) {
            ++differingChannels;
        }
    }
    Require(largestDifference <= maximumSampleDifference && differingChannels > 0,
        "production PCM preserves asymmetric stereo channel order and sample values");
    std::printf("PCM bytes=%zu hash=%016llx largest_sample_delta=%d\n", actual.size(),
        static_cast<unsigned long long>(Hash(actual)), largestDifference);
}

void Decode(const std::filesystem::path& base, unsigned int threads) {
    mkvparser::MkvReader reader;
    Require(reader.Open((base.string() + ".webm").c_str()) == 0, "open actual WebM reader");
    WebMDemuxer demuxer(&reader, 0, 0);
    Require(demuxer.m_bIsOpen && demuxer.GetWidth() == fixtureWidth && demuxer.GetHeight() == fixtureHeight,
        "actual libwebm tracks preserve known dimensions");
    Require(demuxer.GetVideoCodec() == (base.filename() == "vp9" ? WebMDemuxer::VIDEO_VP9 : WebMDemuxer::VIDEO_VP8),
        "production demuxer selects expected VPX codec");
    Require(demuxer.GetAudioCodec() == WebMDemuxer::AUDIO_VORBIS && demuxer.GetChannels() == expectedChannels
        && demuxer.GetSampleRate() == expectedSampleRate, "production demuxer preserves Vorbis stereo metadata");
    Require(demuxer.GetLength() >= minimumFixtureDuration && demuxer.GetLength() <= maximumFixtureDuration,
        "known bounded media duration retained");
    unsigned int privateSize = 0;
    Require(demuxer.GetAudioExtradata(privateSize) != nullptr && privateSize > 2,
        "native libwebm codec-private pointer and checked public size reach actual decoder");
    VPXDecoder video(demuxer, threads);
    Require(video.m_pCtx != nullptr, "initialize actual VPX decoder");
    AudioVorbisDecoder audio(demuxer);
    Require(audio.IsOpen(), "initialize actual Vorbis decoder from native libwebm headers");
    WebMFrame videoFrame;
    WebMFrame audioFrame;
    std::vector<unsigned char> pixels;
    std::vector<unsigned char> pcm;
    int videoPackets = 0;
    int audioPackets = 0;
    int images = 0;
    double priorAudioTime = 0;
    Require(!demuxer.ReadFrame(nullptr, nullptr), "empty output request does not advance actual demuxer");
    while (demuxer.ReadFrame(&videoFrame, &audioFrame)) {
        if (videoFrame.m_uBufferSize > 0) {
            Require(std::abs(videoFrame.m_flTime - videoPackets * videoFrameSeconds) < timestampTolerance,
                "video packet timestamps retain container order and scale");
            if (videoPackets == 0) {
                Require(videoFrame.m_bKey, "initial video packet retains keyframe metadata");
                WebMFrame copy;
                copy = videoFrame;
                Require(copy.m_pBuffer != videoFrame.m_pBuffer && copy.m_uBufferSize == videoFrame.m_uBufferSize
                    && copy.m_uBufferCapacity == videoFrame.m_uBufferCapacity
                    && copy.m_flTime == videoFrame.m_flTime && copy.m_bKey == videoFrame.m_bKey
                    && std::memcmp(copy.m_pBuffer, videoFrame.m_pBuffer,
                        static_cast<std::size_t>(copy.m_uBufferSize)) == 0,
                    "assignment to an empty frame owns an independent packet copy");
            }
            ++videoPackets;
            Require(video.Decode(videoFrame), "decode actual demuxed VPX packet");
            AppendImage(video, pixels, images);
        }
        if (audioFrame.m_uBufferSize > 0) {
            Require(audioFrame.m_flTime >= priorAudioTime, "audio packet timestamps remain ordered");
            priorAudioTime = audioFrame.m_flTime;
            ++audioPackets;
            constexpr short guard = 23100;
            std::vector<short> samples(static_cast<std::size_t>(audio.m_nSamples * expectedChannels) + 2, guard);
            int frames = 0;
            Require(audio.GetPCMS16(audioFrame, samples.data() + 1, frames),
                "decode real Vorbis packet to PCM S16");
            Require(samples.front() == guard && samples.back() == guard
                && frames >= 0 && frames <= audio.m_nSamples,
                "decoded PCM retains scratch-buffer boundaries");
            for (int index = 0; index < frames * expectedChannels; ++index) {
                const auto bits = static_cast<std::uint16_t>(samples[static_cast<std::size_t>(index) + 1]);
                pcm.push_back(static_cast<unsigned char>(bits));
                constexpr int bitsPerByte = 8;
                pcm.push_back(static_cast<unsigned char>(bits >> bitsPerByte));
            }
        }
    }
    Require(!demuxer.ReadFrame(&videoFrame, &audioFrame), "repeated end-of-stream request is stable");
    WebMFrame flush;
    for (int cycle = 0; cycle < expectedVideoFrames; ++cycle) {
        Require(video.Decode(flush), "flush any delayed VPX images");
        AppendImage(video, pixels, images);
    }
    Require(videoPackets == expectedVideoFrames && images == expectedVideoFrames
        && audioPackets == expectedAudioPackets,
        "all actual demux packets and decoded frames retain known counts");
    Require(pixels == ReadBytes(base.string() + ".yuv"), "decoded planes exactly match independent known reference");
    ComparePCM(pcm, ReadBytes(base.parent_path() / "stereo.pcm"));
    audio.Close();
    audio.Close();
    Require(!audio.IsOpen() && audio.OpenVorbis(demuxer), "closed Vorbis decoder can restart on valid headers");
    std::printf("Video images=%d hash=%016llx threads=%u duration=%.6f\n", images,
        static_cast<unsigned long long>(Hash(pixels)), threads, demuxer.GetLength());
}

void TestInvalidContainer(const std::filesystem::path& path) {
    constexpr int cycles = 64;
    for (int cycle = 0; cycle < cycles; ++cycle) {
        mkvparser::MkvReader reader;
        Require(reader.Open(path.string().c_str()) == 0, "open owned truncated container");
        WebMDemuxer demuxer(&reader, 0, 0);
        Require(!demuxer.m_bIsOpen && demuxer.GetVideoCodec() == WebMDemuxer::NO_VIDEO
            && demuxer.GetAudioCodec() == WebMDemuxer::NO_AUDIO, "invalid EBML does not expose open tracks");
        WebMFrame video;
        WebMFrame audio;
        Require(!demuxer.ReadFrame(&video, &audio), "invalid container cannot deliver a packet");
    }
}

void TestTrackSelection(const std::filesystem::path& base) {
    for (bool videoOnly : {true, false}) {
        mkvparser::MkvReader reader;
        Require(reader.Open((base.string() + ".webm").c_str()) == 0, "open known container for selective demux");
        WebMDemuxer demuxer(&reader, 0, 0);
        Require(demuxer.m_bIsOpen, "selective demux opens actual tracks");
        WebMFrame frame;
        int packets = 0;
        const int expectedPackets = videoOnly ? expectedVideoFrames : expectedAudioPackets;
        while (demuxer.ReadFrame(videoOnly ? &frame : nullptr, videoOnly ? nullptr : &frame)) {
            ++packets;
            Require(frame.m_uBufferSize > 0 && packets <= expectedPackets,
                "selective demux delivers each requested track packet once");
        }
        Require(packets == expectedPackets, "null output excludes only its corresponding track");
    }
}

}

int main(int argc, char** argv) {
#if defined(_WIN32)
    PreparePresentationGuardProcess();
    if (argc == 3 && std::strcmp(argv[1], "--presentation-guard") == 0) {
        InvokeUnusedPresentationBinding(argv[2]);
        return EXIT_FAILURE;
    }
    Require(SDL_GetVersion() == SDL_VERSION, "loaded pinned SDL3 provider matches compile-time version");
    constexpr std::size_t providerPathCapacity = 32768;
    wchar_t providerPath[providerPathCapacity] = {};
    const auto provider = GetModuleHandleW(L"SDL3.dll");
    Require(provider && GetModuleFileNameW(provider, providerPath, static_cast<DWORD>(std::size(providerPath))) > 0,
        "real linked SDL3 provider is loaded");
    std::printf("SDL3 runtime_version=%d revision=%s loaded_path=%ls\n", SDL_GetVersion(), SDL_GetRevision(), providerPath);
#endif
    std::printf("WebMFrame=%zu Demuxer=%zu VPXDecoder=%zu Image=%zu VorbisDecoder=%zu pointer=%zu\n",
        sizeof(WebMFrame), sizeof(WebMDemuxer), sizeof(VPXDecoder), sizeof(VPXDecoder::Image),
        sizeof(AudioVorbisDecoder), sizeof(void*));
    TestSizeBoundary();
    if (argc == 2) {
        Decode(argv[1], 1);
    } else if (argc == 3) {
        if (std::strcmp(argv[2], "invalid") == 0) {
            TestInvalidContainer(argv[1]);
        } else if (std::strcmp(argv[2], "select") == 0) {
            TestTrackSelection(argv[1]);
        } else if (std::strcmp(argv[2], "lifetime") == 0) {
            constexpr int cycles = 64;
            for (int cycle = 0; cycle < cycles; ++cycle) {
                Decode(argv[1], 1);
            }
        } else {
            Require(std::strcmp(argv[2], "threads") == 0, "known fixture scenario");
            for (unsigned int threads : {0u, 8u, UINT_MAX}) {
                Decode(argv[1], threads);
            }
        }
    }
    std::puts("Native WebM fixture passed");
    return 0;
}
