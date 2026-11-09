#ifndef CSE_FAKE_COREAUDIO_MIXER_H
#define CSE_FAKE_COREAUDIO_MIXER_H

#include "voice_mixer_controls_coreaudio.h"
#include <functional>
#include <cstdlib>
#include <cstring>
#include <map>
#include <vector>

namespace fake_mixer {

constexpr AudioObjectID device = 20;
constexpr AudioObjectID stream = 30;
constexpr AudioObjectID boost = 40;

struct Key {
    AudioObjectID object;
    AudioObjectPropertyAddress address;
    bool operator<(const Key& other) const;
};

struct Property {
    std::vector<unsigned char> bytes;
    bool writable = true;
    OSStatus readStatus = noErr;
    OSStatus writeStatus = noErr;
    OSStatus sizeStatus = noErr;
    OSStatus settableStatus = noErr;
    UInt32 reportedSize = 0;
    bool overrideSize = false;
    bool wrongReturnedSize = false;
    unsigned int changingReads = 0;
    unsigned int gets = 0;
    unsigned int sizes = 0;
    unsigned int sets = 0;
    CFTypeRef cfObject = nullptr;
    ~Property();
};

struct Store {
    std::map<Key, Property> properties;
    std::map<void*, std::size_t> allocations;
    unsigned int allocationCalls = 0;
    unsigned int frees = 0;
    unsigned int failAllocation = 0;
    unsigned int cfCopies = 0;
    unsigned int cfReleases = 0;
    unsigned int propertyCalls = 0;
    unsigned int writes = 0;
    std::size_t largestRequest = 0;
    std::function<void(const Key&)> onGet;
    std::function<void(const Key&)> onSettable;

    ~Store();
    cse::CoreAudioMixerServices Services();
    Property& At(AudioObjectID object, AudioObjectPropertySelector selector,
        AudioObjectPropertyScope scope = kAudioObjectPropertyScopeGlobal,
        AudioObjectPropertyElement element = kAudioObjectPropertyElementMain);
    void Erase(AudioObjectID object, AudioObjectPropertySelector selector,
        AudioObjectPropertyScope scope = kAudioObjectPropertyScopeGlobal,
        AudioObjectPropertyElement element = kAudioObjectPropertyElementMain);
    void PutBytes(AudioObjectID object, AudioObjectPropertySelector selector, const void* data, std::size_t bytes,
        AudioObjectPropertyScope scope = kAudioObjectPropertyScopeGlobal,
        AudioObjectPropertyElement element = kAudioObjectPropertyElementMain);
    template<typename Value>
    void Put(AudioObjectID object, AudioObjectPropertySelector selector, const Value& value,
        AudioObjectPropertyScope scope = kAudioObjectPropertyScopeGlobal,
        AudioObjectPropertyElement element = kAudioObjectPropertyElementMain) {
        PutBytes(object, selector, &value, sizeof(value), scope, element);
    }
    template<typename Value>
    Value ValueAt(AudioObjectID object, AudioObjectPropertySelector selector,
        AudioObjectPropertyScope scope = kAudioObjectPropertyScopeGlobal,
        AudioObjectPropertyElement element = kAudioObjectPropertyElementMain) {
        Value value = {};
        const auto& bytes = At(object, selector, scope, element).bytes;
        if (bytes.size() != sizeof(value)) {
            std::abort();
        }
        std::memcpy(&value, bytes.data(), sizeof(value));
        return value;
    }
    void PutCF(AudioObjectID object, AudioObjectPropertySelector selector, CFTypeRef value);
    void FixedMicrophone(UInt32 channels = 1);
    void Boost(const char* name = "Mic Boost", AudioClassID type = kAudioBooleanControlClassID);
    void CheckReleased() const;
};

}
#endif
