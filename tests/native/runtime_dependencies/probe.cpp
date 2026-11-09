#include "runtime_sdk_marker.h"

#include <cstdio>
#include <cstring>

static_assert(sizeof(void*) == 8, "Native dependency routing requires a native 64-bit compiler target");
static_assert(RuntimeSdkMarker == 73, "Other SDK includes must retain their original path");

#if defined(CSE_PROBE_FREETYPE)
#include <ft2build.h>
#include FT_FREETYPE_H
static bool Probe() {
    FT_Library library = nullptr;
    if (FT_Init_FreeType(&library) != 0) {
        return false;
    }
    FT_Int major = 0;
    FT_Int minor = 0;
    FT_Int patch = 0;
    FT_Library_Version(library, &major, &minor, &patch);
    FT_Done_FreeType(library);
    return major == FREETYPE_MAJOR && minor == FREETYPE_MINOR && patch == FREETYPE_PATCH;
}
#elif defined(CSE_PROBE_BZIP2)
#include <bzlib.h>
static bool Probe() {
    return std::strncmp(BZ2_bzlibVersion(), "1.", 2) == 0;
}
#elif defined(CSE_PROBE_EXPAT)
#include <expat.h>
static bool Probe() {
    XML_Parser parser = XML_ParserCreate(nullptr);
    if (!parser) {
        return false;
    }
    constexpr char Xml[] = "<native><dependency/></native>";
    const bool parsed = XML_Parse(parser, Xml, static_cast<int>(sizeof(Xml) - 1), XML_TRUE) == XML_STATUS_OK;
    XML_ParserFree(parser);
    return parsed;
}
#elif defined(CSE_PROBE_PNG)
#include <png.h>
static bool Probe() {
    return png_access_version_number() == PNG_LIBPNG_VER;
}
#elif defined(CSE_PROBE_ZLIB)
#include <zlib.h>
static bool Probe() {
    return std::strcmp(zlibVersion(), ZLIB_VERSION) == 0;
}
#elif defined(CSE_PROBE_BROTLICOMMON)
#include <brotli/shared_dictionary.h>
static bool Probe() {
    BrotliSharedDictionary* dictionary = BrotliSharedDictionaryCreateInstance(nullptr, nullptr, nullptr);
    if (!dictionary) {
        return false;
    }
    BrotliSharedDictionaryDestroyInstance(dictionary);
    return true;
}
#elif defined(CSE_PROBE_BROTLIDEC)
#include <brotli/decode.h>
static bool Probe() {
    BrotliDecoderState* decoder = BrotliDecoderCreateInstance(nullptr, nullptr, nullptr);
    if (!decoder) {
        return false;
    }
    BrotliDecoderDestroyInstance(decoder);
    return BrotliDecoderVersion() != 0;
}
#elif defined(CSE_PROBE_OGG)
#include <ogg/ogg.h>
static bool Probe() {
    oggpack_buffer writer = {};
    oggpack_writeinit(&writer);
    constexpr unsigned long Value = 0x123456;
    constexpr int Bits = 24;
    oggpack_write(&writer, Value, Bits);
    oggpack_buffer reader = {};
    oggpack_readinit(&reader, oggpack_get_buffer(&writer), static_cast<int>(oggpack_bytes(&writer)));
    const bool equal = oggpack_read(&reader, Bits) == static_cast<long>(Value);
    oggpack_writeclear(&writer);
    return equal;
}
#elif defined(CSE_PROBE_VORBIS)
#include <vorbis/codec.h>
static bool Probe() {
    vorbis_info info = {};
    vorbis_info_init(&info);
    const bool initialized = info.version == 0 && std::strstr(vorbis_version_string(), "Vorbis") != nullptr;
    vorbis_info_clear(&info);
    return initialized;
}
#elif defined(CSE_PROBE_VORBISFILE)
#include <vorbis/vorbisfile.h>
#include <algorithm>
struct MemoryInput {
    const char* data;
    std::size_t size;
    std::size_t position;
};
static std::size_t Read(void* output, std::size_t size, std::size_t count, void* context) {
    MemoryInput* input = static_cast<MemoryInput*>(context);
    if (size == 0) {
        return 0;
    }
    const std::size_t items = std::min(count, (input->size - input->position) / size);
    std::memcpy(output, input->data + input->position, items * size);
    input->position += items * size;
    return items;
}
static bool Probe() {
    constexpr char InvalidStream[] = "not an Ogg Vorbis bitstream";
    MemoryInput input = {InvalidStream, sizeof(InvalidStream), 0};
    OggVorbis_File file = {};
    const ov_callbacks callbacks = {Read, nullptr, nullptr, nullptr};
    const int result = ov_open_callbacks(&input, &file, nullptr, 0, callbacks);
    if (result == 0) {
        ov_clear(&file);
    }
    return result == OV_ENOTVORBIS;
}
#elif defined(CSE_PROBE_VPX)
#include <vpx/vp8dx.h>
#include <vpx/vpx_decoder.h>
static bool Probe() {
    vpx_codec_ctx_t decoder = {};
    vpx_codec_dec_cfg_t configuration = {};
    configuration.threads = 1;
    if (vpx_codec_dec_init(&decoder, vpx_codec_vp8_dx(), &configuration, 0) != VPX_CODEC_OK) {
        return false;
    }
    return vpx_codec_destroy(&decoder) == VPX_CODEC_OK;
}
#elif defined(CSE_PROBE_WEBM)
#include <mkvparser/mkvparser.h>
static bool Probe() {
    int major = 0;
    int minor = 0;
    int build = 0;
    int revision = 0;
    mkvparser::GetVersion(major, minor, build, revision);
    return major >= 1 && minor >= 0 && build >= 0 && revision >= 0;
}
#else
#error Select one native runtime dependency probe.
#endif

int main() {
    if (!Probe()) {
        std::fputs("Native runtime package symbol probe failed\n", stderr);
        return 1;
    }
    std::printf("Native runtime package symbols passed; pointer_bytes=%zu\n", sizeof(void*));
    return 0;
}
