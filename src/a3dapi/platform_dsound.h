#pragma once

#include "platform_com.h"
#ifdef _WIN32
#include <dsound.h>
#else
DEFINE_GUID(IID_IDirectSound, 0x279AFA83, 0x4981, 0x11CE,
    0xA5, 0x21, 0x00, 0x20, 0xAF, 0x0B, 0xE5, 0x60);
DEFINE_GUID(IID_IDirectSoundBuffer, 0x279AFA85, 0x4981, 0x11CE,
    0xA5, 0x21, 0x00, 0x20, 0xAF, 0x0B, 0xE5, 0x60);
DEFINE_GUID(IID_IDirectSound3DListener, 0x279AFA84, 0x4981, 0x11CE,
    0xA5, 0x21, 0x00, 0x20, 0xAF, 0x0B, 0xE5, 0x60);
DEFINE_GUID(IID_IDirectSound3DBuffer, 0x279AFA86, 0x4981, 0x11CE,
    0xA5, 0x21, 0x00, 0x20, 0xAF, 0x0B, 0xE5, 0x60);
DEFINE_GUID(IID_IDirectSoundNotify, 0xb0210783, 0x89cd, 0x11d0,
    0xaf, 0x8, 0x0, 0xa0, 0xc9, 0x25, 0xcd, 0x16);

// Literal flag/range values from Windows SDK 10.0.26100.0 dsound.h.
constexpr DWORD DS3DMODE_DISABLE = 0x00000002;
constexpr DWORD DS3DMODE_HEADRELATIVE = 0x00000001;
constexpr DWORD DS3DMODE_NORMAL = 0x00000000;
constexpr DWORD DS3D_DEFERRED = 0x00000001;
constexpr DWORD DS3D_IMMEDIATE = 0x00000000;
constexpr DWORD DSBCAPS_CTRL3D = 0x00000010;
constexpr DWORD DSBCAPS_CTRLFREQUENCY = 0x00000020;
constexpr DWORD DSBCAPS_CTRLPAN = 0x00000040;
constexpr DWORD DSBCAPS_CTRLPOSITIONNOTIFY = 0x00000100;
constexpr DWORD DSBCAPS_CTRLVOLUME = 0x00000080;
constexpr DWORD DSBCAPS_GETCURRENTPOSITION2 = 0x00010000;
constexpr DWORD DSBCAPS_GLOBALFOCUS = 0x00008000;
constexpr DWORD DSBCAPS_LOCHARDWARE = 0x00000004;
constexpr DWORD DSBCAPS_LOCSOFTWARE = 0x00000008;
constexpr DWORD DSBCAPS_PRIMARYBUFFER = 0x00000001;
constexpr DWORD DSBCAPS_STATIC = 0x00000002;
constexpr DWORD DSBFREQUENCY_MIN = 100;
constexpr DWORD DSBFREQUENCY_ORIGINAL = 0;
constexpr DWORD DSBLOCK_ENTIREBUFFER = 0x00000002;
constexpr DWORD DSBLOCK_FROMWRITECURSOR = 0x00000001;
constexpr LONG DSBPAN_LEFT = -10000;
constexpr LONG DSBPAN_RIGHT = 10000;
constexpr DWORD DSBPLAY_LOOPING = 0x00000001;
constexpr DWORD DSBSTATUS_LOOPING = 0x00000004;
constexpr DWORD DSBSTATUS_PLAYING = 0x00000001;
constexpr LONG DSBVOLUME_MAX = 0;
constexpr LONG DSBVOLUME_MIN = -10000;
constexpr DWORD DSCAPS_CONTINUOUSRATE = 0x00000010;
constexpr DWORD DSCAPS_PRIMARY16BIT = 0x00000008;
constexpr DWORD DSCAPS_PRIMARY8BIT = 0x00000004;
constexpr DWORD DSCAPS_PRIMARYMONO = 0x00000001;
constexpr DWORD DSCAPS_PRIMARYSTEREO = 0x00000002;
constexpr DWORD DSCAPS_SECONDARY16BIT = 0x00000800;
constexpr DWORD DSCAPS_SECONDARY8BIT = 0x00000400;
constexpr DWORD DSCAPS_SECONDARYMONO = 0x00000100;
constexpr DWORD DSCAPS_SECONDARYSTEREO = 0x00000200;

constexpr DWORD kA3dDirectSoundFacility = 0x878;
constexpr HRESULT DS_OK = S_OK;
constexpr HRESULT DSERR_INVALIDPARAM = E_INVALIDARG;
constexpr HRESULT DSERR_UNINITIALIZED = MAKE_HRESULT(1, kA3dDirectSoundFacility, 170);
constexpr HRESULT DSERR_UNSUPPORTED = E_NOTIMPL;

// Full SDK records and consumed DirectSound views. These are declarations only;
// native services are supplied by the owned provider, never emulated here.
using D3DVALUE = float;
struct D3DVECTOR {
    D3DVALUE x;
    D3DVALUE y;
    D3DVALUE z;
};
using LPD3DVECTOR = D3DVECTOR*;

struct DSCAPS {
    DWORD dwSize;
    DWORD dwFlags;
    DWORD dwMinSecondarySampleRate;
    DWORD dwMaxSecondarySampleRate;
    DWORD dwPrimaryBuffers;
    DWORD dwMaxHwMixingAllBuffers;
    DWORD dwMaxHwMixingStaticBuffers;
    DWORD dwMaxHwMixingStreamingBuffers;
    DWORD dwFreeHwMixingAllBuffers;
    DWORD dwFreeHwMixingStaticBuffers;
    DWORD dwFreeHwMixingStreamingBuffers;
    DWORD dwMaxHw3DAllBuffers;
    DWORD dwMaxHw3DStaticBuffers;
    DWORD dwMaxHw3DStreamingBuffers;
    DWORD dwFreeHw3DAllBuffers;
    DWORD dwFreeHw3DStaticBuffers;
    DWORD dwFreeHw3DStreamingBuffers;
    DWORD dwTotalHwMemBytes;
    DWORD dwFreeHwMemBytes;
    DWORD dwMaxContigFreeHwMemBytes;
    DWORD dwUnlockTransferRateHwBuffers;
    DWORD dwPlayCpuOverheadSwBuffers;
    DWORD dwReserved1;
    DWORD dwReserved2;
};
using LPDSCAPS = DSCAPS*;
using LPCDSCAPS = const DSCAPS*;

struct DSBCAPS {
    DWORD dwSize;
    DWORD dwFlags;
    DWORD dwBufferBytes;
    DWORD dwUnlockTransferRate;
    DWORD dwPlayCpuOverhead;
};
using LPDSBCAPS = DSBCAPS*;
using LPCDSBCAPS = const DSBCAPS*;

struct DSBUFFERDESC1 {
    DWORD dwSize;
    DWORD dwFlags;
    DWORD dwBufferBytes;
    DWORD dwReserved;
    LPWAVEFORMATEX lpwfxFormat;
};
struct DSBUFFERDESC {
    DWORD dwSize;
    DWORD dwFlags;
    DWORD dwBufferBytes;
    DWORD dwReserved;
    LPWAVEFORMATEX lpwfxFormat;
    GUID guid3DAlgorithm;
};
using LPDSBUFFERDESC = DSBUFFERDESC*;
using LPCDSBUFFERDESC = const DSBUFFERDESC*;
using LPDSBUFFERDESC1 = DSBUFFERDESC1*;
using LPCDSBUFFERDESC1 = const DSBUFFERDESC1*;

struct DS3DBUFFER {
    DWORD dwSize;
    D3DVECTOR vPosition;
    D3DVECTOR vVelocity;
    DWORD dwInsideConeAngle;
    DWORD dwOutsideConeAngle;
    D3DVECTOR vConeOrientation;
    LONG lConeOutsideVolume;
    D3DVALUE flMinDistance;
    D3DVALUE flMaxDistance;
    DWORD dwMode;
};
using LPDS3DBUFFER = DS3DBUFFER*;
using LPCDS3DBUFFER = const DS3DBUFFER*;

struct DS3DLISTENER {
    DWORD dwSize;
    D3DVECTOR vPosition;
    D3DVECTOR vVelocity;
    D3DVECTOR vOrientFront;
    D3DVECTOR vOrientTop;
    D3DVALUE flDistanceFactor;
    D3DVALUE flRolloffFactor;
    D3DVALUE flDopplerFactor;
};
using LPDS3DLISTENER = DS3DLISTENER*;
using LPCDS3DLISTENER = const DS3DLISTENER*;

struct DSBPOSITIONNOTIFY {
    DWORD dwOffset;
    HANDLE hEventNotify;
};
using LPDSBPOSITIONNOTIFY = DSBPOSITIONNOTIFY*;
using LPCDSBPOSITIONNOTIFY = const DSBPOSITIONNOTIFY*;

struct IDirectSound;
struct IDirectSoundBuffer;
using LPDIRECTSOUND = IDirectSound*;
using LPDIRECTSOUNDBUFFER = IDirectSoundBuffer*;

struct IDirectSound : public IUnknown {
    virtual HRESULT STDMETHODCALLTYPE CreateSoundBuffer(LPCDSBUFFERDESC pDesc, LPDIRECTSOUNDBUFFER* ppBuffer,
        IUnknown* pOuter) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetCaps(LPDSCAPS pCaps) = 0;
    virtual HRESULT STDMETHODCALLTYPE DuplicateSoundBuffer(LPDIRECTSOUNDBUFFER pBuffer,
        LPDIRECTSOUNDBUFFER* ppDuplicate) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetCooperativeLevel(HWND hWindow, DWORD dwLevel) = 0;
    virtual HRESULT STDMETHODCALLTYPE Compact() = 0;
    virtual HRESULT STDMETHODCALLTYPE GetSpeakerConfig(LPDWORD pdwConfig) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetSpeakerConfig(DWORD dwConfig) = 0;
    virtual HRESULT STDMETHODCALLTYPE Initialize(LPCGUID pDevice) = 0;
};

struct IDirectSoundBuffer : public IUnknown {
    virtual HRESULT STDMETHODCALLTYPE GetCaps(LPDSBCAPS pCaps) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetCurrentPosition(LPDWORD pdwReadPosition, LPDWORD pdwWritePosition) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetFormat(LPWAVEFORMATEX pFormat, DWORD dwBytes, LPDWORD pdwWritten) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetVolume(LPLONG plVolume) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetPan(LPLONG plPan) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetFrequency(LPDWORD pdwFrequency) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetStatus(LPDWORD pdwStatus) = 0;
    virtual HRESULT STDMETHODCALLTYPE Initialize(LPDIRECTSOUND pDirectSound, LPCDSBUFFERDESC pDesc) = 0;
    virtual HRESULT STDMETHODCALLTYPE Lock(DWORD dwOffset, DWORD dwBytes, LPVOID* ppFirst, LPDWORD pdwFirstBytes,
        LPVOID* ppSecond, LPDWORD pdwSecondBytes, DWORD dwFlags) = 0;
    virtual HRESULT STDMETHODCALLTYPE Play(DWORD dwReserved, DWORD dwPriority, DWORD dwFlags) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetCurrentPosition(DWORD dwPosition) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetFormat(LPCWAVEFORMATEX pFormat) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetVolume(LONG lVolume) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetPan(LONG lPan) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetFrequency(DWORD dwFrequency) = 0;
    virtual HRESULT STDMETHODCALLTYPE Stop() = 0;
    virtual HRESULT STDMETHODCALLTYPE Unlock(LPVOID pFirst, DWORD dwFirstBytes, LPVOID pSecond,
        DWORD dwSecondBytes) = 0;
    virtual HRESULT STDMETHODCALLTYPE Restore() = 0;
};

struct IDirectSound3DListener : public IUnknown {
    virtual HRESULT STDMETHODCALLTYPE GetAllParameters(LPDS3DLISTENER pParameters) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetDistanceFactor(D3DVALUE* pflFactor) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetDopplerFactor(D3DVALUE* pflFactor) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetOrientation(LPD3DVECTOR pFront, LPD3DVECTOR pTop) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetPosition(LPD3DVECTOR pPosition) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetRolloffFactor(D3DVALUE* pflFactor) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetVelocity(LPD3DVECTOR pVelocity) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetAllParameters(LPCDS3DLISTENER pParameters, DWORD dwApply) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetDistanceFactor(D3DVALUE flFactor, DWORD dwApply) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetDopplerFactor(D3DVALUE flFactor, DWORD dwApply) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetOrientation(D3DVALUE flFrontX, D3DVALUE flFrontY, D3DVALUE flFrontZ,
        D3DVALUE flTopX, D3DVALUE flTopY, D3DVALUE flTopZ, DWORD dwApply) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetPosition(D3DVALUE flX, D3DVALUE flY, D3DVALUE flZ, DWORD dwApply) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetRolloffFactor(D3DVALUE flFactor, DWORD dwApply) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetVelocity(D3DVALUE flX, D3DVALUE flY, D3DVALUE flZ, DWORD dwApply) = 0;
    virtual HRESULT STDMETHODCALLTYPE CommitDeferredSettings() = 0;
};

struct IDirectSound3DBuffer : public IUnknown {
    virtual HRESULT STDMETHODCALLTYPE GetAllParameters(LPDS3DBUFFER pParameters) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetConeAngles(LPDWORD pdwInside, LPDWORD pdwOutside) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetConeOrientation(LPD3DVECTOR pOrientation) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetConeOutsideVolume(LPLONG plVolume) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetMaxDistance(D3DVALUE* pflDistance) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetMinDistance(D3DVALUE* pflDistance) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetMode(LPDWORD pdwMode) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetPosition(LPD3DVECTOR pPosition) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetVelocity(LPD3DVECTOR pVelocity) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetAllParameters(LPCDS3DBUFFER pParameters, DWORD dwApply) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetConeAngles(DWORD dwInside, DWORD dwOutside, DWORD dwApply) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetConeOrientation(D3DVALUE flX, D3DVALUE flY, D3DVALUE flZ, DWORD dwApply) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetConeOutsideVolume(LONG lVolume, DWORD dwApply) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetMaxDistance(D3DVALUE flDistance, DWORD dwApply) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetMinDistance(D3DVALUE flDistance, DWORD dwApply) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetMode(DWORD dwMode, DWORD dwApply) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetPosition(D3DVALUE flX, D3DVALUE flY, D3DVALUE flZ, DWORD dwApply) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetVelocity(D3DVALUE flX, D3DVALUE flY, D3DVALUE flZ, DWORD dwApply) = 0;
};

struct IDirectSoundNotify : public IUnknown {
    virtual HRESULT STDMETHODCALLTYPE SetNotificationPositions(DWORD count, LPCDSBPOSITIONNOTIFY notifications) = 0;
};

using LPDIRECTSOUND3DBUFFER = IDirectSound3DBuffer*;
using LPDIRECTSOUND3DLISTENER = IDirectSound3DListener*;
using LPDIRECTSOUNDNOTIFY = IDirectSoundNotify*;
#endif
