#include "gl_model.h"
#if !defined(SW_RENDERER)
#include "glquake.h"
#endif
#include "common.h"
#include "zone.h"
#include "engine_studio.h"
#include "sys.h"
#include <cassert>
#include <climits>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <memory>
#include <cstddef>
#include <stdexcept>
#include <string>
#include <vector>

constexpr int PaletteEntries = 256;
constexpr int RgbPaletteBytes = 768;
constexpr int PaletteChannels = 3;
constexpr std::size_t SpriteFrameTypeOffset = 810;
constexpr std::size_t SpriteFrameOffset = 814;
constexpr std::size_t SpriteGroupCountOffset = 814;
constexpr std::size_t SpriteFirstIntervalOffset = 818;
#if !defined(SW_RENDERER)
constexpr int FixtureTextureNumber = 17;
#endif
constexpr byte FixturePixel = 7;

static int allocations;
static int uploads;
static std::vector<std::unique_ptr<byte[]>> memory;
static std::vector<byte> uploadedPixels;
static std::vector<byte> uploadedPalette;
byte texgammatable[PaletteEntries];
model_t* loadmodel;
char loadname[32] = "fixture";
#if defined(SW_RENDERER)
int r_pixbytes = 1;
#else
byte* pspritepal;
qboolean gSpriteMipMap;
static int gSpriteTextureFormat;
#endif
static int Identity(int value) {
    return value;
}
static float IdentityFloat(float value) {
    return value;
}
int (*LittleLong)(int) = Identity;
float (*LittleFloat)(float) = IdentityFloat;
void Q_memset(void* out, int value, int bytes) {
    std::memset(out, value, bytes);
}
void Q_memcpy(void* out, const void* in, int bytes) {
    std::memcpy(out, in, bytes);
}
void Q_strcpy(char* out, const char* in) {
    std::strcpy(out, in);
}
void Sys_Error(const char*, ...) {
    throw std::runtime_error("rejected");
}
void* Hunk_AllocName(int bytes, const char*) {
    assert(bytes > 0);
    ++allocations;
    memory.emplace_back(new byte[bytes]{});
    return memory.back().get();
}
void* Cache_Alloc(cache_user_t* cache, int bytes, const char* name) {
    cache->data = Hunk_AllocName(bytes, name);
    return cache->data;
}
#if !defined(SW_RENDERER)
int GL_LoadTexture(char*, GL_TEXTURETYPE, int width, int height, byte* pixels, int, int, byte* palette) {
    ++uploads;
    uploadedPixels.assign(pixels, pixels + width * height);
    uploadedPalette.assign(palette, palette + RgbPaletteBytes);
    return FixtureTextureNumber;
}
#endif
#include "studio_loader.inc"
#if __has_include("../../../src/engine/model_loading.h")
#include "../../../src/engine/model_loading.h"
#endif
#include "sprite_loader.inc"

static void Load(bool studio, model_t& model, std::vector<byte>& bytes) {
    loadmodel = &model;
#ifdef MODEL_LOADING_BASELINE
    if (studio) {
        Mod_LoadStudioModel(&model, bytes.data());
    }
    else {
        Mod_LoadSpriteModel(&model, bytes.data());
    }
#else
    if (studio) {
        Mod_LoadStudioModel(&model, bytes.data(), static_cast<int>(bytes.size()));
    }
    else {
        Mod_LoadSpriteModel(&model, bytes.data(), static_cast<int>(bytes.size()));
    }
#endif
}
template<class T> static void Append(std::vector<byte>& bytes, const T& value) {
    const auto* data = reinterpret_cast<const byte*>(&value);
    bytes.insert(bytes.end(), data, data + sizeof(value));
}
template<class T> static T Read(const std::vector<byte>& bytes, std::size_t offset) {
    T value;
    std::memcpy(&value, bytes.data() + offset, sizeof(value));
    return value;
}
template<class T> static void Put(std::vector<byte>& bytes, std::size_t offset, const T& value) {
    std::memcpy(bytes.data() + offset, &value, sizeof(value));
}
static std::vector<byte> Studio(bool texture) {
    studiohdr_t header {
    };
    header.id = IDSTUDIOHEADER;
    header.version = STUDIO_VERSION;
    header.length = sizeof(header);
    if (texture) {
        header.numtextures = 1;
        header.textureindex = sizeof(header);
        header.texturedataindex = sizeof(header) + sizeof(mstudiotexture_t);
        header.length = header.texturedataindex + 1 + RgbPaletteBytes;
    }
    std::vector<byte> bytes;
    Append(bytes, header);
    if (texture) {
        mstudiotexture_t entry {
        };
        std::strcpy(entry.name, "test");
        entry.width = 1;
        entry.height = 1;
        entry.index = header.texturedataindex;
        Append(bytes, entry);
        bytes.push_back(FixturePixel);
        for (int i = 0; i < RgbPaletteBytes; ++i) {
            bytes.push_back(static_cast<byte>(i));
        }
    }
    return bytes;
}
static std::vector<byte> Sprite(bool group, unsigned short entries = PaletteEntries) {
    dsprite_t header {
    };
    header.ident = IDSPRITEHEADER;
    header.version = SPRITE_VERSION;
    header.width = 1;
    header.height = 1;
    header.numframes = 1;
    std::vector<byte> bytes;
    Append(bytes, header);
    Append(bytes, entries);
    for (int i = 0; i < entries * PaletteChannels; ++i) {
        bytes.push_back(static_cast<byte>(i));
    }
    Append(bytes, group ? SPR_GROUP : SPR_SINGLE);
    if (group) {
        Append(bytes, 2);
        Append(bytes, 0.1f);
        Append(bytes, 0.2f);
    }
    dspriteframe_t frame {
    };
    frame.width = 1;
    frame.height = 1;
    for (int i = 0; i < (group ? 2 : 1); ++i) {
        Append(bytes, frame);
        bytes.push_back(FixturePixel);
    }
    return bytes;
}
static void Reset() {
    allocations = 0;
    uploads = 0;
    memory.clear();
    uploadedPixels.clear();
    uploadedPalette.clear();
}
static void Reject(bool studio, std::vector<byte> bytes) {
    Reset();
    model_t model {
    };
    bool rejected = false;
    try {
        Load(studio, model, bytes);
    }
    catch (const std::runtime_error&) {
        rejected = true;
    }
    if (!rejected) {
        std::fprintf(stderr, "NOT REJECTED studio=%d size=%zu allocations=%d\n", studio, bytes.size(), allocations);
    }
    assert(rejected);
    assert(allocations == 0 && uploads == 0);
}
static void Valid(bool studio, std::vector<byte> bytes) {
    const bool fallback = studio && Read<int>(bytes, offsetof(studiohdr_t, version)) != STUDIO_VERSION;
    Reset();
    model_t model {
    };
    Load(studio, model, bytes);
    assert(model.cache.data);
    if (studio) {
        const auto* header = static_cast<studiohdr_t*>(model.cache.data);
        assert(model.type == mod_studio);
        if (fallback) {
            assert(header->length == sizeof(studiohdr_t) && header->numtextures == 0);
            assert(std::strcmp(header->name, "bogus") == 0 && model.flags == 0);
        }
        if (header->numtextures) {
            mstudiotexture_t texture;
            std::memcpy(&texture, static_cast<byte*>(model.cache.data) + header->textureindex, sizeof(texture));
#if defined(SW_RENDERER)
            const byte* output = static_cast<byte*>(model.cache.data) + texture.index;
            assert(output[0] == FixturePixel);
            word palette[4];
            std::memcpy(palette, output + 1, sizeof(palette));
            assert(palette[0] == 255 && palette[1] == 254 && palette[2] == 253 && palette[3] == 0);
#else
            assert(texture.index == FixtureTextureNumber && uploads == 1);
            assert(uploadedPixels == std::vector<byte>{FixturePixel});
            assert(uploadedPalette[0] == 0 && uploadedPalette[RgbPaletteBytes - 1] == 255);
#endif
        }
    } else {
        assert(model.type == mod_sprite && model.numframes == 1);
        const auto* sprite = static_cast<msprite_t*>(model.cache.data);
        const mspriteframe_t* frame = sprite->frames[0].frameptr;
        if (sprite->frames[0].type == SPR_GROUP) {
            const auto* group = reinterpret_cast<const mspritegroup_t*>(frame);
            assert(group->numframes == 2 && group->intervals[1] == 0.2f);
            frame = group->frames[1];
        }
        assert(frame->width == 1 && frame->height == 1);
#if defined(SW_RENDERER)
        assert(frame->pixels[0] == FixturePixel);
        const byte* palette = static_cast<byte*>(model.cache.data) + sprite->paloffset;
        PackedColorVec color;
        std::memcpy(&color, palette, sizeof(color));
        assert(color.r == 0 && color.g == 1 && color.b == 2 && color.a == 0);
#else
        assert(uploadedPixels == std::vector<byte>{FixturePixel});
        assert(uploadedPalette[0] == 0 && uploadedPalette[RgbPaletteBytes - 1] == 255);
#endif
    }
}
int main(int argc, char** argv) {
    for (int i = 0; i < PaletteEntries; ++i) {
        texgammatable[i] = static_cast<byte>(PaletteEntries - 1 - i);
    }
    std::string selected = argc > 1 ? argv[1] : "all";
    if (selected == "studio-short") {
        Reject(true, std::vector<byte>(1));
        return 0;
    }
    if (selected == "sprite-valid") {
        Valid(false, Sprite(false));
        return 0;
    }
    Valid(true, Studio(false));
    Valid(true, Studio(true));
    auto fallback = Studio(false);
    Put(fallback, offsetof(studiohdr_t, version), -1);
    Valid(true, fallback);
    Valid(false, Sprite(false));
    Valid(false, Sprite(true));
    Valid(false, Sprite(false, PaletteEntries + 1));
    Valid(false, Sprite(true, SHRT_MAX));
#if defined(SW_RENDERER)
    constexpr unsigned short SmallSoftwarePaletteEntries = 8;
    Valid(false, Sprite(false, SmallSoftwarePaletteEntries));
#else
    Reject(false, Sprite(false, PaletteEntries - 1));
#endif
    for (bool studio : {false, true}) {
        auto valid = studio ? Studio(true) : Sprite(true);
        for (std::size_t size = 0; size < valid.size(); ++size) {
            Reject(studio, std::vector<byte>(valid.begin(), valid.begin() + size));
        }
    }
    for (int value : {-1, 0, INT_MAX}) {
        auto bytes = Sprite(false);
        Put(bytes, SpriteFrameOffset + offsetof(dspriteframe_t, width), value);
        Reject(false, bytes);
        bytes = Sprite(false);
        Put(bytes, offsetof(dsprite_t, numframes), value);
        Reject(false, bytes);
        bytes = Sprite(true);
        Put(bytes, SpriteGroupCountOffset, value);
        Reject(false, bytes);
        bytes = Studio(true);
        Put(bytes, sizeof(studiohdr_t) + offsetof(mstudiotexture_t, width), value);
        Reject(true, bytes);
    }
    for (float value : {-1.0f, 0.0f, std::numeric_limits<float>::infinity(),
                        std::numeric_limits<float>::quiet_NaN()}) {
        auto bytes = Sprite(true);
        Put(bytes, SpriteFirstIntervalOffset, value);
        Reject(false, bytes);
    }
    for (std::size_t offset : {offsetof(studiohdr_t, textureindex), offsetof(studiohdr_t, texturedataindex),
                               offsetof(studiohdr_t, length), offsetof(studiohdr_t, numtextures)}) {
        for (int value : {-1, INT_MAX}) {
            auto bytes = Studio(true);
            Put(bytes, offset, value);
            Reject(true, bytes);
        }
    }
    for (int value : {-1, INT_MAX}) {
        auto bytes = Studio(true);
        Put(bytes, sizeof(studiohdr_t) + offsetof(mstudiotexture_t, index), value);
        Reject(true, bytes);
    }
    for (std::size_t offset : {offsetof(dsprite_t, width), offsetof(dsprite_t, height)}) {
        for (int value : {-1, 0}) {
            auto bytes = Sprite(false);
            Put(bytes, offset, value);
            Reject(false, bytes);
        }
    }
    for (int value : {-1, 0, INT_MAX}) {
        auto bytes = Studio(true);
        Put(bytes, sizeof(studiohdr_t) + offsetof(mstudiotexture_t, height), value);
        Reject(true, bytes);
        bytes = Sprite(false);
        Put(bytes, SpriteFrameOffset + offsetof(dspriteframe_t, height), value);
        Reject(false, bytes);
    }
    for (unsigned short value : {static_cast<unsigned short>(0), static_cast<unsigned short>(32768),
                                 static_cast<unsigned short>(65535)}) {
        auto bytes = Sprite(false);
        Put(bytes, sizeof(dsprite_t), value);
        Reject(false, bytes);
    }
    auto invalidSync = Sprite(false);
    Put(invalidSync, offsetof(dsprite_t, synctype), INT_MAX);
    Reject(false, invalidSync);
    auto invalidType = Sprite(false);
    Put(invalidType, SpriteFrameTypeOffset, 2);
    Reject(false, invalidType);
    auto invalidVersion = Sprite(false);
    Put(invalidVersion, offsetof(dsprite_t, version), -1);
    Reject(false, invalidVersion);
    auto unterminatedName = Studio(true);
    std::memset(unterminatedName.data() + sizeof(studiohdr_t), 'x', sizeof(mstudiotexture_t::name));
    Valid(true, unterminatedName);
    auto invalidPrefix = Studio(true);
    Put(invalidPrefix, offsetof(studiohdr_t, texturedataindex), static_cast<int>(sizeof(studiohdr_t)));
    Reject(true, invalidPrefix);
    auto missingTable = Studio(true);
    Put(missingTable, offsetof(studiohdr_t, textureindex), 0);
    Reject(true, missingTable);
#if defined(SW_RENDERER) && !defined(MODEL_LOADING_BASELINE)
    // A declared native allocation must fail before any access to the fictitious large file extent.
    auto oversizedExpansion = Studio(true);
    Put(oversizedExpansion, offsetof(studiohdr_t, length), INT_MAX);
    Reset();
    model_t overflowModel {
    };
    bool expansionRejected = false;
    try {
        Mod_LoadStudioModel(&overflowModel, oversizedExpansion.data(), INT_MAX);
    }
    catch (const std::runtime_error&) {
        expansionRejected = true;
    }
    assert(expansionRejected && allocations == 0 && uploads == 0);
    r_pixbytes = 2;
    Reset();
    auto twoByteSprite = Sprite(false);
    Load(false, overflowModel, twoByteSprite);
    assert(overflowModel.cache.data);
    r_pixbytes = 1;
    auto truncatedSequential = Studio(true);
    // Hardware's indexed range remains backed, while the software sequential source does not.
    Put(truncatedSequential, sizeof(studiohdr_t) + offsetof(mstudiotexture_t, index), 0);
    Put(truncatedSequential, offsetof(studiohdr_t, texturedataindex),
        static_cast<int>(truncatedSequential.size() - 1));
    Reject(true, truncatedSequential);
#endif
    if (argc > 2) {
        int sprites = 0;
        int models = 0;
        for (const auto& entry : std::filesystem::recursive_directory_iterator(argv[2])) {
            if (!entry.is_regular_file()) {
                continue;
            }
            const auto extension = entry.path().extension();
            if (extension != ".mdl" && extension != ".spr") {
                continue;
            }
            std::ifstream input(entry.path(), std::ios::binary);
            std::vector<byte> bytes((std::istreambuf_iterator<char>(input)), {});
            if (bytes.size() < sizeof(int)) {
                continue;
            }
            const int identifier = Read<int>(bytes, 0);
            if (identifier != IDSPRITEHEADER && identifier != IDSTUDIOHEADER) {
                continue;
            }
            Reset();
            model_t model {
            };
            Load(identifier == IDSTUDIOHEADER, model, bytes);
            assert(model.cache.data);
            if (identifier == IDSTUDIOHEADER) { ++models; } else { ++sprites; }
        }
        std::printf("PASS corpus %d studio models, %d sprites\n", models, sprites);
    }
    std::puts("PASS bounded actual studio and sprite loaders");
}
