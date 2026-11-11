#include "fixture.h"
#include "a2dbuffer.h"
#include <cmath>
#include <string>
#include <cstddef>

static_assert(sizeof(A3DCTRL_SRC_SUPER) == 1036);
static_assert(offsetof(A3DCTRL_SRC_SUPER, flEq) == 0x10);
static_assert(offsetof(A3DCTRL_SRC_SUPER, flPitch) == 0x14);
static_assert(offsetof(A3DCTRL_SRC_SUPER, Left) == 0x1c);
static_assert(offsetof(A3DCTRL_SRC_SUPER, Right) == 0x2c);
static_assert(offsetof(a3d_source_channel_t, flElevation) == 4);
static_assert(offsetof(a3d_source_channel_t, flGain) == 8);
static_assert(offsetof(a3d_source_channel_t, flDelay) == 12);
static_assert(offsetof(A3DCTRL_SRC_SUPER, Reflections) == 0x4c);

void SameControlState(const a3d_mix_voice_t& actual, const a3d_mix_voice_t& expected) {
    CHECK(actual.lRateRequested == expected.lRateRequested);
    CHECK(actual.lRateTarget == expected.lRateTarget);
    CHECK(actual.dwSampleRate == expected.dwSampleRate);
    for (std::size_t ear = 0; ear < std::size(actual.channels); ++ear) {
        const auto& a = actual.channels[ear];
        const auto& e = expected.channels[ear];
        CHECK(a.lGainBase == e.lGainBase);
        CHECK(a.lDelayTarget == e.lDelayTarget);
        CHECK(a.lGainSpatial == e.lGainSpatial);
        CHECK(a.cFilterTaps == e.cFilterTaps);
        CHECK(std::memcmp(a.coefficients, e.coefficients, sizeof(a.coefficients)) == 0);
        CHECK(std::memcmp(a.targetCoefficients, e.targetCoefficients, sizeof(a.targetCoefficients)) == 0);
    }
}

void ConsumedFields() {
    CHrtfMgr hrtf;
    CHECK(hrtf.Initialize(kSampleRate, 1, 0) == S_OK);
    WAVEFORMATEX format{WAVE_FORMAT_PCM, 1, kSampleRate, kSampleRate * sizeof(SHORT), sizeof(SHORT), 16, 0};
    a3d_platform_buffer_desc_t description{};
    description.dwBufferBytes = kSourceFrames * sizeof(SHORT);
    description.lpwfxFormat = &format;
    LONG references = 1;
    A2DBuffer retained;
    CHECK(retained.Init(&description, &hrtf, &references) == S_OK);
    PcmOwner native;
    constexpr double kGainScale = 49150.5;
    constexpr double kDelayScale = 22050.0 * 65536.0;
    constexpr std::array<float, 5> kAzimuths{-2.5f, -0.5f, 0.0f, 0.75f, 2.5f};
    for (float azimuth : kAzimuths) {
        for (float pitch : {0.5f, 1.0f, 1.5f}) {
            for (float delay : {-0.0002f, 0.0f, 0.0002f}) {
                auto control = DirectControl();
                control.flPitch = pitch;
                control.Left.flAzimuth = azimuth;
                control.Right.flAzimuth = -azimuth;
                control.Left.flElevation = 0.3f;
                control.Right.flElevation = -0.4f;
                control.Left.flDelay = delay;
                control.Right.flDelay = -delay;
                native.Control(control);
                CHECK(retained.SetA3dSuperCtrl(&control, sizeof(control)) == S_OK);
                SameControlState(native.native->voice_.voice_, retained.m_Voice);
                const auto& voice = native.native->voice_.voice_;
                CHECK(voice.channels[0].lGainBase == static_cast<LONG>(control.Left.flGain * kGainScale));
                const LONG expectedDelay = static_cast<LONG>((static_cast<double>(delay) + delay) * kDelayScale);
                CHECK(voice.channels[0].lDelayTarget == (delay >= 0 ? expectedDelay : 0));
                CHECK(voice.channels[1].lDelayTarget == (delay < 0 ? -expectedDelay : 0));
            }
        }
    }
    PcmOwner baseline;
    baseline.Control(DirectControl());
    const auto expected = baseline.Render();
    CHECK(std::any_of(expected.begin(), expected.end(), [](SHORT value) { return value != 0; }));
    for (int field = 0; field < 9; ++field) {
        PcmOwner changed;
        auto control = DirectControl();
        float* fields[] = {&control.flPitch, &control.Left.flAzimuth, &control.Left.flElevation,
            &control.Left.flGain, &control.Left.flDelay, &control.Right.flAzimuth,
            &control.Right.flElevation, &control.Right.flGain, &control.Right.flDelay};
        constexpr float kChanges[] = {0.5f, 1.0f, 0.5f, 0.0f, 0.00002f, -1.0f, -0.5f, 0.0f, -0.00002f};
        *fields[field] = kChanges[field];
        changed.Control(control);
        CHECK(changed.Render() != expected);
    }
}

void Invariant(bool reflections) {
    PcmOwner baseline;
    PcmOwner changed;
    auto control = DirectControl();
    baseline.Control(control);
    if (reflections) {
        for (std::size_t slot = 0; slot < std::size(control.Reflections); ++slot) {
            auto& record = control.Reflections[slot];
            record.dwEnabled = 1;
            record.dwActive = 1;
            record.dwNewSlot = slot % 2;
            record.flEq = 0.875f;
            record.flAudibility = 0.5f;
            record.Left = {0.5f, 0.25f, 0.75f, 0.02f};
            record.Right = {-0.5f, -0.25f, 0.25f, 0.03f};
        }
    } else {
        control.flEq = 0.99f;
    }
    changed.Control(control);
    SameControlState(changed.native->voice_.voice_, baseline.native->voice_.voice_);
    auto actualState = changed.native->voice_.voice_;
    auto expectedState = baseline.native->voice_.voice_;
    // Sample allocations differ; compare every other byte of zero-initialized
    // software voice state, including rate, history, delay and filter state.
    actualState.pSamples = nullptr;
    expectedState.pSamples = nullptr;
    CHECK(std::memcmp(&actualState, &expectedState, sizeof(actualState)) == 0);
    CHECK(changed.Render() == baseline.Render());
}

int main(int argc, char** argv) {
    CHECK(argc == 2);
    const std::string name = argv[1];
    if (name == "consumed_fields_match_retained_state") {
        ConsumedFields();
    } else if (name == "eq_only_is_invariant") {
        Invariant(false);
    } else {
        CHECK(name == "reflections_only_are_invariant");
        Invariant(true);
    }
    std::printf("PASS %s\n", argv[1]);
}
