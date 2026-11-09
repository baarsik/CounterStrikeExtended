// Whole retained implementation with real Windows SDK declarations and fixture-
// owned API bindings. No multimedia DLL or physical property service is loaded.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <mmsystem.h>

#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <type_traits>

namespace {

constexpr DWORD waveDestination = 100;
constexpr DWORD speakerDestination = 200;
constexpr DWORD microphoneLine = 101;
constexpr DWORD lineInputLine = 102;
constexpr DWORD speakerMicrophoneLine = 201;
constexpr DWORD selectorControl = 1000;
constexpr DWORD volumeControl = 10;
constexpr DWORD boostControl = 11;
constexpr DWORD muteControl = 30;
constexpr std::size_t selectionItems = 3;

void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "legacy mixer reference: %s\n", message);
        std::exit(1);
    }
}

struct State {
    unsigned int opens = 0;
    unsigned int closes = 0;
    unsigned int gets = 0;
    unsigned int sets = 0;
    bool failOpen = false;
    bool failCaps = false;
    bool failRead = false;
    bool failWrite = false;
    bool failList = false;
    bool noLines = false;
    bool selector = true;
    bool microphoneItem = true;
    bool sourceMicrophone = true;
    bool mixerSelector = false;
    const char* boostName = "Boost";
    DWORD volume = 32768;
    DWORD boost = 7;
    DWORD mute = 0;
    std::array<DWORD, selectionItems> selected{{1, 0, 1}};
};

State state;

HMIXER Handle() {
    return reinterpret_cast<HMIXER>(&state);
}

void Check(HMIXEROBJ object) {
    Require(object == reinterpret_cast<HMIXEROBJ>(Handle()), "full pointer-width mixer handle round-trips");
}

MMRESULT WINAPI FixtureOpen(LPHMIXER handle, UINT device, DWORD_PTR callback, DWORD_PTR instance, DWORD flags) {
    Require(device == 0 && callback == 0 && instance == 0 && flags == 0, "retained opens mixer0 without callbacks");
    ++state.opens;
    if (state.failOpen) {
        return MMSYSERR_ERROR;
    }
    *handle = Handle();
    return MMSYSERR_NOERROR;
}

MMRESULT WINAPI FixtureClose(HMIXER handle) {
    Require(handle == Handle(), "retained closes its acquired mixer handle");
    ++state.closes;
    return MMSYSERR_NOERROR;
}

MMRESULT WINAPI FixtureCaps(UINT_PTR object, LPMIXERCAPSA caps, UINT bytes) {
    Require(object == reinterpret_cast<UINT_PTR>(Handle()) && bytes == sizeof(*caps),
        "real UINT_PTR device-capabilities parameter preserves native handle");
    if (state.failCaps) {
        return MMSYSERR_ERROR;
    }
    *caps = {};
    caps->cDestinations = state.noLines ? 0 : 2;
    return MMSYSERR_NOERROR;
}

MMRESULT WINAPI FixtureLine(HMIXEROBJ object, LPMIXERLINEA line, DWORD flags) {
    Check(object);
    Require(line->cbStruct == sizeof(*line), "retained supplies exact SDK line extent");
    const DWORD query = flags & MIXER_GETLINEINFOF_QUERYMASK;
    if (query == MIXER_GETLINEINFOF_DESTINATION) {
        const DWORD destination = line->dwDestination;
        Require(destination < 2, "bounded fixture destination index");
        *line = {};
        line->cbStruct = sizeof(*line);
        line->dwLineID = destination == 0 ? waveDestination : speakerDestination;
        line->dwComponentType = destination == 0 ? MIXERLINE_COMPONENTTYPE_DST_WAVEIN
            : MIXERLINE_COMPONENTTYPE_DST_SPEAKERS;
        line->cControls = destination == 0 && state.selector ? 1 : 0;
        line->cConnections = 1;
        return MMSYSERR_NOERROR;
    }
    if (query == MIXER_GETLINEINFOF_SOURCE) {
        const DWORD destination = line->dwDestination;
        Require(destination < 2 && line->dwSource == 0, "bounded fixture source index");
        *line = {};
        line->cbStruct = sizeof(*line);
        line->dwLineID = destination == 0 ? microphoneLine : speakerMicrophoneLine;
        line->dwComponentType = state.sourceMicrophone ? MIXERLINE_COMPONENTTYPE_SRC_MICROPHONE
            : MIXERLINE_COMPONENTTYPE_SRC_LINE;
        line->cControls = destination == 0 ? 2 : 1;
        return MMSYSERR_NOERROR;
    }
    Require(query == MIXER_GETLINEINFOF_LINEID, "only declared SDK line query modes are used");
    const DWORD id = line->dwLineID;
    line->dwComponentType = id == microphoneLine ? MIXERLINE_COMPONENTTYPE_SRC_MICROPHONE
        : MIXERLINE_COMPONENTTYPE_SRC_LINE;
    return MMSYSERR_NOERROR;
}

void Control(MIXERCONTROLA& output, DWORD id, DWORD type, const char* name, DWORD multiple = 0) {
    output = {};
    output.cbStruct = sizeof(output);
    output.dwControlID = id;
    output.dwControlType = type;
    output.cMultipleItems = multiple;
    const std::size_t bytes = std::strlen(name) + 1;
    Require(bytes <= sizeof(output.szShortName), "fixture control name fits SDK storage");
    std::memcpy(output.szShortName, name, bytes);
}

MMRESULT WINAPI FixtureControls(HMIXEROBJ object, LPMIXERLINECONTROLSA controls, DWORD flags) {
    Check(object);
    Require(flags == MIXER_GETLINECONTROLSF_ALL && controls->cbStruct == sizeof(*controls)
            && controls->cbmxctrl == sizeof(MIXERCONTROLA), "real control array dimensions and query mode");
    if (controls->dwLineID == waveDestination) {
        if (controls->cControls == 0) {
            Require(!state.selector, "zero-control wave destination has no selector");
            return MMSYSERR_NOERROR;
        }
        Require(controls->cControls == 1, "fixture selector count");
        Control(controls->pamxctrl[0], selectorControl,
            state.mixerSelector ? MIXERCONTROL_CONTROLTYPE_MIXER : MIXERCONTROL_CONTROLTYPE_MUX,
            "Capture", static_cast<DWORD>(selectionItems));
    } else if (controls->dwLineID == microphoneLine) {
        Require(controls->cControls == 2, "fixture microphone control count");
        Control(controls->pamxctrl[0], volumeControl, MIXERCONTROL_CONTROLTYPE_VOLUME, "Volume");
        Control(controls->pamxctrl[1], boostControl, MIXERCONTROL_CONTROLTYPE_ONOFF, state.boostName);
    } else {
        Require(controls->dwLineID == speakerMicrophoneLine && controls->cControls == 1,
            "fixture monitoring control count");
        Control(controls->pamxctrl[0], muteControl, MIXERCONTROL_CONTROLTYPE_MUTE, "Mute");
    }
    return MMSYSERR_NOERROR;
}

MMRESULT WINAPI FixtureGet(HMIXEROBJ object, LPMIXERCONTROLDETAILS details, DWORD flags) {
    Check(object);
    Require(details->cbStruct == sizeof(*details) && details->cChannels == 1, "retained single-channel query");
    if ((flags & MIXER_GETCONTROLDETAILSF_QUERYMASK) == MIXER_GETCONTROLDETAILSF_LISTTEXT) {
        Require(details->dwControlID == selectorControl && details->cMultipleItems == selectionItems
                && details->cbDetails == sizeof(MIXERCONTROLDETAILS_LISTTEXTA), "selector list query extent");
        if (state.failList) {
            return MMSYSERR_ERROR;
        }
        auto* text = static_cast<MIXERCONTROLDETAILS_LISTTEXTA*>(details->paDetails);
        for (std::size_t index = 0; index < selectionItems; ++index) {
            text[index] = {};
            text[index].dwParam1 = state.microphoneItem && index == 1 ? microphoneLine : lineInputLine;
        }
        return MMSYSERR_NOERROR;
    }
    ++state.gets;
    if (state.failRead) {
        return MMSYSERR_ERROR;
    }
    Require(details->cbDetails == sizeof(DWORD), "value query uses raw DWORD payload");
    auto* values = static_cast<DWORD*>(details->paDetails);
    if (details->dwControlID == selectorControl) {
        Require(details->cMultipleItems == selectionItems, "selector boolean-vector extent");
        std::memcpy(values, state.selected.data(), sizeof(state.selected));
    } else {
        Require(details->cMultipleItems == 0, "ordinary fixture controls use one raw DWORD");
        *values = details->dwControlID == volumeControl ? state.volume
            : details->dwControlID == boostControl ? state.boost : state.mute;
    }
    return MMSYSERR_NOERROR;
}

MMRESULT WINAPI FixtureSet(HMIXEROBJ object, LPMIXERCONTROLDETAILS details, DWORD flags) {
    Check(object);
    Require((flags & MIXER_SETCONTROLDETAILSF_QUERYMASK) == MIXER_SETCONTROLDETAILSF_VALUE
            && details->cbStruct == sizeof(*details) && details->cChannels == 1 && details->cbDetails == sizeof(DWORD),
        "retained value write envelope");
    ++state.sets;
    if (state.failWrite) {
        return MMSYSERR_ERROR;
    }
    const auto* values = static_cast<const DWORD*>(details->paDetails);
    if (details->dwControlID == selectorControl) {
        Require(details->cMultipleItems == selectionItems, "selector write preserves vector extent");
        std::memcpy(state.selected.data(), values, sizeof(state.selected));
    } else {
        Require(details->cMultipleItems == 0, "ordinary fixture write uses one raw DWORD");
        DWORD& destination = details->dwControlID == volumeControl ? state.volume
            : details->dwControlID == boostControl ? state.boost : state.mute;
        destination = *values;
    }
    return MMSYSERR_NOERROR;
}

static_assert(std::is_same<decltype(&FixtureOpen), decltype(&mixerOpen)>::value, "SDK mixerOpen signature");
static_assert(std::is_same<decltype(&FixtureClose), decltype(&mixerClose)>::value, "SDK mixerClose signature");
static_assert(std::is_same<decltype(&FixtureCaps), decltype(&mixerGetDevCapsA)>::value, "SDK pointer-width caps");
static_assert(std::is_same<decltype(&FixtureLine), decltype(&mixerGetLineInfoA)>::value, "SDK line query");
static_assert(std::is_same<decltype(&FixtureControls), decltype(&mixerGetLineControlsA)>::value, "SDK controls query");
static_assert(std::is_same<decltype(&FixtureGet), decltype(&mixerGetControlDetailsA)>::value, "SDK value query");
static_assert(std::is_same<decltype(&FixtureSet), decltype(&mixerSetControlDetails)>::value, "SDK value setter");

}

#define mixerOpen FixtureOpen
#define mixerClose FixtureClose
#define mixerGetDevCapsA FixtureCaps
#define mixerGetLineInfoA FixtureLine
#define mixerGetLineControlsA FixtureControls
#define mixerGetControlDetailsA FixtureGet
#define mixerSetControlDetails FixtureSet
#include CSE_MIXER_SOURCE

int main() {
    constexpr unsigned int failedInitCases = 3;
    for (unsigned int scenario = 0; scenario < failedInitCases; ++scenario) {
        state = {};
        state.failOpen = scenario == 0;
        state.failCaps = scenario == 1;
        state.noLines = scenario == 2;
        IMixerControls* mixer = GetMixerControls();
        Require(mixer != nullptr && state.opens == 1, "factory still returns an object after initialization failure");
        float value = -3.25f;
        Require(!mixer->GetValue_Float(IMixerControls::MicVolume, value) && value == -3.25f
                && !mixer->SetValue_Float(IMixerControls::MicVolume, 0.5f), "missing controls fail without output mutation");
        mixer->Release();
        Require(state.closes == (scenario == 0 ? 0u : 1u), "only an acquired handle is closed, exactly once");
    }
    state = {};
    IMixerControls* mixer = GetMixerControls();
    if (sizeof(void*) > sizeof(std::uint32_t)) {
        Require(reinterpret_cast<std::uintptr_t>(Handle()) > UINT32_MAX, "native handle is actually above4GiB");
    }
    float value = 0;
    Require(mixer->GetValue_Float(IMixerControls::MicVolume, value) && value == 32768.0f / 65535.0f,
        "volume getter is raw DWORD divided by65535");
    for (float input : {0.0f, 0.5f, 1.0f, 0.125f, -1.0f, 1.5f}) {
        Require(mixer->SetValue_Float(IMixerControls::MicVolume, input)
                && state.volume == static_cast<DWORD>(static_cast<__int64>(input * 65535.0f)),
            "retained finite volume conversion truncates then converts to unsigned32 without clamping");
        std::printf("volume,input%.9g,raw%lu\n", static_cast<double>(input), state.volume);
    }
    Require(mixer->GetValue_Float(IMixerControls::MicBoost, value) && value == 1,
        "any nonzero boolean DWORD reads as1");
    Require(mixer->SetValue_Float(IMixerControls::MicBoost, -1) && state.boost == 1
            && mixer->SetValue_Float(IMixerControls::MicBoost, -0.0f) && state.boost == 0,
        "boolean setter uses nonzero, not threshold or positive-only");
    Require(mixer->SetValue_Float(IMixerControls::MicMute, 1) && state.mute == 1,
        "MicMute reaches speaker-side microphone monitoring mute");
    state.failRead = true;
    value = -3.25f;
    Require(!mixer->GetValue_Float(IMixerControls::MicVolume, value) && value == -3.25f,
        "failed property read preserves caller output");
    state.failRead = false;
    state.failWrite = true;
    Require(!mixer->SetValue_Float(IMixerControls::MicVolume, 0.5f), "OS write failure propagates");
    state.failWrite = false;
    Require(!mixer->GetValue_Float(IMixerControls::NumControls, value)
            && !mixer->SetValue_Float(IMixerControls::NumControls, 0.5f), "invalid control never reaches OS facade");
    Require(mixer->SelectMicrophoneForWaveInput() && state.selected == std::array<DWORD, 3>{{0, 1, 0}},
        "MUX selection clears other sources and selects the actual microphone item");
    state.failRead = true;
    const unsigned int priorSets = state.sets;
    Require(!mixer->SelectMicrophoneForWaveInput() && state.sets == priorSets,
        "failed selector read prevents a property write");
    state.failRead = false;
    state.failWrite = true;
    Require(!mixer->SelectMicrophoneForWaveInput(), "selector OS write failure propagates");
    state.failWrite = false;
    mixer->Release();
    Require(state.closes == 1, "successful mixer owns one close");
    state = {};
    state.mixerSelector = true;
    mixer = GetMixerControls();
    Require(mixer->SelectMicrophoneForWaveInput() && state.selected == std::array<DWORD, 3>{{1, 1, 1}},
        "MIXER selection preserves other selected inputs");
    mixer->Release();
    for (const char* name : {"Gain", "+20dB", "boost"}) {
        state = {};
        state.boostName = name;
        mixer = GetMixerControls();
        value = -3.25f;
        const bool expected = std::strcmp(name, "boost") != 0;
        Require(mixer->GetValue_Float(IMixerControls::MicBoost, value) == expected,
            "boost discovery uses the actual case-sensitive Gain/Boost/+20d substring rule");
        mixer->Release();
    }
    state = {};
    state.sourceMicrophone = false;
    mixer = GetMixerControls();
    Require(!mixer->GetValue_Float(IMixerControls::MicVolume, value)
            && !mixer->GetValue_Float(IMixerControls::MicMute, value), "non-microphone sources supply no mic controls");
    mixer->Release();
    state = {};
    state.selector = false;
    mixer = GetMixerControls();
    Require(!mixer->SelectMicrophoneForWaveInput(), "absent selector returns false");
    mixer->Release();
    for (bool failedList : {false, true}) {
        state = {};
        state.microphoneItem = false;
        state.failList = failedList;
        mixer = GetMixerControls();
        Require(mixer->SelectMicrophoneForWaveInput() && state.selected == std::array<DWORD, 3>{{1, 0, 0}},
            "legacy failed/missing microphone lookup retains control-index fallback and selects a non-mic item");
        std::printf("legacy_selection_fallback,failed_list%d,wrong_source_selected1\n", failedList ? 1 : 0);
        mixer->Release();
    }
    std::printf("legacy mixer reference passed; pointer_bytes%zu; handle_high%d; no multimedia provider executed\n",
        sizeof(void*), reinterpret_cast<std::uintptr_t>(Handle()) > UINT32_MAX ? 1 : 0);
    return 0;
}
