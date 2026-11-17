#include "legacy_texture.h"
#include "capture_allocation.h"
#include <algorithm>
#include <cmath>
#include <limits>

namespace cse {
namespace render {
namespace {

constexpr std::uint8_t kOpaqueComponent = std::numeric_limits<std::uint8_t>::max();

bool TargetValid(TextureTarget target) {
    return target == TextureTarget::Texture2D || target == TextureTarget::Rectangle;
}

std::uint32_t Name(const LegacyState& state, TextureTarget target) {
    const auto& unit = state.textures[state.ActiveTexture()];
    return target == TextureTarget::Texture2D ? unit.texture2D : unit.textureRectangle;
}

std::size_t Components(PixelFormat format) {
    switch (format) {
    case PixelFormat::RGBA:
    case PixelFormat::BGRA:
        return 4;
    case PixelFormat::RGB:
        return 3;
    case PixelFormat::LuminanceAlpha:
        return 2;
    case PixelFormat::Luminance:
    case PixelFormat::Alpha:
    case PixelFormat::ColorIndex:
        return 1;
    }
    return 0;
}

bool MultiplySize(std::size_t left, std::size_t right, std::size_t& result) {
    if (right != 0 && left > std::numeric_limits<std::size_t>::max() / right) {
        return false;
    }
    result = left * right;
    return true;
}

bool Layout(const TextureUpload& upload, const PixelStoreState& unpack, std::size_t& stride,
    std::size_t& extent) {
    const auto components = Components(upload.format);
    if (!components || unpack.unpackRowLength < 0 || (unpack.unpackAlignment != 1 &&
        unpack.unpackAlignment != 2 && unpack.unpackAlignment != 4 && unpack.unpackAlignment != 8)) {
        return false;
    }
    const auto row = static_cast<std::size_t>(unpack.unpackRowLength ? unpack.unpackRowLength : upload.width);
    std::size_t bytes = 0;
    const auto alignment = static_cast<std::size_t>(unpack.unpackAlignment);
    if (!MultiplySize(row, components, bytes) || bytes > std::numeric_limits<std::size_t>::max() - alignment + 1) {
        return false;
    }
    stride = (bytes + alignment - 1) / alignment * alignment;
    if (upload.width == 0 || upload.height == 0) {
        extent = 0;
        return true;
    }
    std::size_t last = 0;
    if (!MultiplySize(static_cast<std::size_t>(upload.height - 1), stride, extent) ||
        !MultiplySize(static_cast<std::size_t>(upload.width), components, last) ||
        last > std::numeric_limits<std::size_t>::max() - extent) {
        return false;
    }
    extent += last;
    return true;
}

std::array<std::uint8_t, 4> Decode(const std::uint8_t* source, PixelFormat format) {
    switch (format) {
    case PixelFormat::RGBA:
        return {{source[0], source[1], source[2], source[3]}};
    case PixelFormat::BGRA:
        return {{source[2], source[1], source[0], source[3]}};
    case PixelFormat::RGB:
        return {{source[0], source[1], source[2], kOpaqueComponent}};
    case PixelFormat::Luminance:
        return {{source[0], source[0], source[0], kOpaqueComponent}};
    case PixelFormat::Alpha:
        return {{0, 0, 0, source[0]}};
    case PixelFormat::LuminanceAlpha:
        return {{source[0], source[0], source[0], source[1]}};
    case PixelFormat::ColorIndex:
        return {{0, 0, 0, 0}};
    }
    return {{0, 0, 0, 0}};
}

void StoreComponents(std::array<std::uint8_t, 4>& value, InternalFormat internal) {
    if (internal == InternalFormat::RGB8 || internal == InternalFormat::RGB5 || internal == InternalFormat::RGB) {
        value[3] = kOpaqueComponent;
    } else if (internal == InternalFormat::Alpha8 || internal == InternalFormat::Alpha) {
        value[0] = value[1] = value[2] = kOpaqueComponent;
    } else if (internal == InternalFormat::Luminance8 || internal == InternalFormat::Intensity8 ||
        internal == InternalFormat::Luminance || internal == InternalFormat::Intensity) {
        value[1] = value[2] = value[0];
        value[3] = internal == InternalFormat::Intensity8 || internal == InternalFormat::Intensity ?
            value[0] : kOpaqueComponent;
    }
}

}

TextureStore::TextureStore(std::size_t storageLimit, std::size_t namesLimit)
    : storage_((BeforeCaptureAllocation(CaptureAllocationSite::RecorderConstruction), std::make_shared<Storage>())),
      namesLimit_(namesLimit) {
    storage_->limit = storageLimit;
    for (std::size_t index = 0; index < defaults_.size(); ++index) {
        BeforeCaptureAllocation(CaptureAllocationSite::TextureObjectVersion);
        auto object = std::make_shared<TextureObject>();
        object->target = static_cast<TextureTarget>(index);
        if (object->target == TextureTarget::Rectangle) {
            object->sampler.minification = Filter::Linear;
            object->sampler.wrapS = Wrap::ClampToEdge;
            object->sampler.wrapT = Wrap::ClampToEdge;
        }
        defaults_[index] = object;
    }
}

std::size_t TextureStore::StorageBytes() const {
    return storage_->used;
}

std::shared_ptr<TextureImage> TextureStore::NewImage(std::size_t bytes) {
    if (bytes > storage_->limit - storage_->used) {
        return nullptr;
    }
    auto storage = storage_;
    // The charge survives the store if an immutable draw snapshot still owns the image.
    BeforeCaptureAllocation(CaptureAllocationSite::TextureImageStorage);
    std::unique_ptr<TextureImage> image(new TextureImage);
    if (bytes != 0) {
        BeforeCaptureAllocation(CaptureAllocationSite::TextureImageStorage);
    }
    image->pixels.resize(bytes);
    // The shared owner needs a control block; inject before charging so an injected failure is uncharged.
    BeforeCaptureAllocation(CaptureAllocationSite::TextureImageStorage);
    storage_->used += bytes;
    return std::shared_ptr<TextureImage>(image.release(), [storage, bytes](TextureImage* value) {
        delete value;
        storage->used -= bytes;
    });
}

Status TextureStore::Generate(std::size_t count, std::vector<std::uint32_t>& names) {
    if (count > namesLimit_ - objects_.size() || count > kMaximumGeneratedTextureName) {
        return Status::LimitExceeded;
    }
    std::vector<std::uint32_t> generated;
    generated.reserve(count);
    auto candidate = nextName_;
    // Search each legal name at most once. Domain exhaustion leaves both the store and output unchanged.
    for (std::uint32_t visited = 0; generated.size() < count && visited < kMaximumGeneratedTextureName; ++visited) {
        if (objects_.count(candidate) == 0) {
            generated.push_back(candidate);
        }
        candidate = candidate == kMaximumGeneratedTextureName ? 1 : candidate + 1;
    }
    if (generated.size() != count) {
        return Status::LimitExceeded;
    }
    for (auto name : generated) {
        objects_.emplace(name, nullptr);
    }
    nextName_ = candidate;
    names = std::move(generated);
    return Status::Ok;
}

bool TextureStore::IsTexture(std::uint32_t name) const {
    return Find(name) != nullptr;
}

std::shared_ptr<const TextureObject> TextureStore::Find(std::uint32_t name) const {
    const auto found = objects_.find(name);
    return name != 0 && found != objects_.end() ? found->second : nullptr;
}

Status TextureStore::Bind(LegacyState& state, TextureTarget target, std::uint32_t name) {
    if (!TargetValid(target)) {
        return Status::InvalidValue;
    }
    if (name != 0) {
        const auto found = objects_.find(name);
        if (found != objects_.end() && found->second && found->second->target != target) {
            return Status::InvalidOperation;
        }
        if (found == objects_.end() && objects_.size() >= namesLimit_) {
            return Status::LimitExceeded;
        }
        if (found == objects_.end() || !found->second) {
            if (generation_ == std::numeric_limits<std::uint64_t>::max()) {
                return Status::LimitExceeded;
            }
            BeforeCaptureAllocation(CaptureAllocationSite::TextureObjectVersion);
            auto object = std::make_shared<TextureObject>();
            object->target = target;
            object->generation = ++generation_;
            if (target == TextureTarget::Rectangle) {
                object->sampler.minification = Filter::Linear;
                object->sampler.wrapS = Wrap::ClampToEdge;
                object->sampler.wrapT = Wrap::ClampToEdge;
            }
            objects_[name] = object;
        }
    }
    auto& unit = state.textures[state.ActiveTexture()];
    (target == TextureTarget::Texture2D ? unit.texture2D : unit.textureRectangle) = name;
    return Status::Ok;
}

void TextureStore::Delete(LegacyState& state, const std::vector<std::uint32_t>& names) {
    for (auto name : names) {
        if (!name) {
            continue;
        }
        objects_.erase(name);
        for (auto& unit : state.textures) {
            if (unit.texture2D == name) {
                unit.texture2D = 0;
            }
            if (unit.textureRectangle == name) {
                unit.textureRectangle = 0;
            }
        }
    }
}

Status TextureStore::PixelStore(PixelStoreParameter parameter, int value) {
    if (parameter == PixelStoreParameter::UnpackRowLength) {
        if (value < 0) {
            return Status::InvalidValue;
        }
        pixelStore_.unpackRowLength = value;
    } else if (parameter == PixelStoreParameter::UnpackAlignment || parameter == PixelStoreParameter::PackAlignment) {
        if (value != 1 && value != 2 && value != 4 && value != 8) {
            return Status::InvalidValue;
        }
        (parameter == PixelStoreParameter::UnpackAlignment ? pixelStore_.unpackAlignment :
            pixelStore_.packAlignment) = value;
    } else {
        return Status::InvalidValue;
    }
    return Status::Ok;
}

std::shared_ptr<const TextureObject> TextureStore::Bound(const LegacyState& state, TextureTarget target) const {
    if (!TargetValid(target)) {
        return nullptr;
    }
    const auto name = Name(state, target);
    return name == 0 ? defaults_[static_cast<std::size_t>(target)] : objects_.at(name);
}

void TextureStore::Commit(const LegacyState& state, TextureTarget target,
    std::shared_ptr<const TextureObject> object) {
    const auto name = Name(state, target);
    if (name == 0) {
        defaults_[static_cast<std::size_t>(target)] = std::move(object);
    } else {
        objects_[name] = std::move(object);
    }
}

Status TextureStore::Parameter(const LegacyState& state, TextureTarget target, TextureParameter parameter,
    double value) {
    if (!TargetValid(target) || !std::isfinite(value) || value < 0 || value > std::numeric_limits<int>::max()) {
        return Status::InvalidValue;
    }
    const auto prior = Bound(state, target);
    auto sampler = prior->sampler;
    if (parameter == TextureParameter::Anisotropy) {
        if (value < 1) {
            return Status::InvalidValue;
        }
        sampler.requestedAnisotropy = static_cast<float>(value);
    } else if (value != std::floor(value)) {
        return Status::InvalidValue;
    } else if (parameter == TextureParameter::MinFilter || parameter == TextureParameter::MagFilter) {
        const auto filter = static_cast<Filter>(static_cast<int>(value));
        const auto maximum = parameter == TextureParameter::MagFilter || target == TextureTarget::Rectangle ?
            Filter::Linear : Filter::LinearMipmapLinear;
        if (filter > maximum) {
            return Status::InvalidValue;
        }
        (parameter == TextureParameter::MinFilter ? sampler.minification : sampler.magnification) = filter;
    } else if (parameter == TextureParameter::WrapS || parameter == TextureParameter::WrapT) {
        const auto wrap = static_cast<Wrap>(static_cast<int>(value));
        if (wrap > Wrap::ClampToEdge || (target == TextureTarget::Rectangle && wrap == Wrap::Repeat)) {
            return Status::InvalidValue;
        }
        (parameter == TextureParameter::WrapS ? sampler.wrapS : sampler.wrapT) = wrap;
    } else if (parameter == TextureParameter::MaximumLevel) {
        sampler.maximumLevel = static_cast<int>(value);
    } else {
        return Status::InvalidValue;
    }
    const auto& current = prior->sampler;
    // Legacy drawing often repeats these calls. Keep the immutable identity so the GPU cache can reuse its image.
    if (sampler.minification == current.minification && sampler.magnification == current.magnification &&
        sampler.wrapS == current.wrapS && sampler.wrapT == current.wrapT &&
        sampler.maximumLevel == current.maximumLevel && sampler.requestedAnisotropy == current.requestedAnisotropy) {
        return Status::Ok;
    }
    BeforeCaptureAllocation(CaptureAllocationSite::TextureObjectVersion);
    auto object = std::make_shared<TextureObject>(*prior);
    object->sampler = sampler;
    Commit(state, target, std::move(object));
    return Status::Ok;
}

Status TextureStore::UploadByteExtent(const TextureUpload& upload, const PixelStoreState& unpack,
    std::size_t& output) {
    if (upload.width < 0 || upload.height < 0 || upload.width > kTextureDimension ||
        upload.height > kTextureDimension || upload.format > PixelFormat::ColorIndex) {
        return Status::InvalidValue;
    }
    std::size_t stride = 0;
    std::size_t extent = 0;
    if (!Layout(upload, unpack, stride, extent)) {
        return Status::InvalidValue;
    }
    if (extent > kTextureUploadBytes) {
        return Status::LimitExceeded;
    }
    output = extent;
    return Status::Ok;
}

Status TextureStore::ValidateUpload(const TextureUpload& upload, const PixelStoreState& unpack, bool subimage) {
    if (upload.level < 0 || upload.level >= kTextureLevels || upload.width < 0 || upload.height < 0 ||
        upload.width > kTextureDimension || upload.height > kTextureDimension || upload.x < 0 || upload.y < 0 ||
        upload.internal > InternalFormat::Intensity || upload.format > PixelFormat::ColorIndex ||
        (!subimage && (upload.x != 0 || upload.y != 0)) || (subimage && upload.allocateOnly)) {
        return Status::InvalidValue;
    }
    if (!subimage && ((upload.internal == InternalFormat::ColorIndex8) != (upload.format == PixelFormat::ColorIndex))) {
        return Status::InvalidOperation;
    }
    std::size_t stride = 0;
    std::size_t extent = 0;
    if (!Layout(upload, unpack, stride, extent)) {
        return Status::InvalidValue;
    }
    if (upload.bytes.size() > kTextureUploadBytes || extent > kTextureUploadBytes) {
        return Status::LimitExceeded;
    }
    return upload.allocateOnly || upload.bytes.size() >= extent ? Status::Ok : Status::InvalidValue;
}

Status TextureStore::Upload(const LegacyState& state, TextureTarget target, const TextureUpload& upload,
    const PixelStoreState& unpack, bool subimage) {
    const auto validation = ValidateUpload(upload, unpack, subimage);
    if (validation != Status::Ok || !TargetValid(target)) {
        return validation == Status::Ok ? Status::InvalidValue : validation;
    }
    if (target == TextureTarget::Rectangle && upload.level != 0) {
        return Status::InvalidValue;
    }
    const auto prior = Bound(state, target);
    const auto found = prior->levels.find(upload.level);
    const TextureImage* old = found == prior->levels.end() ? nullptr : found->second.get();
    if (subimage && (!old || upload.x > old->width || upload.y > old->height ||
        upload.width > old->width - upload.x || upload.height > old->height - upload.y)) {
        return Status::InvalidValue;
    }
    const auto internal = subimage ? old->internal : upload.internal;
    const bool indexed = internal == InternalFormat::ColorIndex8;
    if (indexed != (upload.format == PixelFormat::ColorIndex)) {
        return Status::InvalidOperation;
    }
    if (subimage && (upload.width == 0 || upload.height == 0)) {
        return Status::Ok;
    }
    const auto width = subimage ? old->width : upload.width;
    const auto height = subimage ? old->height : upload.height;
    std::size_t size = 0;
    if (!MultiplySize(static_cast<std::size_t>(width), static_cast<std::size_t>(height), size) ||
        !MultiplySize(size, indexed ? 1 : 4, size)) {
        return Status::LimitExceeded;
    }
    auto image = NewImage(size);
    if (!image) {
        return Status::LimitExceeded;
    }
    image->width = width;
    image->height = height;
    image->internal = internal;
    image->defined = !upload.allocateOnly && (!subimage || old->defined ||
        (upload.width == width && upload.height == height));
    if (subimage) {
        image->pixels = old->pixels;
    }
    std::size_t stride = 0;
    std::size_t extent = 0;
    Layout(upload, unpack, stride, extent);
    if (!upload.allocateOnly) {
        const auto components = Components(upload.format);
        for (int y = 0; y < upload.height; ++y) {
            for (int x = 0; x < upload.width; ++x) {
                const auto* pixel = upload.bytes.data() + static_cast<std::size_t>(y) * stride +
                    static_cast<std::size_t>(x) * components;
                const auto destination = static_cast<std::size_t>(y + upload.y) * static_cast<std::size_t>(width) +
                    static_cast<std::size_t>(x + upload.x);
                if (indexed) {
                    image->pixels[destination] = *pixel;
                } else {
                    auto value = Decode(pixel, upload.format);
                    StoreComponents(value, internal);
                    std::copy(value.begin(), value.end(), image->pixels.begin() + destination * value.size());
                }
            }
        }
    }
    BeforeCaptureAllocation(CaptureAllocationSite::TextureObjectVersion);
    auto object = std::make_shared<TextureObject>(*prior);
    object->levels[upload.level] = std::move(image);
    Commit(state, target, std::move(object));
    return Status::Ok;
}

Status TextureStore::Palette(const std::vector<std::uint8_t>& bytes) {
    if (bytes.size() != kPaletteBytes) {
        return Status::InvalidValue;
    }
    auto palette = std::make_shared<SharedPalette>();
    std::copy(bytes.begin(), bytes.end(), palette->begin());
    palette_ = std::move(palette);
    return Status::Ok;
}

std::array<TextureResources, kTextureUnits> TextureStore::Capture(const LegacyState& state) const {
    std::array<TextureResources, kTextureUnits> resources;
    auto selected = state;
    for (std::size_t unit = 0; unit < resources.size(); ++unit) {
        selected.SelectTexture(static_cast<std::uint32_t>(unit));
        resources[unit].texture2D = Bound(selected, TextureTarget::Texture2D);
        resources[unit].rectangle = Bound(selected, TextureTarget::Rectangle);
    }
    return resources;
}

EngineTextureCache::EngineTextureCache() {
    names_.fill(-1);
}

Status EngineTextureCache::Select(std::size_t unit) {
    if (unit >= names_.size()) {
        return Status::InvalidValue;
    }
    unit_ = unit;
    return Status::Ok;
}

EngineBindDecision EngineTextureCache::Bind(std::uint32_t encodedHandle) {
    constexpr std::uint32_t NameMask = 0xffff;
    constexpr std::uint32_t SignBitHighWord = 0x8000;
    const auto name = encodedHandle & NameMask;
    if (names_[unit_] == static_cast<int>(name)) {
        return {false, name, -1};
    }
    names_[unit_] = static_cast<int>(name);
    const auto high = encodedHandle >> 16;
    const int palette = high != 0 && high < SignBitHighWord ? static_cast<int>(high) - 1 : -1;
    return {true, name, palette};
}

}
}
