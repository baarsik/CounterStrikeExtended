#include "fake_coreaudio.h"
#include "voice_mixer_controls.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>

namespace {

using fake_mixer::device;
using fake_mixer::stream;

void Require(bool value, const char* message) {
    if (!value) {
        std::fprintf(stderr, "CoreAudio mixer: %s\n", message);
        std::exit(1);
    }
}

void Factory() {
    fake_mixer::Store owner;
    auto services = owner.Services();
    IMixerControls* mixer = cse::CreateCoreAudioMixerControls(services);
    Require(mixer != nullptr, "valid private services must create an owned mixer object");
    float unchanged = -7;
    Require(!mixer->GetValue_Float(IMixerControls::NumControls, unchanged) && unchanged == -7
            && !mixer->SetValue_Float(IMixerControls::NumControls, 1) && owner.propertyCalls == 0,
        "invalid control is rejected before any property query or caller-output mutation");
    services = {};
    mixer->Release();
    Require(owner.allocationCalls == 1 && owner.frees == 1 && owner.propertyCalls == 0,
        "object copies services and releases once without querying a device at construction/destruction");
    owner.failAllocation = owner.allocationCalls + 1;
    Require(cse::CreateCoreAudioMixerControls(owner.Services()) == nullptr, "object allocation failure returns null");
    Require(cse::CreateCoreAudioMixerControls(services) == nullptr, "invalid services are rejected before allocation");
}

void Volume() {
    fake_mixer::Store owner;
    owner.FixedMicrophone();
    IMixerControls* mixer = cse::CreateCoreAudioMixerControls(owner.Services());
    float value = -7.0f;
    Require(mixer->GetValue_Float(IMixerControls::MicVolume, value) && value == 0.625f,
        "identified microphone reads actual input main Float32 scalar");
    Require(mixer->SetValue_Float(IMixerControls::MicVolume, 0.5f)
            && owner.ValueAt<Float32>(device, kAudioDevicePropertyVolumeScalar, kAudioObjectPropertyScopeInput) == 0.5f,
        "normalized setter writes exact Float32 to input main without WinMM quantization");
    const unsigned int writes = owner.writes;
    for (float invalid : {-1.0f, 1.01f, std::numeric_limits<float>::quiet_NaN(),
            std::numeric_limits<float>::infinity()}) {
        Require(!mixer->SetValue_Float(IMixerControls::MicVolume, invalid) && owner.writes == writes,
            "invalid normalized scalar cannot write a hardware property");
    }
    auto& main = owner.At(device, kAudioDevicePropertyVolumeScalar, kAudioObjectPropertyScopeInput);
    main.writable = false;
    owner.Put(device, kAudioDevicePropertyVolumeScalar, 0.2f, kAudioObjectPropertyScopeInput, 1);
    Require(!mixer->SetValue_Float(IMixerControls::MicVolume, 0.25f) && owner.writes == writes,
        "readonly main cannot be bypassed through a channel scalar");
    main.writable = true;
    main.readStatus = kAudioHardwareBadDeviceError;
    value = -7;
    Require(!mixer->GetValue_Float(IMixerControls::MicVolume, value) && value == -7,
        "failed main read preserves output and cannot fall back to a channel");
    owner.Erase(device, kAudioDevicePropertyVolumeScalar, kAudioObjectPropertyScopeInput);
    Require(mixer->GetValue_Float(IMixerControls::MicVolume, value) && value == 0.2f,
        "absent main may use the positively bound sole microphone channel");
    Require(mixer->SetValue_Float(IMixerControls::MicVolume, 0.75f)
            && owner.ValueAt<Float32>(device, kAudioDevicePropertyVolumeScalar, kAudioObjectPropertyScopeInput, 1)
                == 0.75f, "sole-channel fallback writes only that actual channel");
    mixer->Release();
    owner.CheckReleased();
}

void MonitoringAndBoost() {
    fake_mixer::Store owner;
    owner.FixedMicrophone();
    owner.Put(device, kAudioDevicePropertyPlayThru, static_cast<UInt32>(1), kAudioDevicePropertyScopePlayThrough);
    owner.Put(device, kAudioDevicePropertyMute, static_cast<UInt32>(0), kAudioObjectPropertyScopeInput);
    owner.Put(device, kAudioDevicePropertyMute, static_cast<UInt32>(0), kAudioObjectPropertyScopeOutput);
    owner.Boost();
    IMixerControls* mixer = cse::CreateCoreAudioMixerControls(owner.Services());
    float value = -7;
    Require(mixer->GetValue_Float(IMixerControls::MicMute, value) && value == 0,
        "enabled hardware play-through means microphone monitoring is not muted");
    Require(mixer->SetValue_Float(IMixerControls::MicMute, -1)
            && owner.ValueAt<UInt32>(device, kAudioDevicePropertyPlayThru, kAudioDevicePropertyScopePlayThrough) == 0,
        "monitor mute writes inverse zero/nonzero value in play-through scope");
    Require(owner.ValueAt<UInt32>(device, kAudioDevicePropertyMute, kAudioObjectPropertyScopeInput) == 0
            && owner.ValueAt<UInt32>(device, kAudioDevicePropertyMute, kAudioObjectPropertyScopeOutput) == 0,
        "monitor control does not change capture mute or output master mute");
    Require(mixer->GetValue_Float(IMixerControls::MicBoost, value) && value == 1,
        "explicit input BooleanControl boost exposes its real hardware value");
    Require(mixer->SetValue_Float(IMixerControls::MicBoost, -0.0f)
            && owner.ValueAt<UInt32>(fake_mixer::boost, kAudioBooleanControlPropertyValue) == 0,
        "boost setter retains boolean zero/nonzero rule");
    Require(mixer->SetValue_Float(IMixerControls::MicBoost, std::numeric_limits<float>::quiet_NaN())
            && owner.ValueAt<UInt32>(fake_mixer::boost, kAudioBooleanControlPropertyValue) == 1,
        "boolean nonzero comparison preserves the defined NaN case without a numeric cast");
    owner.At(fake_mixer::boost, kAudioBooleanControlPropertyValue).writable = false;
    const unsigned int beforeReadonly = owner.writes;
    Require(mixer->GetValue_Float(IMixerControls::MicBoost, value) && value == 1
            && !mixer->SetValue_Float(IMixerControls::MicBoost, 0) && owner.writes == beforeReadonly,
        "readonly explicit boost remains readable and cannot authorize a write");
    owner.At(fake_mixer::boost, kAudioBooleanControlPropertyValue).writable = true;
    Require(owner.cfCopies != 0 && owner.cfCopies == owner.cfReleases,
        "temporary property CFStrings are released within every completed call");
    const unsigned int writes = owner.writes;
    owner.Boost("boost");
    Require(!mixer->GetValue_Float(IMixerControls::MicBoost, value)
            && !mixer->SetValue_Float(IMixerControls::MicBoost, 1) && owner.writes == writes,
        "lowercase name outside retained case-sensitive boost heuristic is unsupported");
    owner.Boost("Gain", kAudioPhantomPowerControlClassID);
    Require(!mixer->SetValue_Float(IMixerControls::MicBoost, 1) && owner.writes == writes,
        "standard phantom-power control must never be mistaken for microphone boost");
    owner.Boost("+20dB", static_cast<AudioClassID>(0x42535431u));
    Require(mixer->SetValue_Float(IMixerControls::MicBoost, -1),
        "explicit custom BooleanControl with known base class and boost name is supported");
    mixer->Release();
    owner.CheckReleased();
}

void Binding() {
    for (unsigned int scenario = 0; scenario < 8; ++scenario) {
        fake_mixer::Store owner;
        owner.FixedMicrophone();
        if (scenario == 0) {
            owner.Put(stream, kAudioStreamPropertyTerminalType, static_cast<UInt32>(kAudioStreamTerminalTypeLine));
        } else if (scenario == 1) {
            owner.Put(device, kAudioDevicePropertyDeviceIsAlive, static_cast<UInt32>(0));
        } else if (scenario == 2) {
            owner.Put(stream, kAudioStreamPropertyDirection, static_cast<UInt32>(0));
        } else if (scenario == 3) {
            owner.Put(stream, kAudioStreamPropertyStartingChannel, static_cast<UInt32>(2));
        } else if (scenario == 4) {
            const UInt32 choices[] = {1, 2};
            owner.PutBytes(device, kAudioDevicePropertyDataSources, choices, sizeof(choices),
                kAudioObjectPropertyScopeInput);
            owner.Put(device, kAudioDevicePropertyDataSource, choices[0], kAudioObjectPropertyScopeInput);
        } else if (scenario == 5) {
            owner.At(stream, kAudioStreamPropertyVirtualFormat).wrongReturnedSize = true;
        } else if (scenario == 6) {
            owner.Put(device, kAudioDevicePropertyDataSources, static_cast<UInt32>(1),
                kAudioObjectPropertyScopeInput);
        } else {
            owner.Put(kAudioObjectSystemObject, kAudioHardwarePropertyDefaultInputDevice,
                static_cast<AudioObjectID>(kAudioObjectUnknown));
        }
        IMixerControls* mixer = cse::CreateCoreAudioMixerControls(owner.Services());
        float value = -7;
        Require(!mixer->GetValue_Float(IMixerControls::MicVolume, value) && value == -7
                && !mixer->SetValue_Float(IMixerControls::MicVolume, 0.5f)
                && !mixer->SelectMicrophoneForWaveInput() && owner.writes == 0,
            "unproven/dead/malformed/ambiguous microphone routes cannot expose or modify a hardware control");
        mixer->Release();
        owner.CheckReleased();
    }
    fake_mixer::Store owner;
    owner.FixedMicrophone(2);
    owner.Erase(device, kAudioDevicePropertyVolumeScalar, kAudioObjectPropertyScopeInput);
    owner.Put(device, kAudioDevicePropertyVolumeScalar, 0.25f, kAudioObjectPropertyScopeInput, 1);
    owner.Put(device, kAudioDevicePropertyVolumeScalar, 0.75f, kAudioObjectPropertyScopeInput, 2);
    IMixerControls* mixer = cse::CreateCoreAudioMixerControls(owner.Services());
    Require(!mixer->SetValue_Float(IMixerControls::MicVolume, 0.5f) && owner.writes == 0,
        "multiple channels without a main scalar do not authorize averaging or all-channel writes");
    mixer->Release();
}

void SingletonSelection() {
    fake_mixer::Store owner;
    owner.FixedMicrophone();
    IMixerControls* mixer = cse::CreateCoreAudioMixerControls(owner.Services());
    Require(!mixer->SelectMicrophoneForWaveInput() && owner.writes == 0,
        "fixed microphone endpoint with no selector reports unsupported selection");
    constexpr UInt32 source = 91;
    owner.Put(device, kAudioDevicePropertyDataSources, source, kAudioObjectPropertyScopeInput);
    owner.Put(device, kAudioDevicePropertyDataSource, source, kAudioObjectPropertyScopeInput);
    Require(mixer->SelectMicrophoneForWaveInput() && owner.writes == 1
            && owner.ValueAt<UInt32>(device, kAudioDevicePropertyDataSource, kAudioObjectPropertyScopeInput) == source,
        "sole source/current item plus identified mic endpoint is unambiguous and uses its actual ID");
    owner.At(device, kAudioDevicePropertyDataSource, kAudioObjectPropertyScopeInput).writable = false;
    Require(!mixer->SelectMicrophoneForWaveInput() && owner.writes == 1,
        "readonly source selection propagates failure without a write");
    mixer->Release();
}

void Failures() {
    unsigned int successfulAllocations = 0;
    {
        fake_mixer::Store owner;
        owner.FixedMicrophone();
        owner.Boost();
        IMixerControls* mixer = cse::CreateCoreAudioMixerControls(owner.Services());
        float value = -7;
        Require(mixer->GetValue_Float(IMixerControls::MicBoost, value), "baseline boost owns all temporary reads");
        successfulAllocations = owner.allocationCalls;
        mixer->Release();
    }
    for (unsigned int allocation = 1; allocation <= successfulAllocations; ++allocation) {
        fake_mixer::Store owner;
        owner.FixedMicrophone();
        owner.Boost();
        owner.failAllocation = allocation;
        IMixerControls* mixer = cse::CreateCoreAudioMixerControls(owner.Services());
        if (allocation == 1) {
            Require(mixer == nullptr && owner.propertyCalls == 0, "object OOM fails before property service access");
        } else {
            Require(mixer != nullptr, "temporary allocation fault is later than object creation");
            float value = -7;
            Require(!mixer->GetValue_Float(IMixerControls::MicBoost, value) && value == -7 && owner.writes == 0,
                "every temporary-buffer OOM preserves caller output and cannot write hardware");
            mixer->Release();
        }
        owner.CheckReleased();
    }
    for (unsigned int scenario = 0; scenario < 5; ++scenario) {
        fake_mixer::Store owner;
        owner.FixedMicrophone();
        owner.Boost();
        auto& name = owner.At(fake_mixer::boost, kAudioObjectPropertyName);
        if (scenario == 0) {
            name.wrongReturnedSize = true;
        } else if (scenario == 1) {
            const int number = 42;
            CFNumberRef object = CFNumberCreate(nullptr, kCFNumberIntType, &number);
            owner.PutCF(fake_mixer::boost, kAudioObjectPropertyName, object);
            CFRelease(object);
        } else if (scenario == 2) {
            const CFTypeRef nullObject = nullptr;
            owner.Put(fake_mixer::boost, kAudioObjectPropertyName, nullObject);
        } else if (scenario == 3) {
            name.readStatus = kAudioHardwareBadDeviceError;
        } else {
            name.overrideSize = true;
            name.reportedSize = 1;
        }
        IMixerControls* mixer = cse::CreateCoreAudioMixerControls(owner.Services());
        float value = -7;
        Require(!mixer->GetValue_Float(IMixerControls::MicBoost, value) && value == -7
                && !mixer->SetValue_Float(IMixerControls::MicBoost, 1) && owner.writes == 0,
            "malformed/failed CF property metadata never exposes a boost or changes hardware");
        mixer->Release();
        owner.CheckReleased();
    }
    for (unsigned int scenario = 0; scenario < 5; ++scenario) {
        fake_mixer::Store owner;
        owner.FixedMicrophone();
        auto& volume = owner.At(device, kAudioDevicePropertyVolumeScalar, kAudioObjectPropertyScopeInput);
        if (scenario == 0) {
            volume.settableStatus = kAudioHardwareBadDeviceError;
        } else if (scenario == 1) {
            volume.writeStatus = kAudioHardwareBadDeviceError;
        } else if (scenario == 2) {
            volume.wrongReturnedSize = true;
        } else if (scenario == 3) {
            owner.Put(device, kAudioDevicePropertyVolumeScalar, std::numeric_limits<float>::quiet_NaN(),
                kAudioObjectPropertyScopeInput);
        } else {
            volume.overrideSize = true;
            volume.reportedSize = 1;
        }
        IMixerControls* mixer = cse::CreateCoreAudioMixerControls(owner.Services());
        if (scenario == 4) {
            Require(!mixer->SetValue_Float(IMixerControls::MicVolume, 0.5f) && owner.writes == 0,
                "malformed scalar property width cannot authorize any hardware write");
        } else if (scenario < 2) {
            Require(!mixer->SetValue_Float(IMixerControls::MicVolume, 0.5f), "real Settable/Set failure propagates");
        } else {
            float value = -7;
            Require(!mixer->GetValue_Float(IMixerControls::MicVolume, value) && value == -7,
                "malformed scalar result cannot replace caller output");
        }
        mixer->Release();
    }
}

void Topology() {
    for (unsigned int faults : {1u, 2u}) {
        fake_mixer::Store owner;
        owner.FixedMicrophone();
        auto& streams = owner.At(device, kAudioDevicePropertyStreams, kAudioObjectPropertyScopeInput);
        streams.changingReads = faults;
        IMixerControls* mixer = cse::CreateCoreAudioMixerControls(owner.Services());
        float value = -7;
        const bool result = mixer->GetValue_Float(IMixerControls::MicVolume, value);
        Require(result == (faults == 1) && (result ? value == 0.625f : value == -7),
            "one property-size race can rediscover; repeated races fail without committing output");
        if (faults == 2) {
            Require(streams.gets == 2 && streams.sizes == 2, "property rediscovery has a strict two-attempt bound");
        }
        mixer->Release();
    }
    fake_mixer::Store owner;
    owner.FixedMicrophone();
    IMixerControls* mixer = cse::CreateCoreAudioMixerControls(owner.Services());
    owner.onSettable = [&owner](const fake_mixer::Key&) {
        owner.Put(kAudioObjectSystemObject, kAudioHardwarePropertyDefaultInputDevice,
            static_cast<AudioObjectID>(device + 1));
    };
    Require(!mixer->SetValue_Float(IMixerControls::MicVolume, 0.5f) && owner.writes == 0,
        "endpoint changes after Settable cannot authorize a stale hardware write");
    owner.Put(kAudioObjectSystemObject, kAudioHardwarePropertyDefaultInputDevice, device);
    owner.onGet = [&owner](const fake_mixer::Key& key) {
        if (key.address.mSelector == kAudioDevicePropertyVolumeScalar) {
            owner.Put(stream, kAudioStreamPropertyTerminalType, static_cast<UInt32>(kAudioStreamTerminalTypeLine));
        }
    };
    float value = -7;
    Require(!mixer->GetValue_Float(IMixerControls::MicVolume, value) && value == -7,
        "route changes after reading a scalar cannot expose it as a microphone value");
    owner.onGet = {};
    owner.Put(stream, kAudioStreamPropertyTerminalType, static_cast<UInt32>(kAudioStreamTerminalTypeMicrophone));
    auto& streams = owner.At(device, kAudioDevicePropertyStreams, kAudioObjectPropertyScopeInput);
    streams.overrideSize = true;
    streams.reportedSize = (std::numeric_limits<UInt32>::max)() - 3;
    Require(!mixer->SetValue_Float(IMixerControls::MicVolume, 0.5f) && owner.writes == 0
            && owner.largestRequest == streams.reportedSize,
        "large property counts are checked and allocation failure prevents a hardware write");
    mixer->Release();
}

void Owners() {
    fake_mixer::Store first;
    fake_mixer::Store second;
    first.FixedMicrophone();
    second.FixedMicrophone();
    first.Boost();
    second.Boost();
    auto services = first.Services();
    IMixerControls* a = cse::CreateCoreAudioMixerControls(services);
    services = second.Services();
    IMixerControls* b = cse::CreateCoreAudioMixerControls(services);
    services = {};
    if (sizeof(void*) > sizeof(UInt32)) {
        Require(reinterpret_cast<std::uintptr_t>(a) > (std::numeric_limits<UInt32>::max)()
                && reinterpret_cast<std::uintptr_t>(&first) > (std::numeric_limits<UInt32>::max)(),
            "actual object and per-owner context addresses exceed4GiB");
    }
    Require(a->SetValue_Float(IMixerControls::MicVolume, 0.25f)
            && b->SetValue_Float(IMixerControls::MicVolume, 0.75f), "independent instances operate on copied services");
    a->Release();
    first.CheckReleased();
    for (unsigned int index = 0; index < 64; ++index) {
        float value = -7;
        Require(b->GetValue_Float(IMixerControls::MicVolume, value) && value == 0.75f
                && b->GetValue_Float(IMixerControls::MicBoost, value),
            "remaining owner stays usable after another releases and repeated CF-backed queries");
        Require(second.cfCopies == second.cfReleases && second.allocations.size() == 1,
            "every completed call retains only its owning object, not temporary buffers or CF objects");
    }
    b->Release();
    second.CheckReleased();
}

}

int main(int argc, char** argv) {
    if (argc != 2) {
        return 2;
    }
    if (std::strcmp(argv[1], "factory") == 0) {
        Factory();
    } else if (std::strcmp(argv[1], "volume") == 0) {
        Volume();
    } else if (std::strcmp(argv[1], "monitor-boost") == 0) {
        MonitoringAndBoost();
    } else if (std::strcmp(argv[1], "binding") == 0) {
        Binding();
    } else if (std::strcmp(argv[1], "selection") == 0) {
        SingletonSelection();
    } else if (std::strcmp(argv[1], "failures") == 0) {
        Failures();
    } else if (std::strcmp(argv[1], "topology") == 0) {
        Topology();
    } else if (std::strcmp(argv[1], "owners") == 0) {
        Owners();
    } else {
        return 2;
    }
    std::puts("CoreAudio mixer component passed");
    return 0;
}
