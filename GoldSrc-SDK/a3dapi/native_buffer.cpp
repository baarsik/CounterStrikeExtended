#include "native_buffer.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <new>

namespace {
constexpr DWORD kFullConeDegrees = 360;
constexpr float kDefaultMaximumDistance = 1000000000.0f;
constexpr BYTE kUnsignedPcmSilence = 128;
constexpr DWORD kSupportedBufferFlags = DSBCAPS_STATIC | DSBCAPS_LOCHARDWARE | DSBCAPS_LOCSOFTWARE |
    DSBCAPS_CTRL3D | DSBCAPS_CTRLFREQUENCY | DSBCAPS_CTRLPAN | DSBCAPS_CTRLVOLUME |
    DSBCAPS_GETCURRENTPOSITION2 | DSBCAPS_GLOBALFOCUS;
// The retained 8-bit mono boundary reader consumes the byte after the logical
// final frame. It is owned silence, never caller padding or unrelated memory.
constexpr std::size_t kBoundaryBytes = 1;

template<class T> HRESULT Read(const T& value, T* output) {
    if (!output) {
        return E_POINTER;
    }
    A3dNativeMixLock lock;
    *output = value;
    return S_OK;
}

bool ValidFormat(LPCWAVEFORMATEX format, DWORD bytes) {
    if (!format || format->wFormatTag != WAVE_FORMAT_PCM || format->cbSize ||
        (format->nChannels != 1 && format->nChannels != 2) ||
        (format->wBitsPerSample != 8 && format->wBitsPerSample != 16) ||
        !format->nSamplesPerSec ||
        !bytes || bytes > INT32_MAX) {
        return false;
    }
    const DWORD align = format->nChannels * format->wBitsPerSample / 8;
    return format->nBlockAlign == align && bytes % align == 0 &&
        static_cast<std::uint64_t>(format->nAvgBytesPerSec) ==
        static_cast<std::uint64_t>(format->nSamplesPerSec) * align;
}

bool ValidVector(const D3DVECTOR& vector) {
    return std::isfinite(vector.x) && std::isfinite(vector.y) && std::isfinite(vector.z);
}

HRESULT Immediate(DWORD apply) {
    return apply == DS3D_IMMEDIATE ? S_OK : apply == DS3D_DEFERRED ? E_NOTIMPL : E_INVALIDARG;
}
}

void A3dNativeBuffer::Attach(A3dNativeMixer& mixer) {
    mixer_ = &mixer;
    previous_ = mixer.last_;
    if (previous_) {
        previous_->next_ = this;
    } else {
        mixer.first_ = this;
    }
    mixer.last_ = this;
}

void A3dNativeBuffer::Detach() {
    if (mixer_) {
        if (previous_) {
            previous_->next_ = next_;
        } else {
            mixer_->first_ = next_;
        }
        if (next_) {
            next_->previous_ = previous_;
        } else {
            mixer_->last_ = previous_;
        }
    }
    mixer_ = nullptr;
    next_ = nullptr;
    previous_ = nullptr;
}

A3dNativeBuffer::~A3dNativeBuffer() {
    if (samples_) {
        if (samples_->writer == this) {
            samples_->writer = nullptr;
        }
        if (!--samples_->references) {
            std::free(samples_->data);
            delete samples_;
        }
    }
}

HRESULT A3dNativeMixer::CreateBuffer(LPCDSBUFFERDESC description, IDirectSoundBuffer** buffer) {
    if (!buffer) {
        return E_POINTER;
    }
    *buffer = nullptr;
    if (!description || (description->dwSize != sizeof(DSBUFFERDESC) &&
        description->dwSize != sizeof(DSBUFFERDESC1)) ||
        !ValidFormat(description->lpwfxFormat, description->dwBufferBytes)) {
        return E_INVALIDARG;
    }
    if (description->dwFlags & ~kSupportedBufferFlags) {
        return E_NOTIMPL;
    }
    auto* owner = new (std::nothrow) A3dNativeBuffer;
    if (!owner) {
        return E_OUTOFMEMORY;
    }
    owner->samples_ = new (std::nothrow) A3dNativeSamples;
    if (!owner->samples_) {
        delete owner;
        return E_OUTOFMEMORY;
    }
    owner->samples_->bytes = description->dwBufferBytes;
    owner->samples_->data = static_cast<BYTE*>(std::malloc(
        static_cast<std::size_t>(description->dwBufferBytes) + kBoundaryBytes));
    if (!owner->samples_->data) {
        delete owner;
        return E_OUTOFMEMORY;
    }
    owner->format_ = *description->lpwfxFormat;
    owner->flags_ = (description->dwFlags & ~DSBCAPS_LOCHARDWARE) | DSBCAPS_LOCSOFTWARE;
    std::memset(owner->samples_->data, owner->format_.wBitsPerSample == 8 ? kUnsignedPcmSilence : 0,
        static_cast<std::size_t>(owner->samples_->bytes) + kBoundaryBytes);
    const HRESULT status = owner->voice_.Initialize(owner->samples_->data, owner->samples_->bytes, owner->format_);
    if (FAILED(status)) {
        delete owner;
        return status;
    }
    owner->spatial_.dwSize = sizeof(DS3DBUFFER);
    owner->spatial_.dwInsideConeAngle = kFullConeDegrees;
    owner->spatial_.dwOutsideConeAngle = kFullConeDegrees;
    owner->spatial_.vConeOrientation.z = 1;
    owner->spatial_.flMinDistance = 1;
    owner->spatial_.flMaxDistance = kDefaultMaximumDistance;
    A3dNativeMixLock lock;
    owner->Attach(*this);
    *buffer = owner;
    return S_OK;
}

HRESULT A3dNativeMixer::DuplicateBuffer(IDirectSoundBuffer* source, IDirectSoundBuffer** output) {
    if (!output) {
        return E_POINTER;
    }
    *output = nullptr;
    A3dNativeMixLock lock;
    A3dNativeBuffer* original = first_;
    while (original && static_cast<IDirectSoundBuffer*>(original) != source) {
        original = original->next_;
    }
    if (!original) {
        return E_INVALIDARG;
    }
    auto* duplicate = new (std::nothrow) A3dNativeBuffer;
    if (!duplicate) {
        return E_OUTOFMEMORY;
    }
    duplicate->samples_ = original->samples_;
    ++duplicate->samples_->references;
    duplicate->format_ = original->format_;
    duplicate->spatial_ = original->spatial_;
    duplicate->flags_ = original->flags_;
    duplicate->volume_ = original->volume_;
    duplicate->pan_ = original->pan_;
    HRESULT status = duplicate->voice_.Initialize(
        duplicate->samples_->data, duplicate->samples_->bytes, duplicate->format_);
    if (SUCCEEDED(status)) {
        duplicate->voice_.frequency_ = original->voice_.frequency_;
        status = duplicate->voice_.ApplyControl(original->voice_.control_);
    }
    if (FAILED(status)) {
        delete duplicate;
        return status;
    }
    duplicate->Attach(*this);
    *output = duplicate;
    return S_OK;
}

HRESULT A3dNativeBuffer::QueryInterface(REFIID iid, void** object) {
    if (!object) {
        return E_POINTER;
    }
    *object = nullptr;
    if (IsEqualIID(iid, IID_IUnknown) || IsEqualIID(iid, IID_IDirectSoundBuffer)) {
        *object = static_cast<IDirectSoundBuffer*>(this);
    } else if (IsEqualIID(iid, IID_IDirectSound3DBuffer)) {
        *object = static_cast<IDirectSound3DBuffer*>(this);
    } else if (IsEqualIID(iid, IID_A3dNativeRenderBuffer)) {
        *object = static_cast<IA3dDalBuffer*>(this);
    } else if (IsEqualIID(iid, IID_A3dNativeBufferControl)) {
        *object = static_cast<IResManBuffer*>(this);
    } else {
        return E_NOINTERFACE;
    }
    AddRef();
    return S_OK;
}

ULONG A3dNativeBuffer::AddRef() {
    A3dNativeMixLock lock;
    return ++references_;
}

ULONG A3dNativeBuffer::Release() {
    A3dNativeMixLock lock;
    const ULONG remaining = --references_;
    if (!remaining) {
        Detach();
        delete this;
    }
    return remaining;
}

HRESULT A3dNativeBuffer::GetCaps(LPDSBCAPS caps) {
    if (!caps || caps->dwSize != sizeof(*caps)) {
        return E_INVALIDARG;
    }
    A3dNativeMixLock lock;
    *caps = {sizeof(*caps), flags_, samples_->bytes, 0, 0};
    return S_OK;
}

HRESULT A3dNativeBuffer::GetCurrentPosition(LPDWORD read, LPDWORD write) {
    if (!read && !write) {
        return E_POINTER;
    }
    A3dNativeMixLock lock;
    if (read) {
        *read = cursor_;
    }
    if (write) {
        *write = cursor_;
    }
    return S_OK;
}

HRESULT A3dNativeBuffer::GetFormat(LPWAVEFORMATEX format, DWORD bytes, LPDWORD written) {
    if (written) {
        *written = sizeof(format_);
    }
    if (!format) {
        return written ? S_OK : E_POINTER;
    }
    if (bytes < sizeof(format_)) {
        return E_INVALIDARG;
    }
    return Read(format_, format);
}

HRESULT A3dNativeBuffer::GetVolume(LPLONG volume) {

    return Read(volume_, volume);

}
HRESULT A3dNativeBuffer::GetPan(LPLONG pan) {
    return Read(pan_, pan);
}
HRESULT A3dNativeBuffer::GetFrequency(LPDWORD frequency) {
    return Read(voice_.frequency_, frequency);
}
HRESULT A3dNativeBuffer::GetPriority(LPDWORD priority) {
    return Read(priority_, priority);
}
HRESULT A3dNativeBuffer::GetBufferState(LPDWORD state) {
    return Read(state_, state);
}

HRESULT A3dNativeBuffer::GetStatus(LPDWORD status) {
    if (!status) {
        return E_POINTER;
    }
    A3dNativeMixLock lock;
    *status = state_ == A3D_BUFFER_STATE_PLAYING ? DSBSTATUS_PLAYING : 0;
    if (looping_) {
        *status |= DSBSTATUS_LOOPING;
    }
    return S_OK;
}

HRESULT A3dNativeBuffer::Initialize(LPDIRECTSOUND, LPCDSBUFFERDESC) {

    return E_NOTIMPL;

}
HRESULT A3dNativeBuffer::SetFormat(LPCWAVEFORMATEX) {
    return E_NOTIMPL;
}
HRESULT A3dNativeBuffer::Restore() {
    return E_NOTIMPL;
}
HRESULT A3dNativeBuffer::GetAllocationStatus(DWORD) {
    return E_NOTIMPL;
}
HRESULT A3dNativeBuffer::GetWave(DWORD) {
    return E_NOTIMPL;
}
HRESULT A3dNativeBuffer::GetDriverInfo(DWORD, DWORD) {
    return E_NOTIMPL;
}
HRESULT A3dNativeBuffer::SetNewBuffer(DWORD, DWORD) {
    return E_NOTIMPL;
}
HRESULT A3dNativeBuffer::SetA3dDirectCtrl(DWORD, DWORD) {
    return E_NOTIMPL;
}
HRESULT A3dNativeBuffer::GetStatusEx(DWORD) {
    return E_NOTIMPL;
}

HRESULT A3dNativeBuffer::Lock(DWORD offset, DWORD bytes, LPVOID* first, LPDWORD firstBytes,
    LPVOID* second, LPDWORD secondBytes, DWORD flags) {
    if (!first || !firstBytes || ((second == nullptr) != (secondBytes == nullptr))) {
        return E_INVALIDARG;
    }
    *first = nullptr;
    *firstBytes = 0;
    if (second) {
        *second = nullptr;
        *secondBytes = 0;
    }
    A3dNativeMixLock lock;
    if (flags & ~(DSBLOCK_ENTIREBUFFER | DSBLOCK_FROMWRITECURSOR) || samples_->writer) {
        return E_INVALIDARG;
    }
    if (flags & DSBLOCK_ENTIREBUFFER) {
        offset = 0;
        bytes = samples_->bytes;
    } else if (flags & DSBLOCK_FROMWRITECURSOR) {
        offset = cursor_ % samples_->bytes;
    }
    if (offset >= samples_->bytes || !bytes || bytes > samples_->bytes) {
        return E_INVALIDARG;
    }
    const DWORD contiguous = std::min(bytes, samples_->bytes - offset);
    if (contiguous != bytes && !second) {
        return E_INVALIDARG;
    }
    samples_->writer = this;
    lockOffset_ = offset;
    lockBytes_ = bytes;
    *first = samples_->data + offset;
    *firstBytes = contiguous;
    if (second && contiguous != bytes) {
        *second = samples_->data;
        *secondBytes = bytes - contiguous;
    }
    return S_OK;
}

HRESULT A3dNativeBuffer::Unlock(LPVOID first, DWORD firstBytes, LPVOID second, DWORD secondBytes) {
    A3dNativeMixLock lock;
    const DWORD contiguous = std::min(lockBytes_, samples_->bytes - lockOffset_);
    const DWORD remainder = lockBytes_ - contiguous;
    if (samples_->writer != this || first != samples_->data + lockOffset_ || firstBytes != contiguous ||
        secondBytes != remainder || second != (remainder ? samples_->data : nullptr)) {
        return E_INVALIDARG;
    }
    samples_->writer = nullptr;
    lockBytes_ = 0;
    return S_OK;
}

void A3dNativeBuffer::Seek(DWORD position) {
    // Public positions are bytes. The legacy routine writes its argument into
    // both byte and sample fields; explicitly project each native field instead.
    AR2Dsetposition(&voice_.voice_, &voice_.input_, position);
    const LONG frame = static_cast<LONG>(position / format_.nBlockAlign);
    voice_.voice_.channels[0].iSourceSample = frame;
    voice_.voice_.channels[1].iSourceSample = frame;
    cursor_ = position;
}

HRESULT A3dNativeBuffer::SetCurrentPosition(DWORD position) {
    A3dNativeMixLock lock;
    if (position >= samples_->bytes || position % format_.nBlockAlign) {
        return E_INVALIDARG;
    }
    Seek(position);
    return S_OK;
}

HRESULT A3dNativeBuffer::Play(DWORD reserved, DWORD priority, DWORD flags) {
    A3dNativeMixLock lock;
    if (reserved || priority || flags & ~DSBPLAY_LOOPING) {
        return E_INVALIDARG;
    }
    if (!mixer_) {
        return E_ABORT;
    }
    if (cursor_ == samples_->bytes) {
        Seek(0);
    }
    looping_ = (flags & DSBPLAY_LOOPING) != 0;
    if (looping_) {
        voice_.input_.dwFlags |= A3D_MIX_LOOPING;
    } else {
        voice_.input_.dwFlags &= ~A3D_MIX_LOOPING;
    }
    state_ = A3D_BUFFER_STATE_PLAYING;
    return S_OK;
}

HRESULT A3dNativeBuffer::Stop() {
    A3dNativeMixLock lock;
    state_ = A3D_BUFFER_STATE_STOPPED;
    return S_OK;
}

HRESULT A3dNativeBuffer::SetVolume(LONG volume) {
    if (volume < DSBVOLUME_MIN || volume > DSBVOLUME_MAX) {
        return E_INVALIDARG;
    }
    A3dNativeMixLock lock;
    volume_ = volume;
    return S_OK;
}

HRESULT A3dNativeBuffer::SetPan(LONG pan) {
    if (pan < DSBPAN_LEFT || pan > DSBPAN_RIGHT) {
        return E_INVALIDARG;
    }
    A3dNativeMixLock lock;
    pan_ = pan;
    return S_OK;
}

HRESULT A3dNativeBuffer::SetFrequency(DWORD frequency) {
    A3dNativeMixLock lock;
    if (frequency == DSBFREQUENCY_ORIGINAL) {
        frequency = format_.nSamplesPerSec;
    }
    // Like retained A2D, frequency is consumed on the next super-control submit.
    voice_.frequency_ = frequency;
    return S_OK;
}

HRESULT A3dNativeBuffer::SetA3dSuperCtrl(void* data, DWORD) {
    if (!data) {
        return E_INVALIDARG;
    }
    return voice_.SetControl(*static_cast<const A3DCTRL_SRC_SUPER*>(data));
}

HRESULT A3dNativeBuffer::SetPriority(DWORD priority) {
    A3dNativeMixLock lock;
    priority_ = priority;
    return S_OK;
}

HRESULT A3dNativeBuffer::Mute() {
    A3dNativeMixLock lock;
    muted_ = true;
    return S_OK;
}

HRESULT A3dNativeBuffer::UnMute() {
    A3dNativeMixLock lock;
    muted_ = false;
    return S_OK;
}

HRESULT A3dNativeBuffer::GetAllParameters(LPDS3DBUFFER parameters) {
    if (!parameters || parameters->dwSize != sizeof(*parameters)) {
        return E_INVALIDARG;
    }
    return Read(spatial_, parameters);
}

HRESULT A3dNativeBuffer::GetConeAngles(LPDWORD inside, LPDWORD outside) {
    if (!inside || !outside) {
        return E_POINTER;
    }
    A3dNativeMixLock lock;
    *inside = spatial_.dwInsideConeAngle;
    *outside = spatial_.dwOutsideConeAngle;
    return S_OK;
}

HRESULT A3dNativeBuffer::SetAllParameters(LPCDS3DBUFFER parameters, DWORD apply) {
    const HRESULT status = Immediate(apply);
    if (FAILED(status)) {
        return status;
    }
    if (!parameters || parameters->dwSize != sizeof(*parameters) || !ValidVector(parameters->vPosition) ||
        !ValidVector(parameters->vVelocity) || !ValidVector(parameters->vConeOrientation) ||
        parameters->dwInsideConeAngle > parameters->dwOutsideConeAngle ||
        parameters->dwOutsideConeAngle > kFullConeDegrees ||
        parameters->lConeOutsideVolume < DSBVOLUME_MIN || parameters->lConeOutsideVolume > DSBVOLUME_MAX ||
        !std::isfinite(parameters->flMinDistance) || !std::isfinite(parameters->flMaxDistance) ||
        parameters->flMinDistance <= 0 || parameters->flMaxDistance < parameters->flMinDistance ||
        parameters->dwMode > DS3DMODE_DISABLE) {
        return E_INVALIDARG;
    }
    A3dNativeMixLock lock;
    spatial_ = *parameters;
    return S_OK;
}

HRESULT A3dNativeBuffer::SetConeAngles(DWORD inside, DWORD outside, DWORD apply) {
    const HRESULT status = Immediate(apply);
    if (FAILED(status)) {
        return status;
    }
    if (inside > outside || outside > kFullConeDegrees) {
        return E_INVALIDARG;
    }
    A3dNativeMixLock lock;
    spatial_.dwInsideConeAngle = inside;
    spatial_.dwOutsideConeAngle = outside;
    return S_OK;
}

HRESULT A3dNativeBuffer::GetConeOrientation(D3DVECTOR* output) {
    return Read(spatial_.vConeOrientation, output);
}

HRESULT A3dNativeBuffer::SetConeOrientation(D3DVALUE x, D3DVALUE y, D3DVALUE z, DWORD apply) {
    const HRESULT status = Immediate(apply);
    if (FAILED(status)) {
        return status;
    }
    const D3DVECTOR value{x, y, z};
    A3dNativeMixLock lock;
    if (!ValidVector(value)) {
        return E_INVALIDARG;
    }
    spatial_.vConeOrientation = value;
    return S_OK;
}

HRESULT A3dNativeBuffer::GetPosition(D3DVECTOR* output) {
    return Read(spatial_.vPosition, output);
}

HRESULT A3dNativeBuffer::SetPosition(D3DVALUE x, D3DVALUE y, D3DVALUE z, DWORD apply) {
    const HRESULT status = Immediate(apply);
    if (FAILED(status)) {
        return status;
    }
    const D3DVECTOR value{x, y, z};
    A3dNativeMixLock lock;
    if (!ValidVector(value)) {
        return E_INVALIDARG;
    }
    spatial_.vPosition = value;
    return S_OK;
}

HRESULT A3dNativeBuffer::GetVelocity(D3DVECTOR* output) {
    return Read(spatial_.vVelocity, output);
}

HRESULT A3dNativeBuffer::SetVelocity(D3DVALUE x, D3DVALUE y, D3DVALUE z, DWORD apply) {
    const HRESULT status = Immediate(apply);
    if (FAILED(status)) {
        return status;
    }
    const D3DVECTOR value{x, y, z};
    A3dNativeMixLock lock;
    if (!ValidVector(value)) {
        return E_INVALIDARG;
    }
    spatial_.vVelocity = value;
    return S_OK;
}

HRESULT A3dNativeBuffer::GetConeOutsideVolume(LONG* output) {
    return Read(spatial_.lConeOutsideVolume, output);
}

HRESULT A3dNativeBuffer::SetConeOutsideVolume(LONG value, DWORD apply) {
    const HRESULT status = Immediate(apply);
    if (FAILED(status)) {
        return status;
    }
    A3dNativeMixLock lock;
    if (value < DSBVOLUME_MIN || value > DSBVOLUME_MAX) {
        return E_INVALIDARG;
    }
    spatial_.lConeOutsideVolume = value;
    return S_OK;
}

HRESULT A3dNativeBuffer::GetMaxDistance(D3DVALUE* output) {
    return Read(spatial_.flMaxDistance, output);
}

HRESULT A3dNativeBuffer::SetMaxDistance(D3DVALUE value, DWORD apply) {
    const HRESULT status = Immediate(apply);
    if (FAILED(status)) {
        return status;
    }
    A3dNativeMixLock lock;
    if (!std::isfinite(value) || value < spatial_.flMinDistance) {
        return E_INVALIDARG;
    }
    spatial_.flMaxDistance = value;
    return S_OK;
}

HRESULT A3dNativeBuffer::GetMinDistance(D3DVALUE* output) {
    return Read(spatial_.flMinDistance, output);
}

HRESULT A3dNativeBuffer::SetMinDistance(D3DVALUE value, DWORD apply) {
    const HRESULT status = Immediate(apply);
    if (FAILED(status)) {
        return status;
    }
    A3dNativeMixLock lock;
    if (!std::isfinite(value) || value <= 0 || value > spatial_.flMaxDistance) {
        return E_INVALIDARG;
    }
    spatial_.flMinDistance = value;
    return S_OK;
}

HRESULT A3dNativeBuffer::GetMode(DWORD* output) {
    return Read(spatial_.dwMode, output);
}

HRESULT A3dNativeBuffer::SetMode(DWORD value, DWORD apply) {
    const HRESULT status = Immediate(apply);
    if (FAILED(status)) {
        return status;
    }
    A3dNativeMixLock lock;
    if (value > DS3DMODE_DISABLE) {
        return E_INVALIDARG;
    }
    spatial_.dwMode = value;
    return S_OK;
}
