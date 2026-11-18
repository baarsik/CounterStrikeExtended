#pragma once

#include "legacy_state.h"
#include <map>
#include <unordered_map>
#include <memory>
#include <vector>

namespace cse {
namespace render {

constexpr std::size_t kPaletteEntries = 256;
constexpr std::size_t kPaletteBytes = kPaletteEntries * 3;
constexpr std::size_t kTextureStorageBytes = 256 * 1024 * 1024;
constexpr std::size_t kTextureUploadBytes = 128 * 1024 * 1024;
constexpr std::size_t kTextureNames = 65536;
// Engine handles encode palette metadata above the low 16-bit nonzero texture name.
constexpr std::uint32_t kMaximumGeneratedTextureName = 0xffffu;
constexpr int kTextureDimension = 16384;
constexpr int kTextureLevels = 32;
// GL's initial TEXTURE_MAX_LEVEL; independent of this CPU capture's explicit image-count bound.
constexpr int kDefaultTextureMaximumLevel = 1000;
constexpr std::size_t kEngineCachedTextureUnits = 2;

enum class TextureTarget : std::uint32_t { Texture2D, Rectangle };
enum class PixelFormat : std::uint32_t { RGBA, BGRA, RGB, Luminance, Alpha, LuminanceAlpha, ColorIndex };
enum class InternalFormat : std::uint32_t { RGBA8, RGB8, RGB5A1, RGB5, RGBA4, Luminance8, Alpha8,
    Intensity8, ColorIndex8, RGBA, RGB, Luminance, Alpha, Intensity };
enum class Filter : std::uint32_t { Nearest, Linear, NearestMipmapNearest, LinearMipmapNearest,
    NearestMipmapLinear, LinearMipmapLinear };
enum class Wrap : std::uint32_t { Repeat, Clamp, ClampToEdge };
enum class TextureParameter : std::uint32_t { MinFilter, MagFilter, WrapS, WrapT, MaximumLevel, Anisotropy };
enum class PixelStoreParameter : std::uint32_t { UnpackAlignment, UnpackRowLength, PackAlignment };

struct PixelStoreState {
    int unpackAlignment = 4;
    int unpackRowLength = 0;
    int packAlignment = 4;
};

struct TextureUpload {
    int level = 0;
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
    InternalFormat internal = InternalFormat::RGBA8;
    PixelFormat format = PixelFormat::RGBA;
    bool allocateOnly = false;
    std::vector<std::uint8_t> bytes;
};

struct SamplerState {
    Filter minification = Filter::NearestMipmapLinear;
    Filter magnification = Filter::Linear;
    Wrap wrapS = Wrap::Repeat;
    Wrap wrapT = Wrap::Repeat;
    int maximumLevel = kDefaultTextureMaximumLevel;
    float requestedAnisotropy = 1;
};

struct TextureImage {
    int width = 0;
    int height = 0;
    InternalFormat internal = InternalFormat::RGBA8;
    // Tight logical RGBA8 components, or one byte/index. Requested low precision remains
    // explicit in internal; backend storage conversion is not approximated on this CPU lane.
    std::vector<std::uint8_t> pixels;
    bool defined = false;
};

struct TextureObject {
    TextureTarget target = TextureTarget::Texture2D;
    std::uint64_t generation = 0;
    SamplerState sampler;
    std::map<int, std::shared_ptr<const TextureImage>> levels;
};

struct TextureResources {
    std::shared_ptr<const TextureObject> texture2D;
    std::shared_ptr<const TextureObject> rectangle;
};

using SharedPalette = std::array<std::uint8_t, kPaletteBytes>;

enum class TextureCapture { AllBindings, SampledBindings };

class TextureStore {
public:
    explicit TextureStore(std::size_t storageLimit = kTextureStorageBytes, std::size_t namesLimit = kTextureNames);
    Status Generate(std::size_t count, std::vector<std::uint32_t>& names);
    bool IsTexture(std::uint32_t name) const;
    std::shared_ptr<const TextureObject> Find(std::uint32_t name) const;
    Status Bind(LegacyState& state, TextureTarget target, std::uint32_t name);
    void Delete(LegacyState& state, const std::vector<std::uint32_t>& names);
    Status PixelStore(PixelStoreParameter parameter, int value);
    const PixelStoreState& Unpack() const {
        return pixelStore_;
    }
    Status Parameter(const LegacyState& state, TextureTarget target, TextureParameter parameter, double value);
    // Allocation-free caller-copy bound; output is unchanged on failure.
    static Status UploadByteExtent(const TextureUpload& upload, const PixelStoreState& unpack, std::size_t& output);
    static Status ValidateUpload(const TextureUpload& upload, const PixelStoreState& unpack, bool subimage);
    static Status ValidateUpload(const TextureUpload& upload, const PixelStoreState& unpack, bool subimage,
        std::size_t bytes);
    Status Upload(const LegacyState& state, TextureTarget target, const TextureUpload& upload,
        const PixelStoreState& unpack, bool subimage);
    // Copies synchronously into the immutable image while the caller owns the input.
    Status Upload(const LegacyState& state, TextureTarget target, const TextureUpload& upload,
        const PixelStoreState& unpack, bool subimage, const std::uint8_t* pixels, std::size_t bytes);
    Status Palette(const std::vector<std::uint8_t>& bytes);
    std::shared_ptr<const TextureObject> Bound(const LegacyState& state, TextureTarget target) const;
    std::array<TextureResources, kTextureUnits> Capture(const LegacyState& state,
        TextureCapture capture = TextureCapture::AllBindings) const;
    bool MatchesCapture(const LegacyState& state, TextureCapture capture,
        const std::array<TextureResources, kTextureUnits>& resources) const;
    std::shared_ptr<const SharedPalette> PaletteSnapshot() const {
        return palette_;
    }
    std::size_t StorageBytes() const;

private:
    const std::shared_ptr<const TextureObject>& CaptureBinding(const TextureUnitState& binding,
        TextureTarget target, TextureCapture capture) const;
    struct Storage {
        std::size_t used = 0;
        std::size_t limit = 0;
    };
    void Commit(const LegacyState& state, TextureTarget target, std::shared_ptr<const TextureObject> object);
    std::shared_ptr<TextureImage> NewImage(std::size_t bytes, const std::uint8_t* initial);
    std::shared_ptr<Storage> storage_;
    std::size_t namesLimit_;
    std::uint32_t nextName_ = 1;
    std::uint64_t generation_ = 0;
    std::unordered_map<std::uint32_t, std::shared_ptr<const TextureObject>> objects_;
    std::array<std::shared_ptr<const TextureObject>, 2> defaults_;
    PixelStoreState pixelStore_;
    std::shared_ptr<const SharedPalette> palette_;
};

struct EngineBindDecision {
    bool bind = false;
    std::uint32_t name = 0;
    int paletteIndex = -1;
};

// GL_Bind/GL_SelectTexture's separate software cache. Raw qgl binds/selects do not
// update it. A same-low16 hit skips palette selection, exactly as the owning source.
class EngineTextureCache {
public:
    EngineTextureCache();
    Status Select(std::size_t unit);
    EngineBindDecision Bind(std::uint32_t encodedHandle);

private:
    std::array<int, kEngineCachedTextureUnits> names_;
    std::size_t unit_ = 0;
};

}
}
