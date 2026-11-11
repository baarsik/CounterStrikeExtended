#include "native_buffer.h"
#include "a2dbuffer.h"
#include "mixer_filters.h"
#include "WallEdge.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <cstdint>
#include <pthread.h>

namespace {
pthread_mutex_t mixMutex = PTHREAD_MUTEX_INITIALIZER;
constexpr DWORD kOutputChannels = 2;
constexpr WORD kRetainedRateSelector = 22;
constexpr DWORD kRetainedSampleRate = 22050;
constexpr DWORD kHrtfCoefficients = 1;
constexpr double kStockGainScale = 49150.5;
constexpr double kStockDelayScale = 22050.0 * 65536.0;
// The retained source stores angular coordinates as float, including the poles.
constexpr float kFullTurn = 6.28318548f;
constexpr float kHalfPi = 1.57079637f;

bool ValidChannel(const a3d_source_channel_t& channel) {
    return std::isfinite(channel.flAzimuth) && std::isfinite(channel.flElevation) &&
        std::fabs(channel.flAzimuth) <= kFullTurn &&
        std::fabs(channel.flElevation) <= kHalfPi &&
        std::isfinite(channel.flGain) &&
        static_cast<double>(channel.flGain) * kStockGainScale >= INT32_MIN &&
        static_cast<double>(channel.flGain) * kStockGainScale <= INT32_MAX &&
        std::isfinite(channel.flDelay);
}
}

A3dNativeMixLock::A3dNativeMixLock() noexcept {
    pthread_mutex_lock(&mixMutex);
}

A3dNativeMixLock::~A3dNativeMixLock() {
    pthread_mutex_unlock(&mixMutex);
}

A3dNativeMixer::~A3dNativeMixer() {
    A3dNativeMixLock lock;
    while (first_) {
        first_->state_ = A3D_BUFFER_STATE_STOPPED;
        first_->Detach();
    }
}

HRESULT A3dNativeVoice::Initialize(BYTE* samples, DWORD bytes, const WAVEFORMATEX& format) {
    const HRESULT status = hrtf_.Initialize(kRetainedSampleRate, kHrtfCoefficients, 0);
    if (FAILED(status)) {
        return status;
    }
    input_.pSamples = samples;
    input_.pCurrent = samples;
    input_.pEnd = samples + bytes;
    input_.cFrames = bytes / format.nBlockAlign;
    input_.dwFlags = (format.wBitsPerSample == 16 ? A3D_MIX_16BIT : 0) |
        (format.nChannels == 2 ? A3D_MIX_STEREO : 0);
    AR2Dinit(&voice_, &input_, kRetainedRateSelector);
    frequency_ = format.nSamplesPerSec;
    AR2DSetFrequency(&voice_, frequency_);
    initialized_ = true;
    return S_OK;
}

HRESULT A3dNativeVoice::SetControl(const A3DCTRL_SRC_SUPER& control) {
    A3dNativeMixLock lock;
    return ApplyControl(control);
}

HRESULT A3dNativeVoice::ApplyControl(const A3DCTRL_SRC_SUPER& control) {
    if (!initialized_) {
        return DSERR_UNINITIALIZED;
    }
    const double frequency = static_cast<double>(frequency_) * control.flPitch;
    const double delay = (static_cast<double>(control.Left.flDelay) - control.Right.flDelay) * kStockDelayScale;
    // Only consumed fields are validated. Stock-unread EQ/reflection/reserved
    // fields are copied verbatim and do not become invented rendering features.
    if (!std::isfinite(control.flPitch) || frequency < 0 || frequency > UINT32_MAX ||
        !ValidChannel(control.Left) || !ValidChannel(control.Right) ||
        !std::isfinite(delay) || delay < INT32_MIN || delay > INT32_MAX) {
        return E_INVALIDARG;
    }
    const HRESULT status = A3dApplyA2DControl(voice_, frequency_, hrtf_, control);
    if (SUCCEEDED(status)) {
        control_ = control;
    }
    return status;
}

bool A3dNativeVoice::CanRender(DWORD frames) const {
    constexpr DWORD kFractionBits = 16;
    constexpr std::int64_t kFractionMask = UINT16_MAX;
    const LONG rateDifference = static_cast<LONG>(
        static_cast<DWORD>(voice_.lRateTarget) - static_cast<DWORD>(voice_.lRateCurrent));
    const LONG rateStep = rateDifference / static_cast<LONG>(frames);
    for (const auto& channel : voice_.channels) {
        const DWORD targetDelay = Mixer_MultiplyFixed16(
            static_cast<DWORD>(voice_.lRateTarget), static_cast<DWORD>(channel.lDelayTarget));
        const LONG delayDifference = static_cast<LONG>(static_cast<DWORD>(channel.lDelayCurrent) - targetDelay);
        const LONG delayStep = delayDifference / static_cast<LONG>(frames);
        const LONG rate = static_cast<LONG>(static_cast<DWORD>(voice_.lRateCurrent) + delayStep);
        const std::int64_t finalRate = static_cast<std::int64_t>(rate) +
            static_cast<std::int64_t>(rateStep) * (frames - 1);
        // Preserve the kernel arithmetic on admitted blocks. Reject the state
        // that would reach its negative-rate debug trap or overflow sample math.
        if (rate < 0 || finalRate < 0 || finalRate > INT32_MAX || channel.iSourceSample < 0) {
            return false;
        }
        const std::int64_t maximumRate = std::max(static_cast<std::int64_t>(rate), finalRate);
        const std::int64_t advance = (maximumRate * frames + kFractionMask) >> kFractionBits;
        // The retained DWORD tracker adds at most16 bits of projected delay.
        const std::int64_t maximumSample = (INT32_MAX >> voice_.cSampleShift) - kFractionMask - advance;
        if (channel.iSourceSample > maximumSample) {
            return false;
        }
    }
    return true;
}

HRESULT A3dNativeMixer::Render(SHORT* stereo, DWORD frames) {
    if (!stereo && frames) {
        return E_POINTER;
    }
    A3dNativeMixLock lock;
    float accumulation[A3D_MIX_MAX_FRAMES * kOutputChannels];
    while (frames) {
        const DWORD count = std::min(frames, static_cast<DWORD>(A3D_MIX_MAX_FRAMES));
        // Validate the entire block before changing any voice or caller output.
        for (auto* buffer = first_; buffer; buffer = buffer->next_) {
            if (buffer->state_ == A3D_BUFFER_STATE_PLAYING && !buffer->samples_->writer &&
                !buffer->voice_.CanRender(count)) {
                return E_INVALIDARG;
            }
        }
        std::memset(accumulation, 0, count * kOutputChannels * sizeof(float));
        for (auto* buffer = first_; buffer; buffer = buffer->next_) {
            if (buffer->state_ != A3D_BUFFER_STATE_PLAYING || buffer->samples_->writer) {
                continue;
            }
            auto& voice = buffer->voice_;
            AR2Dproc(&voice.input_, accumulation, &voice.voice_, count, FALSE);
            if (!buffer->looping_ && voice.input_.pCurrent >= voice.input_.pEnd) {
                buffer->cursor_ = buffer->samples_->bytes;
                buffer->state_ = A3D_BUFFER_STATE_STOPPED;
            } else {
                const auto position = voice.input_.pCurrent - voice.input_.pSamples;
                buffer->cursor_ = static_cast<DWORD>(position) % buffer->samples_->bytes;
            }
        }
        Mixer_FloatToPcm16(accumulation, stereo, count * kOutputChannels);
        stereo += count * kOutputChannels;
        frames -= count;
    }
    return S_OK;
}
