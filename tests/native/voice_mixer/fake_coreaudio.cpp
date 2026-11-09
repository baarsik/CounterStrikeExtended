#include "fake_coreaudio.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <tuple>

namespace fake_mixer {

bool Key::operator<(const Key& other) const {
    return std::tie(object, address.mSelector, address.mScope, address.mElement)
        < std::tie(other.object, other.address.mSelector, other.address.mScope, other.address.mElement);
}
Property::~Property() {
    if (cfObject != nullptr) {
        CFRelease(cfObject);
    }
}
Store::~Store() {
    CheckReleased();
}
void Store::CheckReleased() const {
    if (!allocations.empty() || cfCopies != cfReleases) {
        std::abort();
    }
}
Property& Store::At(AudioObjectID object, AudioObjectPropertySelector selector, AudioObjectPropertyScope scope,
    AudioObjectPropertyElement element) {
    return properties[{object, {selector, scope, element}}];
}
void Store::Erase(AudioObjectID object, AudioObjectPropertySelector selector, AudioObjectPropertyScope scope,
    AudioObjectPropertyElement element) {
    properties.erase({object, {selector, scope, element}});
}
void Store::PutBytes(AudioObjectID object, AudioObjectPropertySelector selector, const void* data, std::size_t bytes,
    AudioObjectPropertyScope scope, AudioObjectPropertyElement element) {
    Property& property = At(object, selector, scope, element);
    if (property.cfObject != nullptr) {
        CFRelease(property.cfObject);
        property.cfObject = nullptr;
    }
    if (bytes == 0) {
        property.bytes.clear();
        return;
    }
    const auto* input = static_cast<const unsigned char*>(data);
    property.bytes.assign(input, input + bytes);
}
void Store::PutCF(AudioObjectID object, AudioObjectPropertySelector selector, CFTypeRef value) {
    Property& property = At(object, selector);
    if (property.cfObject != nullptr) {
        CFRelease(property.cfObject);
    }
    property.cfObject = CFRetain(value);
    const auto* bytes = reinterpret_cast<const unsigned char*>(&value);
    property.bytes.assign(bytes, bytes + sizeof(value));
}
void Store::FixedMicrophone(UInt32 channels) {
    const UInt32 one = 1;
    Put(kAudioObjectSystemObject, kAudioHardwarePropertyDefaultInputDevice, device);
    Put(device, kAudioDevicePropertyDeviceIsAlive, one);
    Put(device, kAudioDevicePropertyStreams, stream, kAudioObjectPropertyScopeInput);
    Put(stream, kAudioStreamPropertyDirection, one);
    Put(stream, kAudioStreamPropertyTerminalType, static_cast<UInt32>(kAudioStreamTerminalTypeMicrophone));
    Put(stream, kAudioStreamPropertyStartingChannel, one);
    AudioStreamBasicDescription format = {};
    format.mChannelsPerFrame = channels;
    Put(stream, kAudioStreamPropertyVirtualFormat, format);
    AudioBufferList configuration = {};
    configuration.mNumberBuffers = 1;
    configuration.mBuffers[0].mNumberChannels = channels;
    Put(device, kAudioDevicePropertyStreamConfiguration, configuration, kAudioObjectPropertyScopeInput);
    const Float32 volume = 0.625f;
    Put(device, kAudioDevicePropertyVolumeScalar, volume, kAudioObjectPropertyScopeInput);
}
void Store::Boost(const char* name, AudioClassID type) {
    Put(device, kAudioObjectPropertyControlList, boost);
    Put(boost, kAudioObjectPropertyClass, type);
    Put(boost, kAudioObjectPropertyBaseClass, static_cast<AudioClassID>(kAudioBooleanControlClassID));
    Put(boost, kAudioControlPropertyScope, static_cast<AudioObjectPropertyScope>(kAudioObjectPropertyScopeInput));
    Put(boost, kAudioControlPropertyElement, static_cast<AudioObjectPropertyElement>(kAudioObjectPropertyElementMain));
    Put(boost, kAudioBooleanControlPropertyValue, static_cast<UInt32>(1));
    CFStringRef text = CFStringCreateWithCString(nullptr, name, kCFStringEncodingUTF8);
    PutCF(boost, kAudioObjectPropertyName, text);
    CFRelease(text);
}

namespace {

Property* Find(Store& store, AudioObjectID object, const AudioObjectPropertyAddress* address) {
    ++store.propertyCalls;
    const auto found = store.properties.find({object, *address});
    return found == store.properties.end() ? nullptr : &found->second;
}
Boolean Has(void* context, AudioObjectID object, const AudioObjectPropertyAddress* address) {
    return Find(*static_cast<Store*>(context), object, address) != nullptr;
}
OSStatus Settable(void* context, AudioObjectID object, const AudioObjectPropertyAddress* address, Boolean* result) {
    auto& store = *static_cast<Store*>(context);
    Property* property = Find(store, object, address);
    if (property == nullptr) {
        return kAudioHardwareUnknownPropertyError;
    }
    if (store.onSettable) {
        auto operation = std::move(store.onSettable);
        operation({object, *address});
    }
    if (property->settableStatus != noErr) {
        return property->settableStatus;
    }
    *result = property->writable;
    return noErr;
}
OSStatus GetSize(void* context, AudioObjectID object, const AudioObjectPropertyAddress* address, UInt32 qualifierSize,
    const void* qualifier, UInt32* result) {
    if (qualifierSize != 0 || qualifier != nullptr) {
        std::abort();
    }
    Property* property = Find(*static_cast<Store*>(context), object, address);
    if (property == nullptr) {
        return kAudioHardwareUnknownPropertyError;
    }
    ++property->sizes;
    *result = property->overrideSize ? property->reportedSize : static_cast<UInt32>(property->bytes.size());
    return property->sizeStatus;
}
OSStatus GetData(void* context, AudioObjectID object, const AudioObjectPropertyAddress* address, UInt32 qualifierSize,
    const void* qualifier, UInt32* bytes, void* result) {
    if (qualifierSize != 0 || qualifier != nullptr) {
        std::abort();
    }
    auto& store = *static_cast<Store*>(context);
    Property* property = Find(store, object, address);
    if (property == nullptr) {
        return kAudioHardwareUnknownPropertyError;
    }
    ++property->gets;
    if (property->readStatus != noErr) {
        return property->readStatus;
    }
    if (property->changingReads != 0) {
        --property->changingReads;
        return kAudioHardwareBadPropertySizeError;
    }
    if (*bytes < property->bytes.size()) {
        return kAudioHardwareBadPropertySizeError;
    }
    std::memcpy(result, property->bytes.data(), property->bytes.size());
    *bytes = static_cast<UInt32>(property->bytes.size()) + (property->wrongReturnedSize ? 1 : 0);
    if (property->cfObject != nullptr) {
        CFRetain(property->cfObject);
        ++store.cfCopies;
    }
    if (store.onGet) {
        store.onGet({object, *address});
    }
    return noErr;
}
OSStatus SetData(void* context, AudioObjectID object, const AudioObjectPropertyAddress* address, UInt32 qualifierSize,
    const void* qualifier, UInt32 bytes, const void* value) {
    if (qualifierSize != 0 || qualifier != nullptr) {
        std::abort();
    }
    auto& store = *static_cast<Store*>(context);
    Property* property = Find(store, object, address);
    if (property == nullptr || !property->writable) {
        return kAudioHardwareUnknownPropertyError;
    }
    ++property->sets;
    ++store.writes;
    if (property->writeStatus != noErr) {
        return property->writeStatus;
    }
    const auto* input = static_cast<const unsigned char*>(value);
    property->bytes.assign(input, input + bytes);
    return noErr;
}
void* Allocate(void* context, std::size_t bytes) {
    auto& store = *static_cast<Store*>(context);
    ++store.allocationCalls;
    store.largestRequest = std::max(store.largestRequest, bytes);
    constexpr std::size_t maximumFixtureAllocation = 1024 * 1024;
    if (store.allocationCalls == store.failAllocation || bytes > maximumFixtureAllocation) {
        return nullptr;
    }
    void* result = std::malloc(bytes);
    if (result != nullptr) {
        store.allocations[result] = bytes;
    }
    return result;
}
void Free(void* context, void* value) {
    auto& store = *static_cast<Store*>(context);
    if (value == nullptr || store.allocations.erase(value) != 1) {
        std::abort();
    }
    ++store.frees;
    std::free(value);
}
void ReleaseCF(void* context, CFTypeRef value) {
    ++static_cast<Store*>(context)->cfReleases;
    CFRelease(value);
}

}

cse::CoreAudioMixerServices Store::Services() {
    return {this, Has, Settable, GetSize, GetData, SetData, Allocate, Free, ReleaseCF};
}

}
