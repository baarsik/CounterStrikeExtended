#include <webm/mkvmuxer/mkvmuxer.h>
#include <webm/mkvmuxer/mkvwriter.h>
#include <vpx/vp8cx.h>
#include <vpx/vp8dx.h>
#include <vpx/vpx_encoder.h>
#include <vpx/vpx_decoder.h>
#include <vorbis/vorbisenc.h>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <limits>
#include <string>
#include <vector>

namespace {

constexpr unsigned int width = 16;
constexpr unsigned int height = 16;
constexpr int videoFrames = 8;
constexpr int channels = 2;
constexpr int sampleRate = 48000;
constexpr int inputSamples = 8192;
constexpr int planeCount = 3;
constexpr int vorbisHeaderCount = 3;
constexpr unsigned int frameRate = 25;
constexpr unsigned int targetBitrate = 128;
constexpr unsigned int minimumYuv = 16;
constexpr unsigned int yuvRange = 219;
constexpr unsigned int patternXStep = 3;
constexpr unsigned int patternYStep = 5;
constexpr unsigned int patternFrameStep = 7;
constexpr unsigned int patternPlaneStep = 37;
constexpr int lacingContinuation = 255;
constexpr int bitsPerByte = 8;
constexpr std::uint64_t nanosecondsPerSecond = 1000000000;
constexpr std::uint64_t videoFrameNanoseconds = 40000000;

void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "Fixture generation failed: %s\n", message);
        std::exit(EXIT_FAILURE);
    }
}

struct Packet {
    std::vector<unsigned char> bytes;
    std::uint64_t time;
    bool key;
};

void Save(const std::filesystem::path& path, const std::vector<unsigned char>& bytes) {
    std::ofstream output(path, std::ios::binary);
    output.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    Require(output.good(), "write owned reference bytes");
}

std::vector<Packet> EncodeVideo(bool vp9) {
    vpx_codec_ctx_t encoder = {};
    vpx_codec_enc_cfg_t config = {};
    auto* iface = vp9 ? vpx_codec_vp9_cx() : vpx_codec_vp8_cx();
    Require(vpx_codec_enc_config_default(iface, &config, 0) == VPX_CODEC_OK, "configure pinned VPX encoder");
    config.g_w = width;
    config.g_h = height;
    config.g_timebase.num = 1;
    config.g_timebase.den = frameRate;
    config.g_threads = 1;
    config.g_lag_in_frames = 0;
    config.rc_min_quantizer = 0;
    config.rc_max_quantizer = 0;
    config.rc_target_bitrate = targetBitrate;
    Require(vpx_codec_enc_init(&encoder, iface, &config, 0) == VPX_CODEC_OK, "open pinned VPX encoder");
    if (vp9) {
        Require(vpx_codec_control(&encoder, VP9E_SET_LOSSLESS, 1) == VPX_CODEC_OK, "configure lossless VP9 fixture");
    }
    vpx_image_t image = {};
    Require(vpx_img_alloc(&image, VPX_IMG_FMT_I420, width, height, 1) != nullptr, "allocate fixture YUV image");
    std::vector<Packet> packets;
    for (int frame = 0; frame < videoFrames; ++frame) {
        for (int plane = 0; plane < planeCount; ++plane) {
            const unsigned int planeWidth = plane == 0 ? width : width / 2;
            const unsigned int planeHeight = plane == 0 ? height : height / 2;
            for (unsigned int y = 0; y < planeHeight; ++y) {
                for (unsigned int x = 0; x < planeWidth; ++x) {
                    image.planes[plane][y * static_cast<unsigned int>(image.stride[plane]) + x] =
                        static_cast<unsigned char>(minimumYuv + (x * patternXStep + y * patternYStep
                            + static_cast<unsigned int>(frame) * patternFrameStep
                            + static_cast<unsigned int>(plane) * patternPlaneStep) % yuvRange);
                }
            }
        }
        Require(vpx_codec_encode(&encoder, &image, frame, 1, 0, VPX_DL_GOOD_QUALITY) == VPX_CODEC_OK,
            "encode owned video pattern");
        vpx_codec_iter_t iterator = nullptr;
        while (const auto* packet = vpx_codec_get_cx_data(&encoder, &iterator)) {
            if (packet->kind == VPX_CODEC_CX_FRAME_PKT) {
                const auto* begin = static_cast<const unsigned char*>(packet->data.frame.buf);
                packets.push_back({std::vector<unsigned char>(begin, begin + packet->data.frame.sz),
                    static_cast<std::uint64_t>(frame) * videoFrameNanoseconds,
                    (packet->data.frame.flags & VPX_FRAME_IS_KEY) != 0});
            }
        }
    }
    vpx_img_free(&image);
    vpx_codec_destroy(&encoder);
    Require(packets.size() == videoFrames, "all requested video packets encoded");
    return packets;
}

std::vector<unsigned char> DecodeVideo(const std::vector<Packet>& packets, bool vp9) {
    vpx_codec_ctx_t decoder = {};
    vpx_codec_dec_cfg_t config = {};
    config.threads = 1;
    Require(vpx_codec_dec_init(&decoder, vp9 ? vpx_codec_vp9_dx() : vpx_codec_vp8_dx(), &config, 0)
        == VPX_CODEC_OK, "open independent reference decoder");
    std::vector<unsigned char> reference;
    for (const auto& packet : packets) {
        Require(packet.bytes.size() <= std::numeric_limits<unsigned int>::max(),
            "owned VPX packet size fits the native decoder API");
        Require(vpx_codec_decode(&decoder, packet.bytes.data(), static_cast<unsigned int>(packet.bytes.size()),
            nullptr, 0) == VPX_CODEC_OK, "decode independent reference frame");
        vpx_codec_iter_t iterator = nullptr;
        while (const auto* image = vpx_codec_get_frame(&decoder, &iterator)) {
            for (int plane = 0; plane < planeCount; ++plane) {
                const unsigned int planeWidth = plane == 0 ? image->d_w : (image->d_w + 1) / 2;
                const unsigned int planeHeight = plane == 0 ? image->d_h : (image->d_h + 1) / 2;
                for (unsigned int y = 0; y < planeHeight; ++y) {
                    const auto* row = image->planes[plane] + y * static_cast<unsigned int>(image->stride[plane]);
                    reference.insert(reference.end(), row, row + planeWidth);
                }
            }
        }
    }
    vpx_codec_destroy(&decoder);
    return reference;
}

struct Audio {
    std::vector<unsigned char> privateData;
    std::vector<Packet> packets;
    std::vector<unsigned char> pcm;
};

Audio EncodeAudio() {
    Audio output;
    vorbis_info info;
    vorbis_info_init(&info);
    Require(vorbis_encode_init_vbr(&info, channels, sampleRate, 0.3f) == 0, "configure pinned Vorbis encoder");
    vorbis_comment comment;
    vorbis_comment_init(&comment);
    vorbis_comment_add_tag(&comment, "TITLE", "CSE owned codec fixture");
    vorbis_dsp_state encoder;
    Require(vorbis_analysis_init(&encoder, &info) == 0, "open Vorbis analysis");
    vorbis_block block;
    Require(vorbis_block_init(&encoder, &block) == 0, "open Vorbis analysis block");
    ogg_packet headers[vorbisHeaderCount] = {};
    Require(vorbis_analysis_headerout(&encoder, &comment, &headers[0], &headers[1], &headers[2]) == 0,
        "produce real Vorbis headers");
    output.privateData.push_back(2);
    for (int header = 0; header < 2; ++header) {
        long remaining = headers[header].bytes;
        while (remaining >= lacingContinuation) {
            output.privateData.push_back(lacingContinuation);
            remaining -= lacingContinuation;
        }
        output.privateData.push_back(static_cast<unsigned char>(remaining));
    }
    for (const auto& header : headers) {
        output.privateData.insert(output.privateData.end(), header.packet, header.packet + header.bytes);
    }
    vorbis_dsp_state decoder;
    vorbis_block decodeBlock;
    vorbis_info decoderInfo;
    vorbis_info_init(&decoderInfo);
    vorbis_comment decoderComment;
    vorbis_comment_init(&decoderComment);
    for (auto& header : headers) {
        Require(vorbis_synthesis_headerin(&decoderInfo, &decoderComment, &header) == 0,
            "parse independent reference decoder headers");
    }
    Require(vorbis_synthesis_init(&decoder, &decoderInfo) == 0 && vorbis_block_init(&decoder, &decodeBlock) == 0,
        "open independent Vorbis reference decoder");
    float** samples = vorbis_analysis_buffer(&encoder, inputSamples);
    constexpr int leftPeriod = 64;
    constexpr int leftCenter = 32;
    constexpr float leftScale = 128;
    constexpr int rightPeriod = 97;
    constexpr int rightCenter = 48;
    constexpr float rightScale = 256;
    for (int sample = 0; sample < inputSamples; ++sample) {
        samples[0][sample] = static_cast<float>(sample % leftPeriod - leftCenter) / leftScale;
        samples[1][sample] = static_cast<float>(sample % rightPeriod - rightCenter) / rightScale;
    }
    Require(vorbis_analysis_wrote(&encoder, inputSamples) == 0 && vorbis_analysis_wrote(&encoder, 0) == 0,
        "submit owned asymmetric stereo samples");
    std::uint64_t decodedSamples = 0;
    while (vorbis_analysis_blockout(&encoder, &block) == 1) {
        Require(vorbis_analysis(&block, nullptr) == 0 && vorbis_bitrate_addblock(&block) == 0,
            "encode owned Vorbis block");
        ogg_packet packet = {};
        while (vorbis_bitrate_flushpacket(&encoder, &packet)) {
            output.packets.push_back({std::vector<unsigned char>(packet.packet, packet.packet + packet.bytes),
                decodedSamples * nanosecondsPerSecond / sampleRate, true});
            ogg_packet decodedPacket = {};
            decodedPacket.packet = packet.packet;
            decodedPacket.bytes = packet.bytes;
            Require(vorbis_synthesis(&decodeBlock, &decodedPacket) == 0
                && vorbis_synthesis_blockin(&decoder, &decodeBlock) == 0, "decode independent reference packet");
            float** pcm = nullptr;
            int available = 0;
            while ((available = vorbis_synthesis_pcmout(&decoder, &pcm)) > 0) {
                for (int sample = 0; sample < available; ++sample) {
                    for (int channel = 0; channel < channels; ++channel) {
                        constexpr int sampleMinimum = std::numeric_limits<std::int16_t>::min();
                        constexpr int sampleMaximum = std::numeric_limits<std::int16_t>::max();
                        const int value = std::clamp(static_cast<int>(sampleMaximum * pcm[channel][sample]),
                            sampleMinimum, sampleMaximum);
                        const auto bits = static_cast<std::uint16_t>(static_cast<std::int16_t>(value));
                        output.pcm.push_back(static_cast<unsigned char>(bits));
                        output.pcm.push_back(static_cast<unsigned char>(bits >> bitsPerByte));
                    }
                }
                decodedSamples += static_cast<std::uint64_t>(available);
                vorbis_synthesis_read(&decoder, available);
            }
        }
    }
    vorbis_block_clear(&decodeBlock);
    vorbis_dsp_clear(&decoder);
    vorbis_comment_clear(&decoderComment);
    vorbis_info_clear(&decoderInfo);
    vorbis_block_clear(&block);
    vorbis_dsp_clear(&encoder);
    vorbis_comment_clear(&comment);
    vorbis_info_clear(&info);
    return output;
}

void Mux(const std::filesystem::path& path, const std::vector<Packet>& video, const Audio& audio, bool vp9) {
    mkvmuxer::MkvWriter writer;
    Require(writer.Open(path.string().c_str()), "create owned WebM container");
    mkvmuxer::Segment segment;
    Require(segment.Init(&writer), "initialize real libwebm muxer");
    segment.set_mode(mkvmuxer::Segment::kFile);
    segment.GetSegmentInfo()->set_muxing_app("CSE native codec fixture");
    segment.GetSegmentInfo()->set_writing_app("CSE native codec fixture");
    const auto videoTrack = segment.AddVideoTrack(width, height, 1);
    const auto audioTrack = segment.AddAudioTrack(sampleRate, channels, 2);
    Require(videoTrack && audioTrack, "add actual VPX/Vorbis tracks");
    auto* videoSettings = segment.GetTrackByNumber(videoTrack);
    videoSettings->set_codec_id(vp9 ? "V_VP9" : "V_VP8");
    videoSettings->set_uid(1);
    auto* audioSettings = segment.GetTrackByNumber(audioTrack);
    audioSettings->set_codec_id("A_VORBIS");
    audioSettings->set_uid(2);
    Require(audioSettings->SetCodecPrivate(audio.privateData.data(), audio.privateData.size()), "write Xiph headers");
    std::size_t videoIndex = 0;
    std::size_t audioIndex = 0;
    while (videoIndex < video.size() || audioIndex < audio.packets.size()) {
        const bool isVideo = videoIndex < video.size()
            && (audioIndex == audio.packets.size() || video[videoIndex].time <= audio.packets[audioIndex].time);
        const auto& packet = isVideo ? video[videoIndex++] : audio.packets[audioIndex++];
        Require(segment.AddFrame(packet.bytes.data(), packet.bytes.size(), isVideo ? videoTrack : audioTrack,
            packet.time, packet.key), "mux ordered owned media packet");
    }
    Require(segment.Finalize(), "finalize owned WebM container");
    writer.Close();
}

}

int main(int argc, char** argv) {
    Require(argc == 2, "owned fixture output directory is required");
    const std::filesystem::path root = argv[1];
    std::filesystem::create_directories(root);
    const Audio audio = EncodeAudio();
    Save(root / "stereo.pcm", audio.pcm);
    for (bool vp9 : {false, true}) {
        const std::string name = vp9 ? "vp9" : "vp8";
        const auto packets = EncodeVideo(vp9);
        Save(root / (name + ".yuv"), DecodeVideo(packets, vp9));
        Mux(root / (name + ".webm"), packets, audio, vp9);
    }
    std::printf("Generated owned VP8/VP9 + Vorbis: video_frames=%d audio_packets=%zu PCM_bytes=%zu\n",
        videoFrames, audio.packets.size(), audio.pcm.size());
    return EXIT_SUCCESS;
}
