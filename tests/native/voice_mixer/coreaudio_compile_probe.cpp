// Compile-only SDK boundary probe. This object is never linked or executed.
#include <CoreAudio/CoreAudio.h>
#include <CoreFoundation/CoreFoundation.h>

#include <cstdint>
#include <type_traits>

static_assert(sizeof(AudioObjectID) == sizeof(std::uint32_t), "HAL object identifiers are uint32, not pointers");
static_assert(sizeof(Float32) == sizeof(float), "hardware scalar volume uses float32");
static_assert(sizeof(AudioObjectPropertyAddress) == 3 * sizeof(std::uint32_t), "typed HAL property address");

void CompileNativeMixerPropertyCalls(AudioObjectID device, AudioObjectID control, float requested) {
    const AudioObjectPropertyAddress volume = {
        kAudioDevicePropertyVolumeScalar, kAudioObjectPropertyScopeInput, kAudioObjectPropertyElementMain
    };
    Boolean settable = false;
    static_cast<void>(AudioObjectHasProperty(device, &volume));
    static_cast<void>(AudioObjectIsPropertySettable(device, &volume, &settable));
    UInt32 bytes = sizeof(requested);
    static_cast<void>(AudioObjectGetPropertyData(device, &volume, 0, nullptr, &bytes, &requested));
    static_cast<void>(AudioObjectSetPropertyData(device, &volume, 0, nullptr, sizeof(requested), &requested));

    const AudioObjectPropertyAddress monitoring = {
        kAudioDevicePropertyPlayThru, kAudioDevicePropertyScopePlayThrough, kAudioObjectPropertyElementMain
    };
    UInt32 enabled = 0;
    bytes = sizeof(enabled);
    static_cast<void>(AudioObjectGetPropertyData(device, &monitoring, 0, nullptr, &bytes, &enabled));
    static_cast<void>(AudioObjectSetPropertyData(device, &monitoring, 0, nullptr, sizeof(enabled), &enabled));

    const AudioObjectPropertyAddress controls = {
        kAudioObjectPropertyControlList, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMain
    };
    static_cast<void>(AudioObjectGetPropertyDataSize(device, &controls, 0, nullptr, &bytes));
    const AudioObjectPropertyAddress booleanValue = {
        kAudioBooleanControlPropertyValue, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMain
    };
    static_cast<void>(AudioObjectHasProperty(control, &booleanValue));

    const AudioObjectPropertyAddress source = {
        kAudioDevicePropertyDataSource, kAudioObjectPropertyScopeInput, kAudioObjectPropertyElementMain
    };
    static_cast<void>(AudioObjectGetPropertyDataSize(device, &source, 0, nullptr, &bytes));
    const AudioObjectPropertyAddress terminal = {
        kAudioStreamPropertyTerminalType, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMain
    };
    static_cast<void>(AudioObjectHasProperty(control, &terminal));
}
