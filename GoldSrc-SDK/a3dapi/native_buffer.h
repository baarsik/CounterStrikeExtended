#pragma once
#include "native_mix.h"
#include "backend_legacy.h"

// Existing retained interface IIDs; these add no public slots.
constexpr IID IID_A3dNativeRenderBuffer = {
    0xe04814d8, 0x26fc, 0x11d2, {0xbb, 0x48, 0x00, 0x60, 0x08, 0x2f, 0x3c, 0x00}};
constexpr IID IID_A3dNativeBufferControl = {
    0xcbd91fa8, 0x41c6, 0x11d2, {0xbb, 0x48, 0x00, 0x60, 0x08, 0x2f, 0x3c, 0x00}};

struct A3dNativeSamples {
    BYTE* data = nullptr;
    DWORD bytes = 0;
    ULONG references = 1;
    A3dNativeBuffer* writer = nullptr;
};

// Caller references own buffers. The render list is borrowed only while the
// global mix lock is held. Duplicate buffers retain sample storage independently.
class A3dNativeBuffer final : public IDirectSoundBuffer, public IDirectSound3DBuffer,
    public IA3dDalBuffer, public IResManBuffer {
public:
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void** object) override;
    ULONG STDMETHODCALLTYPE AddRef() override;
    ULONG STDMETHODCALLTYPE Release() override;
    HRESULT STDMETHODCALLTYPE GetCaps(LPDSBCAPS pCaps) override;
    HRESULT STDMETHODCALLTYPE GetCurrentPosition(LPDWORD pdwReadPosition, LPDWORD pdwWritePosition) override;
    HRESULT STDMETHODCALLTYPE GetFormat(LPWAVEFORMATEX pFormat, DWORD dwBytes, LPDWORD pdwWritten) override;
    HRESULT STDMETHODCALLTYPE GetVolume(LPLONG plVolume) override;
    HRESULT STDMETHODCALLTYPE GetPan(LPLONG plPan) override;
    HRESULT STDMETHODCALLTYPE GetFrequency(LPDWORD pdwFrequency) override;
    HRESULT STDMETHODCALLTYPE GetStatus(LPDWORD pdwStatus) override;
    HRESULT STDMETHODCALLTYPE Initialize(LPDIRECTSOUND pDirectSound, LPCDSBUFFERDESC pDesc) override;
    HRESULT STDMETHODCALLTYPE Lock(DWORD dwOffset, DWORD dwBytes, LPVOID* ppFirst, LPDWORD pdwFirstBytes,
        LPVOID* ppSecond, LPDWORD pdwSecondBytes, DWORD dwFlags) override;
    HRESULT STDMETHODCALLTYPE Play(DWORD dwReserved, DWORD dwPriority, DWORD dwFlags) override;
    HRESULT STDMETHODCALLTYPE SetCurrentPosition(DWORD dwPosition) override;
    HRESULT STDMETHODCALLTYPE SetFormat(LPCWAVEFORMATEX pFormat) override;
    HRESULT STDMETHODCALLTYPE SetVolume(LONG lVolume) override;
    HRESULT STDMETHODCALLTYPE SetPan(LONG lPan) override;
    HRESULT STDMETHODCALLTYPE SetFrequency(DWORD dwFrequency) override;
    HRESULT STDMETHODCALLTYPE Stop() override;
    HRESULT STDMETHODCALLTYPE Unlock(LPVOID pFirst, DWORD dwFirstBytes, LPVOID pSecond,
        DWORD dwSecondBytes) override;
    HRESULT STDMETHODCALLTYPE Restore() override;
    HRESULT STDMETHODCALLTYPE GetAllParameters(LPDS3DBUFFER pParameters) override;
    HRESULT STDMETHODCALLTYPE GetConeAngles(LPDWORD pdwInside, LPDWORD pdwOutside) override;
    HRESULT STDMETHODCALLTYPE GetConeOrientation(LPD3DVECTOR pOrientation) override;
    HRESULT STDMETHODCALLTYPE GetConeOutsideVolume(LPLONG plVolume) override;
    HRESULT STDMETHODCALLTYPE GetMaxDistance(D3DVALUE* pflDistance) override;
    HRESULT STDMETHODCALLTYPE GetMinDistance(D3DVALUE* pflDistance) override;
    HRESULT STDMETHODCALLTYPE GetMode(LPDWORD pdwMode) override;
    HRESULT STDMETHODCALLTYPE GetPosition(LPD3DVECTOR pPosition) override;
    HRESULT STDMETHODCALLTYPE GetVelocity(LPD3DVECTOR pVelocity) override;
    HRESULT STDMETHODCALLTYPE SetAllParameters(LPCDS3DBUFFER pParameters, DWORD dwApply) override;
    HRESULT STDMETHODCALLTYPE SetConeAngles(DWORD dwInside, DWORD dwOutside, DWORD dwApply) override;
    HRESULT STDMETHODCALLTYPE SetConeOrientation(D3DVALUE flX, D3DVALUE flY, D3DVALUE flZ, DWORD dwApply) override;
    HRESULT STDMETHODCALLTYPE SetConeOutsideVolume(LONG lVolume, DWORD dwApply) override;
    HRESULT STDMETHODCALLTYPE SetMaxDistance(D3DVALUE flDistance, DWORD dwApply) override;
    HRESULT STDMETHODCALLTYPE SetMinDistance(D3DVALUE flDistance, DWORD dwApply) override;
    HRESULT STDMETHODCALLTYPE SetMode(DWORD dwMode, DWORD dwApply) override;
    HRESULT STDMETHODCALLTYPE SetPosition(D3DVALUE flX, D3DVALUE flY, D3DVALUE flZ, DWORD dwApply) override;
    HRESULT STDMETHODCALLTYPE SetVelocity(D3DVALUE flX, D3DVALUE flY, D3DVALUE flZ, DWORD dwApply) override;
    HRESULT STDMETHODCALLTYPE SetA3dSuperCtrl(void* data, DWORD flags) override;
    HRESULT STDMETHODCALLTYPE GetAllocationStatus(DWORD value) override;
    HRESULT STDMETHODCALLTYPE GetWave(DWORD value) override;
    HRESULT STDMETHODCALLTYPE GetDriverInfo(DWORD first, DWORD second) override;
    HRESULT STDMETHODCALLTYPE SetNewBuffer(DWORD first, DWORD second) override;
    HRESULT STDMETHODCALLTYPE SetA3dDirectCtrl(DWORD first, DWORD second) override;
    HRESULT STDMETHODCALLTYPE GetStatusEx(DWORD value) override;
    HRESULT STDMETHODCALLTYPE GetPriority(LPDWORD priority) override;
    HRESULT STDMETHODCALLTYPE GetBufferState(LPDWORD state) override;
    HRESULT STDMETHODCALLTYPE SetPriority(DWORD priority) override;
    HRESULT STDMETHODCALLTYPE Mute() override;
    HRESULT STDMETHODCALLTYPE UnMute() override;
private:
    friend class A3dNativeMixer;
    A3dNativeBuffer() = default;
    A3dNativeBuffer(const A3dNativeBuffer&) = delete;
    A3dNativeBuffer& operator=(const A3dNativeBuffer&) = delete;
    ~A3dNativeBuffer();
    void Seek(DWORD position);
    void Attach(A3dNativeMixer& mixer);
    void Detach();
    ULONG references_ = 1;
    A3dNativeMixer* mixer_ = nullptr;
    A3dNativeBuffer* next_ = nullptr;
    A3dNativeBuffer* previous_ = nullptr;
    A3dNativeSamples* samples_ = nullptr;
    WAVEFORMATEX format_{};
    DS3DBUFFER spatial_{};
    A3dNativeVoice voice_;
    DWORD flags_ = 0;
    DWORD cursor_ = 0;
    DWORD state_ = A3D_BUFFER_STATE_STOPPED;
    DWORD priority_ = 0;
    LONG volume_ = 0;
    LONG pan_ = 0;
    bool looping_ = false;
    bool muted_ = false;
    DWORD lockOffset_ = 0;
    DWORD lockBytes_ = 0;
};
