// SDK reference probe only. Objects are never linked or executed.
#include <CoreAudio/CoreAudio.h>
#include <AudioToolbox/AudioHardwareService.h>

#include <cstdint>
#include <type_traits>

static_assert(std::is_same<decltype(AudioStreamBasicDescription{}.mChannelsPerFrame), UInt32>::value,
    "stream channel count is an unsigned32 property field");
static_assert(sizeof(AudioObjectPropertyElement) == sizeof(std::uint32_t), "device channel element width");
constexpr AudioObjectPropertySelector virtualMain = kAudioHardwareServiceDeviceProperty_VirtualMainVolume;
static_assert(virtualMain != kAudioDevicePropertyVolumeScalar, "AHS virtual main is a distinct property");

void CompileSoleMicrophoneChannelCalls(AudioObjectID device, AudioObjectID stream, UInt32 channel, Float32 value) {
    const AudioObjectPropertyAddress startingChannel = {
        kAudioStreamPropertyStartingChannel, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMain
    };
    UInt32 bytes = sizeof(channel);
    static_cast<void>(AudioObjectGetPropertyData(stream, &startingChannel, 0, nullptr, &bytes, &channel));
    const AudioObjectPropertyAddress format = {
        kAudioStreamPropertyVirtualFormat, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMain
    };
    AudioStreamBasicDescription description = {};
    bytes = sizeof(description);
    static_cast<void>(AudioObjectGetPropertyData(stream, &format, 0, nullptr, &bytes, &description));
    const AudioObjectPropertyAddress volume = {
        kAudioDevicePropertyVolumeScalar, kAudioObjectPropertyScopeInput, channel
    };
    static_cast<void>(AudioObjectHasProperty(device, &volume));
    Boolean settable = false;
    static_cast<void>(AudioObjectIsPropertySettable(device, &volume, &settable));
    bytes = sizeof(value);
    static_cast<void>(AudioObjectGetPropertyData(device, &volume, 0, nullptr, &bytes, &value));
    static_cast<void>(AudioObjectSetPropertyData(device, &volume, 0, nullptr, sizeof(value), &value));
}

#if defined(CSE_AHS_DEPRECATED_CALLS)
void CompileDeprecatedVirtualMainCalls(AudioObjectID ahsDevice, Float32 value) {
    const AudioObjectPropertyAddress volume = {
        virtualMain, kAudioObjectPropertyScopeInput, kAudioObjectPropertyElementMain
    };
    static_cast<void>(AudioHardwareServiceHasProperty(ahsDevice, &volume));
    Boolean settable = false;
    static_cast<void>(AudioHardwareServiceIsPropertySettable(ahsDevice, &volume, &settable));
    UInt32 bytes = sizeof(value);
    static_cast<void>(AudioHardwareServiceGetPropertyData(ahsDevice, &volume, 0, nullptr, &bytes, &value));
    static_cast<void>(AudioHardwareServiceSetPropertyData(ahsDevice, &volume, 0, nullptr, sizeof(value), &value));
}
#endif
