#include "voice_mixer_controls_coreaudio.h"
#include "voice_mixer_controls.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <new>

namespace cse {
namespace {

constexpr unsigned int propertyReadAttempts = 2;
constexpr UInt32 inputStreamDirection = 1;
constexpr UInt32 firstDeviceChannel = 1;

static_assert(sizeof(Float32) == sizeof(float) && sizeof(UInt32) == 4,
    "Native mixer hardware scalars and identifiers retain their SDK widths");

AudioObjectPropertyAddress Address(AudioObjectPropertySelector selector,
    AudioObjectPropertyScope scope = kAudioObjectPropertyScopeGlobal,
    AudioObjectPropertyElement element = kAudioObjectPropertyElementMain) {
    return {selector, scope, element};
}

class PropertyBuffer {
public:
    explicit PropertyBuffer(const CoreAudioMixerServices& services) : services_(services) {
    }
    ~PropertyBuffer() {
        Clear();
    }
    PropertyBuffer(const PropertyBuffer&) = delete;
    PropertyBuffer& operator=(const PropertyBuffer&) = delete;

    bool Allocate(std::size_t bytes) {
        Clear();
        if (bytes != 0) {
            data_ = services_.allocate(services_.context, bytes);
            if (data_ == nullptr) {
                return false;
            }
        }
        bytes_ = bytes;
        return true;
    }

    bool Read(AudioObjectID object, const AudioObjectPropertyAddress& address, std::size_t unit) {
        for (unsigned int attempt = 0; attempt < propertyReadAttempts; ++attempt) {
            UInt32 requested = 0;
            if (services_.getSize(services_.context, object, &address, 0, nullptr, &requested) != noErr
                    || requested % unit != 0 || !Allocate(requested)) {
                return false;
            }
            if (requested == 0) {
                return true;
            }
            UInt32 returned = requested;
            const OSStatus status = services_.getData(services_.context, object, &address, 0, nullptr,
                &returned, data_);
            if (status == kAudioHardwareBadPropertySizeError || returned > requested) {
                continue;
            }
            if (status != noErr || returned % unit != 0) {
                return false;
            }
            bytes_ = returned;
            return true;
        }
        return false;
    }

    template<typename Value>
    Value At(std::size_t offset) const {
        Value value = {};
        std::memcpy(&value, static_cast<const unsigned char*>(data_) + offset, sizeof(value));
        return value;
    }

    void* Data() {
        return data_;
    }
    std::size_t Bytes() const {
        return bytes_;
    }

private:
    void Clear() {
        if (data_ != nullptr) {
            services_.free(services_.context, data_);
            data_ = nullptr;
        }
        bytes_ = 0;
    }
    const CoreAudioMixerServices& services_;
    void* data_ = nullptr;
    std::size_t bytes_ = 0;
};

class CoreAudioMixer final : public IMixerControls {
public:
    explicit CoreAudioMixer(const CoreAudioMixerServices& services) : services_(services) {
    }
    CoreAudioMixer(const CoreAudioMixer&) = delete;
    CoreAudioMixer& operator=(const CoreAudioMixer&) = delete;

    void Release() override {
        const auto services = services_;
        this->~CoreAudioMixer();
        services.free(services.context, this);
    }

    bool GetValue_Float(Control control, float& output) override {
        Target target;
        if (!Resolve(control, target)) {
            return false;
        }
        float value = 0;
        if (target.kind == Kind::Volume) {
            if (!Read(target.object, target.address, value) || !Normalized(value)) {
                return false;
            }
        } else {
            UInt32 raw = 0;
            if (!Read(target.object, target.address, raw)) {
                return false;
            }
            value = ((raw != 0) != (target.kind == Kind::Monitoring)) ? 1.0f : 0.0f;
        }
        Target verified;
        if (!Resolve(control, verified) || !target.Same(verified)) {
            return false;
        }
        output = value;
        return true;
    }

    bool SetValue_Float(Control control, float value) override {
        if (control == MicVolume && !Normalized(value)) {
            return false;
        }
        Target target;
        if (!Resolve(control, target)) {
            return false;
        }
        Boolean settable = false;
        if (services_.isSettable(services_.context, target.object, &target.address, &settable) != noErr
                || !settable) {
            return false;
        }
        Target verified;
        if (!Resolve(control, verified) || !target.Same(verified)) {
            return false;
        }
        if (target.kind == Kind::Volume) {
            return Write(target.object, target.address, value);
        }
        const UInt32 raw = ((value != 0) != (target.kind == Kind::Monitoring)) ? 1 : 0;
        return Write(target.object, target.address, raw);
    }

    bool SelectMicrophoneForWaveInput() override {
        Binding binding;
        if (!Bind(binding) || !binding.selector) {
            return false;
        }
        const auto address = Address(kAudioDevicePropertyDataSource, kAudioObjectPropertyScopeInput);
        Boolean settable = false;
        if (services_.isSettable(services_.context, binding.device, &address, &settable) != noErr || !settable) {
            return false;
        }
        Binding verified;
        if (!Bind(verified) || verified.device != binding.device || !verified.selector
                || verified.source != binding.source) {
            return false;
        }
        return Write(binding.device, address, binding.source);
    }

private:
    enum class Kind {
        Volume,
        Monitoring,
        Boost
    };
    struct Target {
        AudioObjectID owner = kAudioObjectUnknown;
        AudioObjectID object = kAudioObjectUnknown;
        AudioObjectPropertyAddress address = {};
        Kind kind = Kind::Volume;
        bool Same(const Target& other) const {
            return owner == other.owner && object == other.object && kind == other.kind
                && address.mSelector == other.address.mSelector && address.mScope == other.address.mScope
                && address.mElement == other.address.mElement;
        }
    };
    struct Binding {
        AudioObjectID device = kAudioObjectUnknown;
        UInt32 channels = 0;
        UInt32 soleChannel = 0;
        bool selector = false;
        UInt32 source = 0;
    };
    struct Interval {
        UInt32 first;
        UInt32 last;
    };

    static bool Normalized(float value) {
        return std::isfinite(value) && value >= 0.0f && value <= 1.0f;
    }

    template<typename Value>
    bool Read(AudioObjectID object, const AudioObjectPropertyAddress& address, Value& output) const {
        UInt32 bytes = 0;
        if (services_.getSize(services_.context, object, &address, 0, nullptr, &bytes) != noErr
                || bytes != sizeof(Value)) {
            return false;
        }
        Value value = {};
        const OSStatus status = services_.getData(services_.context, object, &address, 0, nullptr, &bytes, &value);
        if (status != noErr || bytes != sizeof(value)) {
            return false;
        }
        output = value;
        return true;
    }

    template<typename Value>
    bool Write(AudioObjectID object, const AudioObjectPropertyAddress& address, const Value& value) const {
        UInt32 bytes = 0;
        if (services_.getSize(services_.context, object, &address, 0, nullptr, &bytes) != noErr
                || bytes != sizeof(value)) {
            return false;
        }
        return services_.setData(services_.context, object, &address, 0, nullptr, sizeof(value), &value) == noErr;
    }

    bool Has(AudioObjectID object, const AudioObjectPropertyAddress& address) const {
        return services_.hasProperty(services_.context, object, &address) != 0;
    }

    bool Bind(Binding& binding) const {
        if (!Read(kAudioObjectSystemObject, Address(kAudioHardwarePropertyDefaultInputDevice), binding.device)
                || binding.device == kAudioObjectUnknown) {
            return false;
        }
        UInt32 alive = 0;
        if (!Read(binding.device, Address(kAudioDevicePropertyDeviceIsAlive), alive) || alive == 0) {
            return false;
        }
        PropertyBuffer configuration(services_);
        if (!configuration.Read(binding.device,
                Address(kAudioDevicePropertyStreamConfiguration, kAudioObjectPropertyScopeInput), 1)) {
            return false;
        }
        constexpr std::size_t bufferHeaderBytes = offsetof(AudioBufferList, mBuffers);
        if (configuration.Bytes() < bufferHeaderBytes) {
            return false;
        }
        const UInt32 buffers = configuration.At<UInt32>(0);
        if (buffers == 0 || buffers != (configuration.Bytes() - bufferHeaderBytes) / sizeof(AudioBuffer)
                || (configuration.Bytes() - bufferHeaderBytes) % sizeof(AudioBuffer) != 0) {
            return false;
        }
        std::uint64_t channels = 0;
        for (UInt32 index = 0; index < buffers; ++index) {
            const auto buffer = configuration.At<AudioBuffer>(bufferHeaderBytes + index * sizeof(AudioBuffer));
            if (buffer.mData != nullptr || buffer.mNumberChannels == 0) {
                return false;
            }
            channels += buffer.mNumberChannels;
        }
        if (channels == 0 || channels > (std::numeric_limits<UInt32>::max)()) {
            return false;
        }
        PropertyBuffer streams(services_);
        if (!streams.Read(binding.device, Address(kAudioDevicePropertyStreams, kAudioObjectPropertyScopeInput),
                sizeof(AudioStreamID)) || streams.Bytes() == 0) {
            return false;
        }
        const std::size_t count = streams.Bytes() / sizeof(AudioStreamID);
        if (count > (std::numeric_limits<std::size_t>::max)() / sizeof(Interval)) {
            return false;
        }
        PropertyBuffer intervals(services_);
        if (!intervals.Allocate(count * sizeof(Interval))) {
            return false;
        }
        auto* ranges = static_cast<Interval*>(intervals.Data());
        for (std::size_t index = 0; index < count; ++index) {
            const auto stream = streams.At<AudioStreamID>(index * sizeof(AudioStreamID));
            UInt32 direction = 0;
            UInt32 terminal = 0;
            UInt32 first = 0;
            AudioStreamBasicDescription format = {};
            if (!Read(stream, Address(kAudioStreamPropertyDirection), direction) || direction != inputStreamDirection
                    || !Read(stream, Address(kAudioStreamPropertyTerminalType), terminal)
                    || (terminal != kAudioStreamTerminalTypeMicrophone
                        && terminal != kAudioStreamTerminalTypeHeadsetMicrophone
                        && terminal != kAudioStreamTerminalTypeReceiverMicrophone)
                    || !Read(stream, Address(kAudioStreamPropertyStartingChannel), first) || first == 0
                    || !Read(stream, Address(kAudioStreamPropertyVirtualFormat), format)
                    || format.mChannelsPerFrame == 0) {
                return false;
            }
            const std::uint64_t last = static_cast<std::uint64_t>(first) + format.mChannelsPerFrame - 1;
            if (last > channels) {
                return false;
            }
            new (&ranges[index]) Interval{first, static_cast<UInt32>(last)};
        }
        std::sort(ranges, ranges + count, [](const Interval& first, const Interval& second) {
            return first.first < second.first;
        });
        std::uint64_t next = firstDeviceChannel;
        for (std::size_t index = 0; index < count; ++index) {
            if (ranges[index].first != next) {
                return false;
            }
            next = static_cast<std::uint64_t>(ranges[index].last) + 1;
        }
        if (next - 1 != channels) {
            return false;
        }
        binding.channels = static_cast<UInt32>(channels);
        binding.soleChannel = channels == 1 ? ranges[0].first : 0;
        const auto available = Address(kAudioDevicePropertyDataSources, kAudioObjectPropertyScopeInput);
        const auto current = Address(kAudioDevicePropertyDataSource, kAudioObjectPropertyScopeInput);
        const bool hasSources = Has(binding.device, available);
        if (hasSources != Has(binding.device, current)) {
            return false;
        }
        if (hasSources) {
            PropertyBuffer choices(services_);
            PropertyBuffer selected(services_);
            if (!choices.Read(binding.device, available, sizeof(UInt32)) || choices.Bytes() != sizeof(UInt32)
                    || !selected.Read(binding.device, current, sizeof(UInt32)) || selected.Bytes() != sizeof(UInt32)
                    || choices.At<UInt32>(0) != selected.At<UInt32>(0)) {
                return false;
            }
            binding.selector = true;
            binding.source = choices.At<UInt32>(0);
        }
        return true;
    }

    bool Resolve(Control control, Target& target) const {
        if (control != MicVolume && control != MicMute && control != MicBoost) {
            return false;
        }
        Binding binding;
        if (!Bind(binding)) {
            return false;
        }
        target.owner = binding.device;
        target.object = binding.device;
        if (control == MicBoost) {
            return FindBoost(binding, target);
        }
        if (control == MicMute) {
            target.kind = Kind::Monitoring;
            target.address = Address(kAudioDevicePropertyPlayThru, kAudioDevicePropertyScopePlayThrough);
            return Has(binding.device, target.address);
        }
        target.address = Address(kAudioDevicePropertyVolumeScalar, kAudioObjectPropertyScopeInput);
        if (!Has(binding.device, target.address)) {
            if (binding.soleChannel == 0) {
                return false;
            }
            target.address.mElement = binding.soleChannel;
            if (!Has(binding.device, target.address)) {
                return false;
            }
        }
        target.kind = Kind::Volume;
        return true;
    }

    static bool StandardNonBoostClass(AudioClassID type) {
        return type == kAudioMuteControlClassID || type == kAudioSoloControlClassID
            || type == kAudioJackControlClassID || type == kAudioLFEMuteControlClassID
            || type == kAudioPhantomPowerControlClassID || type == kAudioPhaseInvertControlClassID
            || type == kAudioClipLightControlClassID || type == kAudioTalkbackControlClassID
            || type == kAudioListenbackControlClassID;
    }

    bool BoostName(AudioObjectID object) const {
        const auto address = Address(kAudioObjectPropertyName);
        UInt32 bytes = 0;
        if (services_.getSize(services_.context, object, &address, 0, nullptr, &bytes) != noErr
                || bytes != sizeof(CFTypeRef)) {
            return false;
        }
        CFTypeRef value = nullptr;
        if (services_.getData(services_.context, object, &address, 0, nullptr, &bytes, &value) != noErr) {
            return false;
        }
        bool matched = false;
        if (value != nullptr && bytes == sizeof(value) && CFGetTypeID(value) == CFStringGetTypeID()) {
            const auto text = static_cast<CFStringRef>(value);
            matched = CFStringFind(text, CFSTR("Gain"), 0).location != kCFNotFound
                || CFStringFind(text, CFSTR("Boost"), 0).location != kCFNotFound
                || CFStringFind(text, CFSTR("+20d"), 0).location != kCFNotFound;
        }
        if (value != nullptr) {
            services_.releaseCF(services_.context, value);
        }
        return matched;
    }

    bool FindBoost(const Binding& binding, Target& target) const {
        PropertyBuffer controls(services_);
        if (!controls.Read(binding.device, Address(kAudioObjectPropertyControlList), sizeof(AudioObjectID))) {
            return false;
        }
        bool found = false;
        const std::size_t count = controls.Bytes() / sizeof(AudioObjectID);
        for (std::size_t index = 0; index < count; ++index) {
            const auto object = controls.At<AudioObjectID>(index * sizeof(AudioObjectID));
            AudioClassID type = 0;
            AudioClassID base = 0;
            AudioObjectPropertyScope scope = 0;
            AudioObjectPropertyElement element = 0;
            if (!Read(object, Address(kAudioObjectPropertyClass), type) || StandardNonBoostClass(type)
                    || (type != kAudioBooleanControlClassID
                        && (!Read(object, Address(kAudioObjectPropertyBaseClass), base)
                            || base != kAudioBooleanControlClassID))
                    || !Read(object, Address(kAudioControlPropertyScope), scope)
                    || scope != kAudioObjectPropertyScopeInput
                    || !Read(object, Address(kAudioControlPropertyElement), element)
                    || (element != kAudioObjectPropertyElementMain && element != binding.soleChannel)
                    || !BoostName(object)) {
                continue;
            }
            const auto valueAddress = Address(kAudioBooleanControlPropertyValue);
            if (Has(object, valueAddress)) {
                // Retain the last matching control, as the WinMM enumeration does.
                target.object = object;
                target.address = valueAddress;
                target.kind = Kind::Boost;
                found = true;
            }
        }
        return found;
    }

    const CoreAudioMixerServices services_;
};

Boolean NativeHas(void*, AudioObjectID object, const AudioObjectPropertyAddress* address) {
    return AudioObjectHasProperty(object, address);
}
OSStatus NativeSettable(void*, AudioObjectID object, const AudioObjectPropertyAddress* address, Boolean* value) {
    return AudioObjectIsPropertySettable(object, address, value);
}
OSStatus NativeSize(void*, AudioObjectID object, const AudioObjectPropertyAddress* address, UInt32 size,
    const void* qualifier, UInt32* bytes) {
    return AudioObjectGetPropertyDataSize(object, address, size, qualifier, bytes);
}
OSStatus NativeGet(void*, AudioObjectID object, const AudioObjectPropertyAddress* address, UInt32 size,
    const void* qualifier, UInt32* bytes, void* value) {
    return AudioObjectGetPropertyData(object, address, size, qualifier, bytes, value);
}
OSStatus NativeSet(void*, AudioObjectID object, const AudioObjectPropertyAddress* address, UInt32 size,
    const void* qualifier, UInt32 bytes, const void* value) {
    return AudioObjectSetPropertyData(object, address, size, qualifier, bytes, value);
}
void* NativeAllocate(void*, std::size_t bytes) {
    return std::malloc(bytes);
}
void NativeFree(void*, void* address) {
    std::free(address);
}
void NativeReleaseCF(void*, CFTypeRef value) {
    CFRelease(value);
}

}

IMixerControls* CreateCoreAudioMixerControls(const CoreAudioMixerServices& services) {
    if (services.hasProperty == nullptr || services.isSettable == nullptr || services.getSize == nullptr
            || services.getData == nullptr || services.setData == nullptr || services.allocate == nullptr
            || services.free == nullptr || services.releaseCF == nullptr) {
        return nullptr;
    }
    void* storage = services.allocate(services.context, sizeof(CoreAudioMixer));
    return storage == nullptr ? nullptr : new (storage) CoreAudioMixer(services);
}

CoreAudioMixerServices NativeMixerServices() {
    return {nullptr, NativeHas, NativeSettable, NativeSize, NativeGet, NativeSet,
        NativeAllocate, NativeFree, NativeReleaseCF};
}

}

IMixerControls* GetMixerControls() {
    return cse::CreateCoreAudioMixerControls(cse::NativeMixerServices());
}
