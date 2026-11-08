#include <brotli/decode.h>
#include <brotli/encode.h>
#include <bzlib.h>
#include <expat.h>
#include <ft2build.h>
#include FT_FREETYPE_H
#include <ogg/ogg.h>
#include <png.h>
#include <vorbis/vorbisenc.h>
#include <vpx/vp8dx.h>
#include <vpx/vpx_decoder.h>
#include <webm/mkvparser/mkvparser.h>
#include <zlib.h>

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstring>
#include <vector>

static bool CheckCompression() {
    constexpr char input[] = "CounterStrikeExtended native compression round trip: 0123456789";
    constexpr auto inputBytes = sizeof(input);
    const auto *source = reinterpret_cast<const unsigned char *>(input);
    std::array<unsigned char, inputBytes> decoded{};
    uLongf encodedBytes = compressBound(inputBytes);
    uLongf decodedBytes = inputBytes;
    std::vector<unsigned char> encoded(encodedBytes);
    if (compress2(encoded.data(), &encodedBytes, source, inputBytes, Z_BEST_COMPRESSION) != Z_OK
        || uncompress(decoded.data(), &decodedBytes, encoded.data(), encodedBytes) != Z_OK
        || decodedBytes != inputBytes || std::memcmp(decoded.data(), input, inputBytes) != 0) {
        return false;
    }

    // bzip2 documents a worst-case bound of source + 1% + 600 bytes.
    constexpr unsigned int bzipOverhead = 600;
    encoded.resize(inputBytes + inputBytes / 100 + bzipOverhead);
    unsigned int bzipBytes = static_cast<unsigned int>(encoded.size());
    unsigned int bzipDecoded = static_cast<unsigned int>(decoded.size());
    if (BZ2_bzBuffToBuffCompress(reinterpret_cast<char *>(encoded.data()), &bzipBytes,
                               const_cast<char *>(input), inputBytes, 9, 0, 30) != BZ_OK
        || BZ2_bzBuffToBuffDecompress(reinterpret_cast<char *>(decoded.data()), &bzipDecoded,
                                    reinterpret_cast<char *>(encoded.data()), bzipBytes, 0, 0) != BZ_OK
        || bzipDecoded != inputBytes || std::memcmp(decoded.data(), input, inputBytes) != 0) {
        return false;
    }

    encoded.resize(BrotliEncoderMaxCompressedSize(inputBytes));
    size_t brotliBytes = encoded.size();
    size_t brotliDecoded = decoded.size();
    return BrotliEncoderCompress(BROTLI_DEFAULT_QUALITY, BROTLI_DEFAULT_WINDOW, BROTLI_MODE_GENERIC,
                                 inputBytes, source, &brotliBytes, encoded.data())
        && BrotliDecoderDecompress(brotliBytes, encoded.data(), &brotliDecoded, decoded.data())
            == BROTLI_DECODER_RESULT_SUCCESS
        && brotliDecoded == inputBytes && std::memcmp(decoded.data(), input, inputBytes) == 0;
}

static void XMLCALL CountElement(void *context, const XML_Char *, const XML_Char **) {
    ++*static_cast<int *>(context);
}

static bool CheckXml() {
    XML_Parser parser = XML_ParserCreate("UTF-8");
    if (!parser) {
        return false;
    }
    int elements = 0;
    XML_SetUserData(parser, &elements);
    XML_SetStartElementHandler(parser, CountElement);
    constexpr char input[] = "<fixture><value>native &amp; compatible</value></fixture>";
    const bool success = XML_Parse(parser, input, sizeof(input) - 1, XML_TRUE) == XML_STATUS_OK && elements == 2;
    XML_ParserFree(parser);
    return success;
}

static bool CheckPng() {
    constexpr std::array<unsigned char, 4> pixel{17, 89, 203, 255};
    png_image image{};
    image.version = PNG_IMAGE_VERSION;
    image.width = 1;
    image.height = 1;
    image.format = PNG_FORMAT_RGBA;
    png_alloc_size_t bytes = 0;
    if (!png_image_write_to_memory(&image, nullptr, &bytes, 0, pixel.data(), 0, nullptr)) {
        return false;
    }
    std::vector<unsigned char> encoded(bytes);
    if (!png_image_write_to_memory(&image, encoded.data(), &bytes, 0, pixel.data(), 0, nullptr)) {
        return false;
    }
    png_image decoded{};
    decoded.version = PNG_IMAGE_VERSION;
    if (!png_image_begin_read_from_memory(&decoded, encoded.data(), bytes)) {
        return false;
    }
    decoded.format = PNG_FORMAT_RGBA;
    std::array<unsigned char, pixel.size()> result{};
    const bool success = decoded.width == 1 && decoded.height == 1
        && png_image_finish_read(&decoded, nullptr, result.data(), 0, nullptr) && result == pixel;
    png_image_free(&decoded);
    return success;
}

static bool CheckFonts() {
    FT_Library library = nullptr;
    if (FT_Init_FreeType(&library)) {
        return false;
    }
    FT_Int major = 0;
    FT_Int minor = 0;
    FT_Int patch = 0;
    FT_Library_Version(library, &major, &minor, &patch);
    FT_Done_FreeType(library);
    return major == FREETYPE_MAJOR && minor == FREETYPE_MINOR && patch == FREETYPE_PATCH;
}

static bool CheckMedia() {
    oggpack_buffer packed{};
    oggpack_writeinit(&packed);
    constexpr unsigned long packetValue = 0x5a7fff;
    constexpr int packetBits = 24;
    oggpack_write(&packed, packetValue, packetBits);
    oggpack_buffer reader{};
    oggpack_readinit(&reader, oggpack_get_buffer(&packed), static_cast<int>(oggpack_bytes(&packed)));
    const bool oggSuccess = oggpack_read(&reader, packetBits) == packetValue;
    oggpack_writeclear(&packed);
    if (!oggSuccess) {
        return false;
    }

    vorbis_info info{};
    vorbis_info_init(&info);
    constexpr long sampleRate = 44100;
    const bool vorbisSuccess = vorbis_encode_init_vbr(&info, 2, sampleRate, 0.4f) == 0;
    vorbis_info_clear(&info);
    if (!vorbisSuccess) {
        return false;
    }
    for (auto *interface : {vpx_codec_vp8_dx(), vpx_codec_vp9_dx()}) {
        vpx_codec_ctx_t decoder{};
        vpx_codec_dec_cfg_t configuration{};
        configuration.threads = 1;
        if (vpx_codec_dec_init(&decoder, interface, &configuration, 0) != VPX_CODEC_OK) {
            return false;
        }
        if (vpx_codec_destroy(&decoder) != VPX_CODEC_OK) {
            return false;
        }
    }
    int major = 0;
    int minor = 0;
    int build = 0;
    int revision = 0;
    mkvparser::GetVersion(major, minor, build, revision);
    return major >= 1;
}

int main() {
    if (!CheckCompression() || !CheckXml() || !CheckPng() || !CheckFonts() || !CheckMedia()) {
        std::fputs("Native compression/font/media fixture failed.\n", stderr);
        return 1;
    }
    std::printf("Native compression, XML, PNG, FreeType, Ogg/Vorbis, VP8/VP9 and WebM passed; pointer_bytes=%zu\n",
                sizeof(void *));
    return 0;
}
