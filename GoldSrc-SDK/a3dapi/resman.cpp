#define _CRT_SECURE_NO_WARNINGS
#include "native_platform.h"

#include "resman.h"
#include "dalinfo.h"
#include "rmstatbuffer.h"
#include "A3dGeom.h"
#include "dal_a2d.h"

#include <string.h>
#include <new>
#include <stdlib.h>
#include <stdio.h>
#include <algorithm>
#include <cmath>
#ifndef _WIN32
#include "native_capacity.h"
#include "native_device_init.h"
#include "native_provider.h"
#include "native_buffer.h"
#endif
#ifndef _WIN32


std::recursive_mutex& A3dNativeResourceMutex() noexcept {
    static std::recursive_mutex mutex;
    return mutex;
}

HRESULT ResManStreamBuffer::QueryInterface(REFIID iid, void** object) {
    if (!object) {
        return E_POINTER;
    }
    if (IsEqualIID(iid, IID_IDirectSound3DBuffer)) {
        *object = static_cast<IDirectSound3DBuffer*>(this);
        AddRef();
        return S_OK;
    }
    if (IsEqualIID(iid, IID_IA3dNativeNotifyV1)) {
        *object = static_cast<IA3dNativeNotifyV1*>(this);
        AddRef();
        return S_OK;
    }
    return ResManBuffer::QueryInterface(iid, object);
}

HRESULT ResManStreamBuffer::GetNotificationCaps(A3dNativeNotificationCapsV1* caps) {
    return A3dNativeNotifications::Caps(caps, A3D_NATIVE_CURSOR_STREAM_SUBMITTED);
}

HRESULT ResManStreamBuffer::SetNotificationPositions(DWORD count,
    const A3dNativeNotificationPositionV1* positions) {
    return m_nativeNotifications.Replace(count, positions, m_Desc.dwBufferBytes);
}
#endif

#define A3D_PLATFORM_LIST_BLOCK_NODES	10
#define A3D_PLATFORM_MAX_FREQUENCY	100000

static const IID IID_A3dRenderBuffer =
	{ 0xe04814d8, 0x26fc, 0x11d2, { 0xbb, 0x48, 0x00, 0x60, 0x08, 0x2f, 0x3c, 0x00 } };
static const IID IID_A3dBufferControl =
	{ 0xcbd91fa8, 0x41c6, 0x11d2, { 0xbb, 0x48, 0x00, 0x60, 0x08, 0x2f, 0x3c, 0x00 } };
static const IID IID_A3dSoftwareBufferControl =
	{ 0x03509d01, 0x74d0, 0x11d2, { 0x97, 0x1b, 0x00, 0x60, 0x08, 0x8d, 0x87, 0x88 } };
static const IID IID_A3dPlatformAudio =
	{ 0xcbd91fa7, 0x41c6, 0x11d2, { 0xbb, 0x48, 0x00, 0x60, 0x08, 0x2f, 0x3c, 0x00 } };
static const IID IID_A3dPlatformDeviceBackend =
	{ 0xa3c62f2a, 0x2671, 0x11d2, { 0xbb, 0x48, 0x00, 0x60, 0x08, 0x2f, 0x3c, 0x00 } };

ResMan::ResMan()
{
	memset( &m_SoftwareBuffers, 0, sizeof( m_SoftwareBuffers ) );
	m_SoftwareBuffers.cBlockNodes = A3D_PLATFORM_LIST_BLOCK_NODES;
	memset( &m_PendingBuffers, 0, sizeof( m_PendingBuffers ) );
	m_PendingBuffers.cBlockNodes = A3D_PLATFORM_LIST_BLOCK_NODES;
	memset( &m_HardwareBuffers, 0, sizeof( m_HardwareBuffers ) );
	m_HardwareBuffers.cBlockNodes = A3D_PLATFORM_LIST_BLOCK_NODES;
	m_pA3dBackend = NULL;
	m_dwReserved = 0;
	m_pDefaultPool = NULL;
	m_pHrtfPool = NULL;
	m_pSecondaryPool = NULL;
	m_dwResourceManagerMode = A3D_RESOURCE_MODE_OFF;
#ifdef _WIN32
	m_hUpdateEvent = INVALID_HANDLE_VALUE;
#else
    m_hUpdateEvent = nullptr;
#endif
	m_dwInitialized = 0;
	m_dwThreadId = 0;
	m_hThread = NULL;
	m_dwWaitTime = 0;
	m_dwUpdateInterval = 0;
	m_dwPreviousUpdate = 0;
	m_cReferences = 0;
}

ULONG STDMETHODCALLTYPE ResMan::AddRef()
{
	return A3dReferenceIncrement( &m_cReferences );
}

HRESULT STDMETHODCALLTYPE ResMan::QueryInterface( REFIID iid, void** ppObject )
{
	if ( !ppObject )
		return E_INVALIDARG;

	IUnknown* pObject;
	if ( !memcmp( &iid, &IID_IUnknown, sizeof( IID ) ) ||
		!memcmp( &iid, &IID_A3dPlatformAudio, sizeof( IID ) ) ||
		!memcmp( &iid, &IID_IDirectSound, sizeof( IID ) ) )
	{
		pObject = (IDirectSound*)this;
	}
	else if ( !memcmp( &iid, &IID_A3dPlatformDeviceBackend, sizeof( IID ) ) )
	{
		pObject = (IA3dDal*)this;
	}
	else if ( !memcmp( &iid, &IID_IA3d, sizeof( IID ) ) ||
		!memcmp( &iid, &IID_IA3d2, sizeof( IID ) ) )
	{
		pObject = (IA3d2*)this;
	}
	else
	{
		*ppObject = NULL;
		return E_NOINTERFACE;
	}

	*ppObject = pObject;
	pObject->AddRef();
	return S_OK;
}

ULONG STDMETHODCALLTYPE ResMan::Release()
{
	if ( !A3dReferenceDecrement( &m_cReferences ) )
		return 0;

	return m_cReferences;
}

HRESULT STDMETHODCALLTYPE ResMan::GetCaps( LPDSCAPS pCaps )
{
	IDirectSound* pDirectSound = NULL;
	HRESULT hr = ( (IA3dDal*)this )->GetDS( &pDirectSound );
	if ( FAILED( hr ) )
		return hr;
	if ( !pDirectSound )
		return E_FAIL;

#ifndef _WIN32
    hr = pDirectSound->GetCaps( pCaps );
    pDirectSound->Release();
    return hr;
#else
	return pDirectSound->GetCaps( pCaps );
#endif
}

HRESULT STDMETHODCALLTYPE ResMan::SetCooperativeLevel( HWND hWindow, DWORD dwLevel )
{
	IDirectSound* pDirectSound = NULL;
	HRESULT hr = ( (IA3dDal*)this )->GetDS( &pDirectSound );
	if ( FAILED( hr ) )
		return hr;
	if ( !pDirectSound )
		return E_FAIL;

#ifndef _WIN32
    hr = pDirectSound->SetCooperativeLevel( hWindow, dwLevel );
    pDirectSound->Release();
    return hr;
#else
	return pDirectSound->SetCooperativeLevel( hWindow, dwLevel );
#endif
}

HRESULT STDMETHODCALLTYPE ResMan::Compact()
{
	IDirectSound* pDirectSound = NULL;
	HRESULT hr = ( (IA3dDal*)this )->GetDS( &pDirectSound );
	if ( FAILED( hr ) )
		return hr;
	if ( !pDirectSound )
		return E_FAIL;

#ifndef _WIN32
    hr = pDirectSound->Compact();
    pDirectSound->Release();
    return hr;
#else
	return pDirectSound->Compact();
#endif
}

HRESULT STDMETHODCALLTYPE ResMan::GetSpeakerConfig( LPDWORD pdwConfig )
{
	if ( !pdwConfig )
		return E_POINTER;

	IDirectSound* pDirectSound = NULL;
	HRESULT hr = ( (IA3dDal*)this )->GetDS( &pDirectSound );
	if ( FAILED( hr ) )
		return hr;
	if ( !pDirectSound )
		return E_FAIL;

#ifndef _WIN32
    hr = pDirectSound->GetSpeakerConfig( pdwConfig );
    pDirectSound->Release();
    return hr;
#else
	return pDirectSound->GetSpeakerConfig( pdwConfig );
#endif
}

HRESULT STDMETHODCALLTYPE ResMan::SetSpeakerConfig( DWORD dwConfig )
{
	IDirectSound* pDirectSound = NULL;
	HRESULT hr = ( (IA3dDal*)this )->GetDS( &pDirectSound );
	if ( FAILED( hr ) )
		return hr;
	if ( !pDirectSound )
		return E_FAIL;

#ifndef _WIN32
    hr = pDirectSound->SetSpeakerConfig( dwConfig );
    pDirectSound->Release();
    return hr;
#else
	return pDirectSound->SetSpeakerConfig( dwConfig );
#endif
}

HRESULT STDMETHODCALLTYPE ResMan::Initialize( LPCGUID pDevice )
{
	IDirectSound* pDirectSound = NULL;
	HRESULT hr = ( (IA3dDal*)this )->GetDS( &pDirectSound );
	if ( FAILED( hr ) )
		return hr;
	if ( !pDirectSound )
		return E_FAIL;

#ifndef _WIN32
    hr = pDirectSound->Initialize( pDevice );
    pDirectSound->Release();
    return hr;
#else
	return pDirectSound->Initialize( pDevice );
#endif
}

HRESULT STDMETHODCALLTYPE ResMan::CreateSoundBuffer( LPCDSBUFFERDESC pDesc,
		IDirectSoundBuffer** ppBuffer, IUnknown* pOuter )
{
#ifndef _WIN32
    std::lock_guard<std::recursive_mutex> admission(m_nativeAdmission);
    if (!ppBuffer) {
        return E_POINTER;
    }
    *ppBuffer = nullptr;
    if (!m_dwInitialized) {
        return DSERR_UNINITIALIZED;
    }
    if (!pDesc || pOuter || (pDesc->dwSize != sizeof(DSBUFFERDESC) &&
        pDesc->dwSize != sizeof(DSBUFFERDESC1))) {
        return E_INVALIDARG;
    }
    if (pDesc->dwFlags & DSBCAPS_CTRLPOSITIONNOTIFY) {
        return DSERR_UNSUPPORTED;
    }
    a3d_platform_buffer_desc_t desc{sizeof(desc), pDesc->dwFlags, pDesc->dwBufferBytes,
        pDesc->dwReserved, pDesc->lpwfxFormat};
    if (m_dwResourceManagerMode == A3D_RESOURCE_MODE_OFF || pDesc->dwFlags & DSBCAPS_PRIMARYBUFFER) {
        const HRESULT result = m_pDefaultPool->GetIDal()->CreateSoundBufferEx(&desc, nullptr, ppBuffer, nullptr);
        if (SUCCEEDED(result)) {
            if (auto* native = dynamic_cast<A3dNativeBuffer*>(*ppBuffer)) {
                native->RetainNativeOwner(static_cast<IDirectSound*>(this));
            }
        }
        return result;
    }
    return CreateSecondaryBuffer(&desc, ppBuffer, nullptr);
#else
	if ( !m_dwInitialized )
		return DSERR_UNINITIALIZED;
	if ( pOuter )
		return DSERR_INVALIDPARAM;

	a3d_platform_buffer_desc_t desc;
	memcpy( &desc, pDesc, sizeof( desc ) );
	if ( desc.dwFlags & DSBCAPS_PRIMARYBUFFER )
		return CreatePrimaryBuffer( &desc, ppBuffer, NULL );

	desc.dwFlags |= DSBCAPS_LOCHARDWARE | DSBCAPS_CTRL3D;
	return CreateSecondaryBuffer( &desc, ppBuffer, NULL );
#endif
}

HRESULT ResMan::CreatePrimaryBuffer( a3d_platform_buffer_desc_t* pDesc,
		IDirectSoundBuffer** ppBuffer, IUnknown* pOuter )
{
	BYTE bMode = 0;
	pDesc->dwFlags |= DSBCAPS_PRIMARYBUFFER;
	return ( (IA3dDal*)this )->CreateSoundBufferEx( pDesc, &bMode, ppBuffer, pOuter );
}

HRESULT STDMETHODCALLTYPE ResMan::InitializeEx( DWORD dwFirst, DWORD dwSecond,
		void* pConfiguration, void* pBackend )
{
	if ( !pBackend )
		return E_POINTER;

	return StartRenderer( dwFirst, dwSecond, pConfiguration, pBackend );
}

HRESULT STDMETHODCALLTYPE ResMan::CreateSoundBufferEx( const a3d_platform_buffer_desc_t* pDesc,
		BYTE* pbMode, IDirectSoundBuffer** ppBuffer, IUnknown* pOuter )
{
#ifndef _WIN32
    if (ppBuffer) {
        *ppBuffer = nullptr;
    }
    if (!pDesc) {
        return E_POINTER;
    }
    if (pbMode) {
        *pbMode = 0;
    }
    DSBUFFERDESC description{};
    description.dwSize = sizeof(description);
    description.dwFlags = pDesc->dwFlags;
    description.dwBufferBytes = pDesc->dwBufferBytes;
    description.lpwfxFormat = pDesc->lpwfxFormat;
    return CreateSoundBuffer(&description, ppBuffer, pOuter);
#else
	if ( !pDesc || !pbMode || !ppBuffer )
		return E_POINTER;
	if ( pOuter )
		return E_INVALIDARG;

	return ( (IA3dDal*)m_pDefaultPool->GetIDal() )->CreateSoundBufferEx(
		pDesc, pbMode, ppBuffer, NULL );
#endif
}

HRESULT STDMETHODCALLTYPE ResMan::GetDS( IDirectSound** ppDirectSound )
{
#ifndef _WIN32
    std::lock_guard<std::recursive_mutex> admission(m_nativeAdmission);
    if (!m_nativeAttached) {
        return DSERR_UNINITIALIZED;
    }
#endif
	if ( !m_dwInitialized )
		return DSERR_UNINITIALIZED;
	if ( !ppDirectSound )
		return E_POINTER;

	return ( (IA3dDal*)m_pDefaultPool->GetIDal() )->GetDS( ppDirectSound );
}

HRESULT STDMETHODCALLTYPE ResMan::GetA3dCaps( void* pFirst, void* pSecond )
{
#ifndef _WIN32
    std::lock_guard<std::recursive_mutex> admission(m_nativeAdmission);
    if (!m_nativeAttached) {
        return DSERR_UNINITIALIZED;
    }
#endif
	if ( !pFirst || !pSecond )
		return E_POINTER;

	return ( (IA3dDal*)m_pDefaultPool->GetIDal() )->GetA3dCaps( pFirst, pSecond );
}

HRESULT STDMETHODCALLTYPE ResMan::GetDriverInfo( void* pFirst, void* pSecond,
		void* pThird, void* pFourth )
{
#ifndef _WIN32
    std::lock_guard<std::recursive_mutex> admission(m_nativeAdmission);
    if (!m_nativeAttached) {
        return DSERR_UNINITIALIZED;
    }
#endif
	if ( !pFirst || !pSecond || !pThird || !pFourth )
		return E_POINTER;

	return ( (IA3dDal*)m_pDefaultPool->GetIDal() )->GetDriverInfo(
		pFirst, pSecond, pThird, pFourth );
}

HRESULT STDMETHODCALLTYPE ResMan::GetDSDriverDesc( void* pFirst, void* pSecond )
{
#ifndef _WIN32
    std::lock_guard<std::recursive_mutex> admission(m_nativeAdmission);
    if (!m_nativeAttached) {
        return DSERR_UNINITIALIZED;
    }
#endif
	if ( !pFirst || !pSecond )
		return E_POINTER;

	return ( (IA3dDal*)m_pDefaultPool->GetIDal() )->GetDSDriverDesc( pFirst, pSecond );
}

HRESULT STDMETHODCALLTYPE ResMan::QueryFunctionality( DWORD dwFirst, void* pSecond )
{
#ifndef _WIN32
    std::lock_guard<std::recursive_mutex> admission(m_nativeAdmission);
    if (!m_nativeAttached) {
        return DSERR_UNINITIALIZED;
    }
#endif
	if ( !pSecond )
		return E_POINTER;

	return ( (IA3dDal*)m_pDefaultPool->GetIDal() )->QueryFunctionality( dwFirst, pSecond );
}

HRESULT STDMETHODCALLTYPE ResMan::DeviceCompatibility5( void* pFirst, void* pSecond, void* pThird )
{
#ifndef _WIN32
    std::lock_guard<std::recursive_mutex> admission(m_nativeAdmission);
    if (!m_nativeAttached) {
        return DSERR_UNINITIALIZED;
    }
#endif
	if ( !pFirst || !pSecond || !pThird )
		return E_POINTER;

	return ( (IA3dDal*)m_pDefaultPool->GetIDal() )->DeviceCompatibility5( pFirst, pSecond, pThird );
}

HRESULT STDMETHODCALLTYPE ResMan::SetOutputMode( DWORD dwFront, DWORD dwBack, DWORD dwQuad )
{
#ifndef _WIN32
    std::lock_guard<std::recursive_mutex> admission(m_nativeAdmission);
    if (!m_nativeAttached) {
        return DSERR_UNINITIALIZED;
    }
#endif
	if ( dwFront <= OUTPUT_HEADPHONES || dwFront >= OUTPUT_SPEAKERS_NARROW ||
		dwBack <= OUTPUT_HEADPHONES || dwBack >= OUTPUT_SPEAKERS_NARROW ||
		dwQuad <= OUTPUT_MODE_STEREO || dwQuad >= OUTPUT_MODE_QUAD )
		return E_INVALIDARG;

	return m_pA3dBackend->SetOutputMode( dwFront, dwBack, dwQuad );
}

HRESULT STDMETHODCALLTYPE ResMan::GetOutputMode( LPDWORD pdwFront, LPDWORD pdwBack, LPDWORD pdwQuad )
{
#ifndef _WIN32
    std::lock_guard<std::recursive_mutex> admission(m_nativeAdmission);
    if (!m_nativeAttached) {
        return DSERR_UNINITIALIZED;
    }
#endif
	if ( !pdwFront || !pdwBack || !pdwQuad )
		return E_POINTER;

	return m_pA3dBackend->GetOutputMode( pdwFront, pdwBack, pdwQuad );
}

HRESULT STDMETHODCALLTYPE ResMan::SetResourceManagerMode( DWORD dwMode )
{
#ifndef _WIN32
    std::lock_guard<std::recursive_mutex> lock(A3dNativeResourceMutex());
#endif
	if ( dwMode > A3D_RESOURCE_MODE_LAST )
		return E_INVALIDARG;

	m_dwResourceManagerMode = dwMode;
	return S_OK;
}

HRESULT STDMETHODCALLTYPE ResMan::GetResourceManagerMode( LPDWORD pdwMode )
{
#ifndef _WIN32
    std::lock_guard<std::recursive_mutex> lock(A3dNativeResourceMutex());
#endif
	if ( !pdwMode )
		return E_POINTER;

	*pdwMode = m_dwResourceManagerMode;
	return S_OK;
}

HRESULT STDMETHODCALLTYPE ResMan::SetHFAbsorbFactor( FLOAT flFactor )
{
#ifndef _WIN32
    std::lock_guard<std::recursive_mutex> admission(m_nativeAdmission);
    if (!m_nativeAttached) {
        return DSERR_UNINITIALIZED;
    }
#endif
	return m_pA3dBackend->SetHFAbsorbFactor( flFactor );
}

HRESULT STDMETHODCALLTYPE ResMan::GetHFAbsorbFactor( FLOAT* pflFactor )
{
#ifndef _WIN32
    std::lock_guard<std::recursive_mutex> admission(m_nativeAdmission);
    if (!m_nativeAttached) {
        return DSERR_UNINITIALIZED;
    }
#endif
	return m_pA3dBackend->GetHFAbsorbFactor( pflFactor );
}

HRESULT STDMETHODCALLTYPE ResMan::RegisterVersion( DWORD dwVersion )
{
#ifndef _WIN32
    std::lock_guard<std::recursive_mutex> admission(m_nativeAdmission);
    if (!m_nativeAttached) {
        return DSERR_UNINITIALIZED;
    }
#endif
	return m_pA3dBackend->RegisterVersion( dwVersion );
}

HRESULT STDMETHODCALLTYPE ResMan::GetSoftwareCaps( LPA3DCAPS_SOFTWARE pCaps )
{
#ifndef _WIN32
    std::lock_guard<std::recursive_mutex> admission(m_nativeAdmission);
    if (!m_nativeAttached) {
        return DSERR_UNINITIALIZED;
    }
#endif
	return m_pA3dBackend->GetSoftwareCaps( pCaps );
}

HRESULT STDMETHODCALLTYPE ResMan::GetHardwareCaps( LPA3DCAPS_HARDWARE pCaps )
{
#ifndef _WIN32
    std::lock_guard<std::recursive_mutex> admission(m_nativeAdmission);
    if (!m_nativeAttached) {
        return DSERR_UNINITIALIZED;
    }
#endif
	return m_pA3dBackend->GetHardwareCaps( pCaps );
}

DWORD ResMan::GetSoftwareStreamCount()
{
	return m_pDefaultPool->m_Streams.cStreams;
}

DWORD ResMan::GetFreeSoftwareVoices()
{
	return m_pDefaultPool->GetNumAvailable();
}

DWORD ResMan::CountPlayingSoftwareVoices()
{
	DWORD dwCount = 0;
	DalBufferInfo* pStream = NULL;
	a3d_platform_buffer_node_t* pNode = m_SoftwareBuffers.pHead;
	while ( pNode )
	{
		ResManBuffer* pBuffer = pNode->pBuffer;
		pNode = pNode->pNext;
		DWORD dwStatus;
		( (IA3dPlatformBufferControl*)pBuffer )->GetBufferState( &dwStatus );
		pBuffer->GetRenderStream( &pStream );
		if ( pStream && pStream->m_pPool == m_pDefaultPool && dwStatus == A3D_BUFFER_STATE_PLAYING )
			dwCount++;
	}

	return dwCount;
}

DWORD ResMan::CountPlayingBuffers()
{
	DWORD dwCount = 0;
	a3d_platform_buffer_node_t* pNode = m_SoftwareBuffers.pHead;
	while ( pNode )
	{
		ResManBuffer* pBuffer = pNode->pBuffer;
		pNode = pNode->pNext;
		DWORD dwStatus;
		( (IA3dPlatformBufferControl*)pBuffer )->GetBufferState( &dwStatus );
		if ( dwStatus == A3D_BUFFER_STATE_PLAYING )
			dwCount++;
	}

	return dwCount;
}

ResManStreamBuffer::ResManStreamBuffer( IUnknown* pOwner, DWORD dwBufferKind )
{
	m_pOwner = pOwner;
	m_pOwner->AddRef();
	m_cReferences = 0;
	m_pBufferToken = NULL;
	m_pStream = NULL;
	m_pRenderData = NULL;
	m_lVolume = 0;
	m_hData = NULL;
	m_lPan = 0;
	m_dwControl = 0;
	m_dwOriginalFrequency = 0;
	m_dwReadPosition = 0;
	m_dwFlags = 0;
	m_dwNotificationCount = 0;
	m_pNotifications = NULL;
	m_hNotificationMutex = NULL;
	m_dwBufferKind = dwBufferKind;
	m_dwFrequency = DSBFREQUENCY_MIN;
	m_dwOverridePosition = A3D_BUFFER_POSITION_UNSET;
	m_dwStatus = A3D_BUFFER_STATE_STOPPED;
#ifndef _WIN32
    m_Desc = {};
    constexpr DWORD kFullConeDegrees = 360;
    m_nativeSpatial.dwSize = sizeof(m_nativeSpatial);
    m_nativeSpatial.dwInsideConeAngle = kFullConeDegrees;
    m_nativeSpatial.dwOutsideConeAngle = kFullConeDegrees;
    m_nativeSpatial.vConeOrientation.z = 1.0f;
    constexpr float kDefaultMinimumDistance = 1.0f;
    constexpr float kDefaultMaximumDistance = 1000000000.0f;
    m_nativeSpatial.flMinDistance = kDefaultMinimumDistance;
    m_nativeSpatial.flMaxDistance = kDefaultMaximumDistance;
#endif
}

ResManStreamBuffer::~ResManStreamBuffer()
{
#ifndef _WIN32
    Stop();
    m_nativeNotifications.Shutdown();
    {
        std::lock_guard<std::recursive_mutex> lock(A3dNativeResourceMutex());
        if (m_nativeSamples) {
            if (m_nativeSamples->writer == this) {
                m_nativeSamples->writer = nullptr;
            }
            if (!--m_nativeSamples->references) {
                A3dFree(m_nativeSamples->data);
                delete m_nativeSamples;
            }
        }
    }
    free(m_pBufferToken);
#endif
#ifdef _WIN32
	if ( m_hNotificationMutex )
	{
		CloseHandle( m_hNotificationMutex );
		m_hNotificationMutex = NULL;
	}
#endif

	if ( m_pOwner )
	{
		m_pOwner->Release();
		m_pOwner = NULL;
	}
}

HRESULT STDMETHODCALLTYPE ResManBuffer::QueryInterface( REFIID iid, void** ppObject )
{
	if ( !ppObject )
		return E_INVALIDARG;

	IUnknown* pObject;
	if ( !memcmp( &iid, &IID_IUnknown, sizeof( IID ) ) ||
		!memcmp( &iid, &IID_IDirectSoundBuffer, sizeof( IID ) ) )
	{
		pObject = (IDirectSoundBuffer*)this;
	}
	else if ( !memcmp( &iid, &IID_A3dRenderBuffer, sizeof( IID ) ) )
	{
		pObject = (IA3dDalBuffer*)this;
	}
	else if ( !memcmp( &iid, &IID_A3dBufferControl, sizeof( IID ) ) )
	{
		pObject = (IResManBuffer*)this;
	}
#ifdef _WIN32
	else if ( !memcmp( &iid, &IID_IDirectSoundNotify, sizeof( IID ) ) )
	{
		pObject = (IDirectSoundBuffer*)this;
	}
#endif
	else if ( !memcmp( &iid, &IID_A3dSoftwareBufferControl, sizeof( IID ) ) )
	{
		pObject = (IResManBuffer*)this;
	}
	else
	{
		*ppObject = NULL;
		return E_NOINTERFACE;
	}

	*ppObject = pObject;
	pObject->AddRef();
	return S_OK;
}

ULONG STDMETHODCALLTYPE ResManBuffer::AddRef()
{
	return A3dReferenceIncrement( &m_cReferences );
}

ULONG STDMETHODCALLTYPE ResManBuffer::Release()
{
#ifndef _WIN32
    LONG remaining;
    {
        std::lock_guard<std::recursive_mutex> lock(A3dNativeResourceMutex());
        remaining = A3dReferenceDecrement(&m_cReferences);
    }
    if (!remaining) {
        auto* stream = dynamic_cast<ResManStreamBuffer*>(this);
        auto* owner = dynamic_cast<ResMan*>(m_pOwner);
        if (stream && owner) {
            owner->ReleaseNativeBuffer(stream);
        }
    }
    return remaining;
#else
	if ( !A3dReferenceDecrement( &m_cReferences ) )
	{
		m_dwStatus = A3D_BUFFER_STATE_RELEASED;
		return 0;
	}

	return m_cReferences;
#endif
}

HRESULT STDMETHODCALLTYPE ResManBuffer::GetCaps( LPDSBCAPS caps )
{
#ifndef _WIN32
    if (!caps || caps->dwSize != sizeof(*caps)) {
        return E_INVALIDARG;
    }
    std::lock_guard<std::recursive_mutex> lock(A3dNativeResourceMutex());
    const DWORD flags = (m_Desc.dwFlags & ~(DSBCAPS_LOCHARDWARE | DSBCAPS_CTRLPOSITIONNOTIFY)) |
        DSBCAPS_LOCSOFTWARE;
    *caps = {sizeof(*caps), flags, m_Desc.dwBufferBytes, 0, 0};
#endif
	return DS_OK;
}

HRESULT STDMETHODCALLTYPE ResManBuffer::GetFormat( LPWAVEFORMATEX format, DWORD bytes, LPDWORD written )
{
#ifndef _WIN32
    if (written) {
        *written = sizeof(m_nativeFormat);
    }
    if (!format) {
        return written ? S_OK : E_POINTER;
    }
    if (bytes < sizeof(m_nativeFormat)) {
        return E_INVALIDARG;
    }
    std::lock_guard<std::recursive_mutex> lock(A3dNativeResourceMutex());
    *format = m_nativeFormat;
    return S_OK;
#else
	return DS_OK;
#endif
}

HRESULT STDMETHODCALLTYPE ResManBuffer::Initialize( LPDIRECTSOUND, LPCDSBUFFERDESC )
{
	return DS_OK;
}

HRESULT STDMETHODCALLTYPE ResManBuffer::SetFormat( LPCWAVEFORMATEX )
{
	return DS_OK;
}

HRESULT STDMETHODCALLTYPE ResManBuffer::Unlock( LPVOID first, DWORD firstBytes, LPVOID second, DWORD secondBytes )
{
#ifndef _WIN32
    std::lock_guard<std::recursive_mutex> lock(A3dNativeResourceMutex());
    if (!m_nativeSamples) {
        return E_INVALIDARG;
    }
    const DWORD contiguous = std::min(m_nativeLockBytes, m_Desc.dwBufferBytes - m_nativeLockOffset);
    const DWORD remainder = m_nativeLockBytes - contiguous;
    if (m_nativeSamples->writer != this || first != m_nativeSamples->data + m_nativeLockOffset ||
        firstBytes != contiguous || secondBytes != remainder ||
        second != (remainder ? m_nativeSamples->data : nullptr)) {
        return E_INVALIDARG;
    }
    m_nativeSamples->writer = nullptr;
    m_nativeLockBytes = 0;
    return S_OK;
#else
	return DS_OK;
#endif
}

HRESULT STDMETHODCALLTYPE ResManBuffer::Restore()
{
	return DS_OK;
}

HRESULT STDMETHODCALLTYPE ResManBuffer::SetA3dSuperCtrl( void* pData, DWORD dwFlags )
{
#ifndef _WIN32
    if (!pData) {
        return E_INVALIDARG;
    }
    std::lock_guard<std::recursive_mutex> lock(A3dNativeResourceMutex());
    const auto& control = *static_cast<const A3DCTRL_SRC_SUPER*>(pData);
    if (m_pStream) {
        IA3dDalBuffer* render = m_pStream->GetIDalBuffer();
        if (!render) {
            return E_NOINTERFACE;
        }
        HRESULT result = m_pStream->GetDSBuffer()->SetFrequency(m_dwFrequency);
        if (SUCCEEDED(result)) {
            result = render->SetA3dSuperCtrl(pData, dwFlags);
        }
        if (FAILED(result)) {
            return result;
        }
    }
    m_nativeControl = control;
    m_nativeHasControl = true;
    m_pRenderData = &m_nativeControl;
    return S_OK;
#else
	if ( !m_pStream )
		return E_FAIL;

	m_pRenderData = pData;
	( (IA3dPlatformRenderBuffer*)m_pStream->GetIDalBuffer() )->SetA3dSuperCtrl( pData, dwFlags );
	return DS_OK;
#endif
}

HRESULT STDMETHODCALLTYPE ResManBuffer::GetAllocationStatus( DWORD )
{
	return DS_OK;
}

HRESULT STDMETHODCALLTYPE ResManBuffer::GetWave( DWORD )
{
	return DS_OK;
}

HRESULT STDMETHODCALLTYPE ResManBuffer::GetDriverInfo( DWORD, DWORD )
{
	return DS_OK;
}

HRESULT STDMETHODCALLTYPE ResManBuffer::SetNewBuffer( DWORD, DWORD )
{
	return DS_OK;
}

HRESULT STDMETHODCALLTYPE ResManBuffer::SetA3dDirectCtrl( DWORD, DWORD )
{
	return DS_OK;
}

HRESULT STDMETHODCALLTYPE ResManBuffer::GetStatusEx( DWORD )
{
	return DS_OK;
}

HRESULT STDMETHODCALLTYPE ResManBuffer::GetCurrentPosition( LPDWORD pdwReadPosition,
		LPDWORD pdwWritePosition )
{
#ifndef _WIN32
    if (!pdwReadPosition && !pdwWritePosition) {
        return E_POINTER;
    }
    std::lock_guard<std::recursive_mutex> lock(A3dNativeResourceMutex());
    const DWORD position = m_dwOverridePosition != A3D_BUFFER_POSITION_UNSET ?
        m_dwOverridePosition : m_dwReadPosition;
    if (pdwReadPosition) {
        *pdwReadPosition = position;
    }
    if (pdwWritePosition) {
        *pdwWritePosition = position;
    }
    return S_OK;
#else
	if ( m_dwOverridePosition != A3D_BUFFER_POSITION_UNSET )
	{
		if ( pdwReadPosition )
			*pdwReadPosition = m_dwOverridePosition;
	}
	else
	{
		DWORD dwPosition = m_dwReadPosition;
		if ( m_pStream )
		{
			DWORD dwStreamPosition;
			m_pStream->GetAudioBytesQueued( &dwStreamPosition );
			if ( dwStreamPosition > dwPosition )
				dwPosition += m_Desc.dwBufferBytes - dwStreamPosition;
			else
				dwPosition -= dwStreamPosition;
		}

		*pdwReadPosition = dwPosition;
	}

	if ( pdwWritePosition )
		*pdwWritePosition = *pdwReadPosition;

	return DS_OK;
#endif
}

HRESULT STDMETHODCALLTYPE ResManBuffer::GetVolume( LPLONG plVolume )
{
#ifndef _WIN32
    std::lock_guard<std::recursive_mutex> lock(A3dNativeResourceMutex());
#endif
	if ( !plVolume )
		return DSERR_INVALIDPARAM;

	*plVolume = m_lVolume;
	return DS_OK;
}

HRESULT STDMETHODCALLTYPE ResManBuffer::GetPan( LPLONG plPan )
{
#ifndef _WIN32
    std::lock_guard<std::recursive_mutex> lock(A3dNativeResourceMutex());
#endif
	if ( !plPan )
		return DSERR_INVALIDPARAM;

	*plPan = m_lPan;
	return DS_OK;
}

HRESULT STDMETHODCALLTYPE ResManBuffer::GetFrequency( LPDWORD pdwFrequency )
{
#ifndef _WIN32
    if (!pdwFrequency) {
        return E_POINTER;
    }
    std::lock_guard<std::recursive_mutex> lock(A3dNativeResourceMutex());
    *pdwFrequency = m_dwFrequency;
    return S_OK;
#else
	*pdwFrequency = m_dwFrequency;
	return DS_OK;
#endif
}

HRESULT STDMETHODCALLTYPE ResManBuffer::GetStatus( LPDWORD pdwStatus )
{
#ifndef _WIN32
    if (!pdwStatus) {
        return E_POINTER;
    }
    std::lock_guard<std::recursive_mutex> lock(A3dNativeResourceMutex());
    *pdwStatus = m_dwStatus == A3D_BUFFER_STATE_PLAYING ? DSBSTATUS_PLAYING : 0;
    if (m_dwFlags & A3D_RENDER_LOOPING) {
        *pdwStatus |= DSBSTATUS_LOOPING;
    }
    return S_OK;
#else
	*pdwStatus = 0;
	if ( m_dwStatus == A3D_BUFFER_STATE_PLAYING )
		*pdwStatus = DSBSTATUS_PLAYING;
	if ( m_dwFlags == A3D_RENDER_LOOPING )
		*pdwStatus |= DSBSTATUS_LOOPING;

	return DS_OK;
#endif
}

HRESULT STDMETHODCALLTYPE ResManBuffer::Lock( DWORD dwOffset, DWORD dwBytes, LPVOID* ppFirst,
		LPDWORD pdwFirstBytes, LPVOID* ppSecond, LPDWORD pdwSecondBytes, DWORD dwFlags )
{
#ifndef _WIN32
    if (!ppFirst || !pdwFirstBytes || ((ppSecond == nullptr) != (pdwSecondBytes == nullptr))) {
        return E_INVALIDARG;
    }
    *ppFirst = nullptr;
    *pdwFirstBytes = 0;
    if (ppSecond) {
        *ppSecond = nullptr;
        *pdwSecondBytes = 0;
    }
    std::lock_guard<std::recursive_mutex> lock(A3dNativeResourceMutex());
    if (!m_nativeSamples || m_nativeSamples->writer ||
        dwFlags & ~(DSBLOCK_ENTIREBUFFER | DSBLOCK_FROMWRITECURSOR)) {
        return E_INVALIDARG;
    }
    if (dwFlags & DSBLOCK_ENTIREBUFFER) {
        dwOffset = 0;
        dwBytes = m_Desc.dwBufferBytes;
    } else if (dwFlags & DSBLOCK_FROMWRITECURSOR) {
        dwOffset = m_dwReadPosition % m_Desc.dwBufferBytes;
    }
    if (dwOffset >= m_Desc.dwBufferBytes || !dwBytes || dwBytes > m_Desc.dwBufferBytes) {
        return E_INVALIDARG;
    }
    const DWORD contiguous = std::min(dwBytes, m_Desc.dwBufferBytes - dwOffset);
    if (contiguous != dwBytes && !ppSecond) {
        return E_INVALIDARG;
    }
    m_nativeSamples->writer = this;
    m_nativeLockOffset = dwOffset;
    m_nativeLockBytes = dwBytes;
    *ppFirst = m_nativeSamples->data + dwOffset;
    *pdwFirstBytes = contiguous;
    if (ppSecond && contiguous != dwBytes) {
        *ppSecond = m_nativeSamples->data;
        *pdwSecondBytes = dwBytes - contiguous;
    }
    return S_OK;
#else
	if ( !ppFirst || !pdwFirstBytes || dwBytes > m_Desc.dwBufferBytes )
		return DSERR_INVALIDPARAM;

	if ( ( dwFlags & DSBLOCK_FROMWRITECURSOR ) == DSBLOCK_FROMWRITECURSOR )
		dwOffset = m_dwReadPosition;

	*ppFirst = (BYTE*)m_hData + dwOffset;
	if ( dwOffset + dwBytes <= m_Desc.dwBufferBytes )
	{
		*pdwFirstBytes = dwBytes;
		if ( ppSecond && pdwSecondBytes )
		{
			*ppSecond = NULL;
			*pdwSecondBytes = 0;
		}
	}
	else
	{
		*pdwFirstBytes = m_Desc.dwBufferBytes - dwOffset;
		if ( ppSecond && pdwSecondBytes )
		{
			*ppSecond = m_hData;
			*pdwSecondBytes = dwBytes - *pdwFirstBytes;
		}
	}

	return DS_OK;
#endif
}

HRESULT STDMETHODCALLTYPE ResManBuffer::SetCurrentPosition( DWORD dwPosition )
{
#ifndef _WIN32
    std::lock_guard<std::recursive_mutex> lock(A3dNativeResourceMutex());
    if (!m_nativeSamples || dwPosition >= m_Desc.dwBufferBytes || dwPosition % m_nativeFormat.nBlockAlign) {
        return E_INVALIDARG;
    }
    m_dwOverridePosition = dwPosition;
    return S_OK;
#else
	if ( dwPosition > m_Desc.dwBufferBytes )
		return DSERR_INVALIDPARAM;

	m_dwOverridePosition = dwPosition;
	return DS_OK;
#endif
}

HRESULT STDMETHODCALLTYPE ResManBuffer::Play( DWORD dwReserved, DWORD dwPriority, DWORD dwFlags )
{
#ifndef _WIN32
    std::lock_guard<std::recursive_mutex> lock(A3dNativeResourceMutex());
    if (m_nativeRetired) {
        return E_ABORT;
    }
    if (dwReserved || dwPriority || dwFlags & ~DSBPLAY_LOOPING || !m_nativeSamples) {
        return E_INVALIDARG;
    }
    if (m_dwReadPosition == m_Desc.dwBufferBytes) {
        m_dwOverridePosition = 0;
    }
    m_dwStatus = A3D_BUFFER_STATE_PLAYING;
    if (dwFlags & DSBPLAY_LOOPING) {
        m_dwFlags |= A3D_RENDER_LOOPING;
    } else {
        m_dwFlags &= ~A3D_RENDER_LOOPING;
    }
    return S_OK;
#else
	if ( dwReserved || dwPriority )
		return DSERR_INVALIDPARAM;

	if ( m_dwStatus != A3D_BUFFER_STATE_PLAYING && m_dwBufferKind == 1 )
	{
		ResMan* pOwner = (ResMan*)(IDirectSound*)m_pOwner;
		DWORD dwStreams = pOwner->GetSoftwareStreamCount();
		DWORD dwPlaying = 0;
		A3dTickMilliseconds();
		if ( dwStreams )
			dwPlaying = pOwner->CountPlayingSoftwareVoices();
		if ( dwPlaying < dwStreams )
		{
			dwStreams += pOwner->GetFreeSoftwareVoices();
			if ( dwPlaying > dwStreams )
				return E_FAIL;
		}
	}

	m_dwStatus = A3D_BUFFER_STATE_PLAYING;
	if ( dwFlags & DSBPLAY_LOOPING )
		m_dwFlags |= A3D_RENDER_LOOPING;
	else
		m_dwFlags &= ~A3D_RENDER_LOOPING;

	return DS_OK;
#endif
}

HRESULT STDMETHODCALLTYPE ResManBuffer::SetVolume( LONG lVolume )
{
#ifndef _WIN32
    std::lock_guard<std::recursive_mutex> lock(A3dNativeResourceMutex());
#endif
	if ( lVolume < DSBVOLUME_MIN || lVolume > DSBVOLUME_MAX )
		return DSERR_INVALIDPARAM;

	m_lVolume = lVolume;
	return DS_OK;
}

HRESULT STDMETHODCALLTYPE ResManBuffer::SetPan( LONG lPan )
{
#ifndef _WIN32
    std::lock_guard<std::recursive_mutex> lock(A3dNativeResourceMutex());
#endif
	if ( lPan < DSBPAN_LEFT || lPan > DSBPAN_RIGHT )
		return DSERR_INVALIDPARAM;

	m_lPan = lPan;
	return DS_OK;
}

HRESULT STDMETHODCALLTYPE ResManBuffer::SetFrequency( DWORD dwFrequency )
{
#ifndef _WIN32
    std::lock_guard<std::recursive_mutex> lock(A3dNativeResourceMutex());
#endif
	if ( ( dwFrequency < DSBFREQUENCY_MIN || dwFrequency > A3D_PLATFORM_MAX_FREQUENCY ) && dwFrequency )
		return DSERR_INVALIDPARAM;

	if ( dwFrequency == DSBFREQUENCY_ORIGINAL )
		m_dwFrequency = m_dwOriginalFrequency;
	else
		m_dwFrequency = dwFrequency;

	return DS_OK;
}

HRESULT STDMETHODCALLTYPE ResManBuffer::Stop()
{
#ifndef _WIN32
    A3dNativeNotificationBatch batch;
    {
        std::lock_guard<std::recursive_mutex> lock(A3dNativeResourceMutex());
        CaptureNativeStop(batch);
    }
    batch.Dispatch();
    return S_OK;
#else
	m_dwStatus = A3D_BUFFER_STATE_STOPPED;
	return DS_OK;
#endif
}

HRESULT ResManBuffer::SetStream( DalBufferInfo* pStream )
{
#ifndef _WIN32
    std::lock_guard<std::recursive_mutex> lock(A3dNativeResourceMutex());
    if (pStream) {
        IDirectSoundBuffer* output = pStream->GetDSBuffer();
        HRESULT result = output->SetFrequency(m_dwFrequency);
        if (SUCCEEDED(result) && m_nativeHasControl) {
            IA3dDalBuffer* render = pStream->GetIDalBuffer();
            result = render ? render->SetA3dSuperCtrl(&m_nativeControl, 0) : E_NOINTERFACE;
        }
        if (FAILED(result)) {
            return result;
        }
    }
    m_pStream = pStream;
    return S_OK;
#else
	m_pStream = pStream;
	return DS_OK;
#endif
}

HRESULT ResManBuffer::Duplicate( const ResManBuffer* pBuffer )
{
#ifndef _WIN32
    if (!pBuffer || !pBuffer->m_nativeSamples || m_nativeSamples) {
        return E_INVALIDARG;
    }
    std::lock_guard<std::recursive_mutex> lock(A3dNativeResourceMutex());
    m_nativeFormat = pBuffer->m_nativeFormat;
    m_Desc = pBuffer->m_Desc;
    m_Desc.lpwfxFormat = &m_nativeFormat;
    m_nativeSamples = pBuffer->m_nativeSamples;
    ++m_nativeSamples->references;
    m_hData = m_nativeSamples->data;
    m_nativeControl = pBuffer->m_nativeControl;
    m_nativeHasControl = pBuffer->m_nativeHasControl;
    m_pRenderData = m_nativeHasControl ? &m_nativeControl : nullptr;
    m_nativeSpatial = pBuffer->m_nativeSpatial;
    m_lVolume = pBuffer->m_lVolume;
    m_lPan = pBuffer->m_lPan;
    m_dwControl = pBuffer->m_dwControl;
    m_dwFrequency = pBuffer->m_dwFrequency;
    m_dwOriginalFrequency = pBuffer->m_dwOriginalFrequency;
    m_dwFlags = pBuffer->m_dwFlags & ~A3D_RENDER_LOOPING;
    return S_OK;
#else
	if ( pBuffer )
	{
		m_Desc = pBuffer->m_Desc;
		m_pBufferToken = pBuffer->m_pBufferToken;
		m_hData = pBuffer->m_hData;
		m_dwFlags = pBuffer->m_dwFlags;
	}

	return DS_OK;
#endif
}

HRESULT ResManBuffer::Update( DWORD dwTime )
{
#ifndef _WIN32
    A3dNativeNotificationBatch batch;
    HRESULT result;
    {
        std::lock_guard<std::recursive_mutex> lock(A3dNativeResourceMutex());
        result = CaptureNativeUpdate(dwTime, batch);
    }
    batch.Dispatch();
    return result;
#else
	if ( !m_pStream )
		return E_ABORT;

	if ( m_dwOverridePosition != A3D_BUFFER_POSITION_UNSET )
	{
		m_dwReadPosition = m_dwOverridePosition;
		m_dwOverridePosition = A3D_BUFFER_POSITION_UNSET;
	}

	DWORD dwSoundStatus;
	m_pStream->GetDSBuffer()->GetStatus( &dwSoundStatus );
	if ( !( dwSoundStatus & DSBSTATUS_PLAYING ) )
		m_pStream->Reset();

	DWORD dwAvailable;
	if ( m_pStream->Refresh( &dwAvailable ) )
	{
		[[maybe_unused]] DWORD dwPreviousPosition = m_dwReadPosition;
		DWORD dwPosition = m_dwReadPosition;
		m_pStream->Write( dwTime, (const BYTE*)m_hData, m_Desc.dwBufferBytes, &dwPosition, m_dwFlags );
		m_dwReadPosition = dwPosition;

#ifdef _WIN32
		if ( m_hNotificationMutex )
		{
			WaitForSingleObject( m_hNotificationMutex, INFINITE );
			for ( DWORD i = 0; i < m_dwNotificationCount; i++ )
			{
				DWORD dwOffset = m_pNotifications[i].dwOffset;
				if ( dwOffset >= dwPreviousPosition && dwOffset < dwPosition )
					SetEvent( m_pNotifications[i].hEventNotify );
			}
			ReleaseMutex( m_hNotificationMutex );
		}
#endif

		if ( m_dwReadPosition >= m_Desc.dwBufferBytes && !( m_dwFlags & A3D_RENDER_LOOPING ) &&
			!( dwSoundStatus & DSBSTATUS_PLAYING ) )
		{
			Stop();
			SetCurrentPosition( 0 );
		}

		if ( m_dwStatus == A3D_BUFFER_STATE_PLAYING && !( dwSoundStatus & DSBSTATUS_PLAYING ) )
			m_pStream->GetDSBuffer()->Play( 0, 0, DSBPLAY_LOOPING );
	}

	return DS_OK;
#endif
}

#define A3D_RENDER_WAIT_MILLISECONDS		10
#define A3D_RENDER_UPDATE_MILLISECONDS	100

[[maybe_unused]] static const IID IID_A3dHardwareBufferControl =
	{ 0x2e56ef65, 0x74ce, 0x11d2, { 0x97, 0x1b, 0x00, 0x60, 0x08, 0x8d, 0x87, 0x88 } };

HRESULT ResMan::StartRenderer( DWORD dwFirst, DWORD dwSecond,
        void* pConfiguration, void* pBackend )
{
#ifndef _WIN32
    if (m_dwInitialized) {
        return E_INVALIDARG;
    }
#endif
	HRESULT hr = InitResMan( dwFirst, dwSecond, pConfiguration, pBackend );
	if ( FAILED( hr ) )
		return hr;

	m_pA3dBackend->SetResourceManagerMode( A3D_RESOURCE_MODE_OFF );
	m_dwWaitTime = A3D_RENDER_WAIT_MILLISECONDS;
	m_dwUpdateInterval = A3D_RENDER_UPDATE_MILLISECONDS;
#ifdef _WIN32
	m_hUpdateEvent = CreateEventA( NULL, FALSE, FALSE, NULL );
	if ( !m_hUpdateEvent )
		return E_FAIL;

	m_hThread = CreateThread( NULL, 0, RenderThread, this, 0, &m_dwThreadId );
	if ( !m_hThread )
	{
		CloseHandle( m_hUpdateEvent );
		m_hUpdateEvent = INVALID_HANDLE_VALUE;
		return E_FAIL;
	}

#else
    hr = m_nativeWorker.Start(RenderThread, this);
    if (FAILED(hr)) {
        return hr;
    }
#endif

	m_dwInitialized = TRUE;
	return S_OK;
}

DWORD WINAPI ResMan::RenderThread( LPVOID pContext )
{
	ResMan* pAudio = (ResMan*)pContext;
#ifdef _WIN32
	WaitForSingleObject( pAudio->m_hUpdateEvent, pAudio->m_dwWaitTime );
	for ( ;; )
#else
    while (pAudio->m_nativeWorker.Wait(pAudio->m_dwWaitTime))
#endif
	{
		DWORD dwTime = A3dTickMilliseconds();
#ifndef _WIN32
        pAudio->TickNative(dwTime);
#else
		if ( dwTime - pAudio->m_dwPreviousUpdate > pAudio->m_dwUpdateInterval )
		{
			pAudio->m_dwPreviousUpdate = dwTime;
			pAudio->FreeReleasedStreams( dwTime );
			pAudio->HaltStoppedStreams( dwTime );
			pAudio->EnsureStreamCapacity( dwTime );
			pAudio->BuildPendingBuffers( dwTime );
			pAudio->AssignStreams( dwTime );
			pAudio->m_pDefaultPool->Update( dwTime );
			pAudio->m_pHrtfPool->Update( dwTime );
			pAudio->m_pSecondaryPool->Update( dwTime );
			pAudio->DebugBufferCounts();
		}
		pAudio->ProcessPlayingBuffers( dwTime );
#endif
#ifdef _WIN32
		WaitForSingleObject( pAudio->m_hUpdateEvent, pAudio->m_dwWaitTime );
#endif
	}
#ifndef _WIN32
    return 0;
#endif
}

HRESULT ResMan::DetachStream( DWORD dwTime, ResManBuffer* pBuffer,
		DalBufferInfo* pStream )
{
	pStream->GetDSBuffer()->Stop();
	pStream->GetDSBuffer()->SetCurrentPosition( 0 );
	pStream->m_dwUnusedSince = dwTime;
	pStream->m_dwState = 1;
	pStream->m_pOwner = NULL;
	pBuffer->SetStream( NULL );
	return S_OK;
}

HRESULT ResMan::EnsurePoolCapacity( DWORD dwRequired, LPDWORD pdwCommitted,
		DalInfo* pPool )
{
#ifndef _WIN32
    if (!pdwCommitted || !pPool) {
        return E_POINTER;
    }
    const DWORD allocated = static_cast<DWORD>(pPool->m_Streams.cStreams);
    const DWORD committed = *pdwCommitted;
    if (dwRequired > committed + allocated) {
        const DWORD needed = std::min(dwRequired - committed - allocated, pPool->GetNumAvailable());
        for (DWORD index = 0; index < needed; ++index) {
            const HRESULT result = pPool->CreateDalBuffer(nullptr);
            if (FAILED(result)) {
                return result;
            }
        }
    }
    *pdwCommitted += static_cast<DWORD>(pPool->m_Streams.cStreams);
    return S_OK;
#else
	DWORD dwCommitted = *pdwCommitted;
	DWORD dwPoolStreams = pPool->m_Streams.cStreams;
	HRESULT hr = S_OK;
	if ( dwRequired > dwCommitted + dwPoolStreams )
	{
		DWORD dwNeeded = dwRequired - dwPoolStreams - dwCommitted;
		for ( DWORD i = 0; i < dwNeeded; i++ )
		{
			hr = pPool->CreateDalBuffer( NULL );
			if ( FAILED( hr ) )
				break;
		}
	}

	*pdwCommitted += pPool->m_Streams.cStreams;
	return hr;
#endif
}

HRESULT ResMan::CommitStreams( DWORD dwTime, DalInfo* pPool,
		LPLONG plRemaining, a3d_platform_buffer_node_t** ppNext )
{
	for ( DWORD i = 0; i < (DWORD)pPool->m_Streams.cStreams; i++ )
	{
		LONG nRemaining = (LONG)( (DWORD)*plRemaining - 1 );
		*plRemaining = nRemaining;
		if ( nRemaining < 0 )
			break;

		a3d_platform_buffer_node_t* pNode = *ppNext;
		*ppNext = pNode->pNext;
		ResManBuffer* pBuffer = pNode->pBuffer;
		DalBufferInfo* pStream;
		pBuffer->GetRenderStream( &pStream );
		if ( pStream && pStream->m_pPool == pPool )
			continue;
		if ( pStream )
			DetachStream( dwTime, pBuffer, pStream );

		pPool->FindUnused( &pStream );
		if ( !pStream )
		{
			ResManBuffer* pPreviousOwner = NULL;
			pNode = m_PendingBuffers.pTail;
			while ( pNode )
			{
				ResManBuffer* pOther = pNode->pBuffer;
				pNode = pNode->pPrevious;
				DalBufferInfo* pOtherStream;
				pOther->GetRenderStream( &pOtherStream );
				if ( pOtherStream && pOtherStream->m_pPool == pPool )
				{
					pPreviousOwner = pOther;
					pStream = pOtherStream;
#ifndef _WIN32
                    break;
#endif
				}
			}

			if ( !pPreviousOwner || !pStream )
				break;
			DetachStream( dwTime, pPreviousOwner, pStream );
		}

		if ( pStream )
		{
#ifndef _WIN32
            const HRESULT result = pBuffer->SetStream(pStream);
            if (FAILED(result)) {
                return result;
            }
            pStream->m_pOwner = pBuffer;
            pStream->m_dwUnusedSince = dwTime;
#else
			pStream->m_pOwner = pBuffer;
			pStream->m_dwUnusedSince = dwTime;
			pBuffer->SetStream( pStream );
#endif
		}
	}

	return S_OK;
}

HRESULT STDMETHODCALLTYPE ResMan::DuplicateSoundBuffer( IDirectSoundBuffer* pBuffer,
		IDirectSoundBuffer** ppDuplicate )
{
#ifndef _WIN32
    std::lock_guard<std::recursive_mutex> admission(m_nativeAdmission);
    if (!ppDuplicate) {
        return E_POINTER;
    }
    *ppDuplicate = nullptr;
    if (!pBuffer || !m_nativeAttached) {
        return E_INVALIDARG;
    }
    auto* original = dynamic_cast<ResManStreamBuffer*>(pBuffer);
    if (!original) {
        IDirectSound* leaf = nullptr;
        HRESULT result = GetDS(&leaf);
        if (FAILED(result)) {
            return result;
        }
        result = leaf->DuplicateSoundBuffer(pBuffer, ppDuplicate);
        if (SUCCEEDED(result)) {
            if (auto* native = dynamic_cast<A3dNativeBuffer*>(*ppDuplicate)) {
                native->RetainNativeOwner(static_cast<IDirectSound*>(this));
            }
        }
        leaf->Release();
        return result;
    }
    std::lock_guard<std::recursive_mutex> lock(A3dNativeResourceMutex());
    bool found = false;
    for (auto* node = m_SoftwareBuffers.pHead; node; node = node->pNext) {
        found |= node->pBuffer == original;
    }
    if (!found) {
        return E_INVALIDARG;
    }
    auto* duplicate = new (std::nothrow) ResManStreamBuffer(static_cast<IDirectSound*>(this),
        m_dwResourceManagerMode);
    if (!duplicate) {
        return E_OUTOFMEMORY;
    }
    HRESULT result = duplicate->Duplicate(original);
    auto* node = SUCCEEDED(result) ? new (std::nothrow) a3d_platform_buffer_node_t{} : nullptr;
    if (!node) {
        delete duplicate;
        return FAILED(result) ? result : E_OUTOFMEMORY;
    }
    node->pBuffer = duplicate;
    node->pPrevious = m_SoftwareBuffers.pTail;
    if (node->pPrevious) {
        node->pPrevious->pNext = node;
    } else {
        m_SoftwareBuffers.pHead = node;
    }
    m_SoftwareBuffers.pTail = node;
    ++m_SoftwareBuffers.cBuffers;
    duplicate->AddRef();
    *ppDuplicate = static_cast<IDirectSoundBuffer*>(duplicate);
    return S_OK;
#else
	IUnknown* pHardwareControl = NULL;
	IUnknown* pSoftwareControl = NULL;
	pBuffer->QueryInterface( IID_A3dHardwareBufferControl, (void**)&pHardwareControl );
	pBuffer->QueryInterface( IID_A3dSoftwareBufferControl, (void**)&pSoftwareControl );

	if ( pHardwareControl )
	{
		ResManStatBuffer* pDuplicate = new ( std::nothrow ) ResManStatBuffer(
			(IDirectSound*)this, m_dwResourceManagerMode );
		if ( !pDuplicate )
		{
			pHardwareControl->Release();
			return E_OUTOFMEMORY;
		}

		DalBufferInfo* pOriginalStream;
		DalBufferInfo* pStream;
		HRESULT hr = pDuplicate->Duplicate( (ResManBuffer*)pBuffer );
		if ( FAILED( hr ) ||
			FAILED( hr = pDuplicate->QueryInterface( IID_IDirectSoundBuffer, (void**)ppDuplicate ) ) ||
			FAILED( hr = ( (ResManBuffer*)pBuffer )->GetRenderStream( &pOriginalStream ) ) ||
			FAILED( hr = m_pDefaultPool->CreateDalBuffer( pOriginalStream, &pStream ) ) )
		{
			delete pDuplicate;
			pHardwareControl->Release();
			return hr;
		}

		pDuplicate->SetStream( pStream );
		a3d_platform_buffer_node_t* pPrevious = m_HardwareBuffers.pTail;
		if ( !m_HardwareBuffers.pFreeNodes )
		{
			CPlex* pBlock = CPlex::Create(
				(CPlex**)&m_HardwareBuffers.pBlocks, m_HardwareBuffers.cBlockNodes,
				sizeof( a3d_platform_buffer_node_t ) );
			a3d_platform_buffer_node_t* pNodes = (a3d_platform_buffer_node_t*)( pBlock + 1 );
			for ( LONG i = m_HardwareBuffers.cBlockNodes - 1; i >= 0; i-- )
			{
				pNodes[i].pNext = m_HardwareBuffers.pFreeNodes;
				m_HardwareBuffers.pFreeNodes = &pNodes[i];
			}
		}

		a3d_platform_buffer_node_t* pNode = m_HardwareBuffers.pFreeNodes;
		m_HardwareBuffers.pFreeNodes = pNode->pNext;
		pNode->pPrevious = pPrevious;
		pNode->pNext = NULL;
		m_HardwareBuffers.cBuffers++;
		pNode->pBuffer = pDuplicate;
		a3d_platform_buffer_node_t* pTail = m_HardwareBuffers.pTail;
		if ( pTail )
			pTail->pNext = pNode;
		else
			m_HardwareBuffers.pHead = pNode;
		m_HardwareBuffers.pTail = pNode;
		pHardwareControl->Release();
	}

	if ( pSoftwareControl )
	{
		ResManStreamBuffer* pDuplicate = new ( std::nothrow ) ResManStreamBuffer(
			(IDirectSound*)this, m_dwResourceManagerMode );
		if ( !pDuplicate )
		{
			pSoftwareControl->Release();
			return E_OUTOFMEMORY;
		}

		HRESULT hr = pDuplicate->Duplicate( (ResManBuffer*)pBuffer );
		if ( SUCCEEDED( hr ) )
			hr = pDuplicate->QueryInterface( IID_IDirectSoundBuffer, (void**)ppDuplicate );
		if ( FAILED( hr ) )
		{
			delete pDuplicate;
			pSoftwareControl->Release();
			return hr;
		}

		a3d_platform_buffer_node_t* pPrevious = m_SoftwareBuffers.pTail;
		if ( !m_SoftwareBuffers.pFreeNodes )
		{
			CPlex* pBlock = CPlex::Create(
				(CPlex**)&m_SoftwareBuffers.pBlocks, m_SoftwareBuffers.cBlockNodes,
				sizeof( a3d_platform_buffer_node_t ) );
			a3d_platform_buffer_node_t* pNodes = (a3d_platform_buffer_node_t*)( pBlock + 1 );
			for ( LONG i = m_SoftwareBuffers.cBlockNodes - 1; i >= 0; i-- )
			{
				pNodes[i].pNext = m_SoftwareBuffers.pFreeNodes;
				m_SoftwareBuffers.pFreeNodes = &pNodes[i];
			}
		}

		a3d_platform_buffer_node_t* pNode = m_SoftwareBuffers.pFreeNodes;
		m_SoftwareBuffers.pFreeNodes = pNode->pNext;
		pNode->pPrevious = pPrevious;
		pNode->pNext = NULL;
		m_SoftwareBuffers.cBuffers++;
		pNode->pBuffer = pDuplicate;
		a3d_platform_buffer_node_t* pTail = m_SoftwareBuffers.pTail;
		if ( pTail )
			pTail->pNext = pNode;
		else
			m_SoftwareBuffers.pHead = pNode;
		m_SoftwareBuffers.pTail = pNode;
		pSoftwareControl->Release();
	}

	return DS_OK;
#endif
}

HRESULT ResMan::CreateSecondaryBuffer( a3d_platform_buffer_desc_t* pDesc,
		IDirectSoundBuffer** ppBuffer, IUnknown* )
{
#ifndef _WIN32
    if (!ppBuffer) {
        return E_POINTER;
    }
    *ppBuffer = nullptr;
    auto* buffer = new (std::nothrow) ResManStreamBuffer(static_cast<IDirectSound*>(this),
        m_dwResourceManagerMode);
    if (!buffer) {
        return E_OUTOFMEMORY;
    }
    void* token = malloc(sizeof(DWORD));
    HRESULT result = token ? buffer->Init(pDesc, token) : E_OUTOFMEMORY;
    if (FAILED(result)) {
        free(token);
        delete buffer;
        return result;
    }
    auto* node = new (std::nothrow) a3d_platform_buffer_node_t{};
    if (!node) {
        delete buffer;
        return E_OUTOFMEMORY;
    }
    {
        std::lock_guard<std::recursive_mutex> lock(A3dNativeResourceMutex());
        node->pBuffer = buffer;
        node->pPrevious = m_SoftwareBuffers.pTail;
        if (node->pPrevious) {
            node->pPrevious->pNext = node;
        } else {
            m_SoftwareBuffers.pHead = node;
        }
        m_SoftwareBuffers.pTail = node;
        ++m_SoftwareBuffers.cBuffers;
        buffer->AddRef();
        *ppBuffer = static_cast<IDirectSoundBuffer*>(buffer);
    }
    return S_OK;
#else
	if ( m_dwResourceManagerMode != A3D_RESOURCE_MODE_OFF )
	{
		void* pBufferToken = malloc( sizeof( DWORD ) );
		if ( !pBufferToken )
			return E_OUTOFMEMORY;

		ResManStreamBuffer* pBuffer = new ( std::nothrow ) ResManStreamBuffer(
			(IDirectSound*)this, m_dwResourceManagerMode );
		if ( !pBuffer )
		{
			free( pBufferToken );
			return E_OUTOFMEMORY;
		}

		HRESULT hr = pBuffer->Init( pDesc, pBufferToken );
		if ( SUCCEEDED( hr ) )
			hr = pBuffer->QueryInterface( IID_IDirectSoundBuffer, (void**)ppBuffer );
		if ( FAILED( hr ) )
		{
			delete pBuffer;
			free( pBufferToken );
			return hr;
		}

		a3d_platform_buffer_node_t* pPrevious = m_SoftwareBuffers.pTail;
		if ( !m_SoftwareBuffers.pFreeNodes )
		{
			CPlex* pBlock = CPlex::Create(
				(CPlex**)&m_SoftwareBuffers.pBlocks, m_SoftwareBuffers.cBlockNodes,
				sizeof( a3d_platform_buffer_node_t ) );
			a3d_platform_buffer_node_t* pNodes = (a3d_platform_buffer_node_t*)( pBlock + 1 );
			for ( LONG i = m_SoftwareBuffers.cBlockNodes - 1; i >= 0; i-- )
			{
				pNodes[i].pNext = m_SoftwareBuffers.pFreeNodes;
				m_SoftwareBuffers.pFreeNodes = &pNodes[i];
			}
		}

		a3d_platform_buffer_node_t* pNode = m_SoftwareBuffers.pFreeNodes;
		m_SoftwareBuffers.pFreeNodes = pNode->pNext;
		pNode->pPrevious = pPrevious;
		pNode->pNext = NULL;
		m_SoftwareBuffers.cBuffers++;
		pNode->pBuffer = pBuffer;
		a3d_platform_buffer_node_t* pTail = m_SoftwareBuffers.pTail;
		if ( pTail )
			pTail->pNext = pNode;
		else
			m_SoftwareBuffers.pHead = pNode;
		m_SoftwareBuffers.pTail = pNode;
	}
	else
	{
		void* pBufferToken = malloc( sizeof( DWORD ) );
		if ( !pBufferToken )
			return E_OUTOFMEMORY;

		ResManStatBuffer* pBuffer = new ( std::nothrow ) ResManStatBuffer(
			(IDirectSound*)this, m_dwResourceManagerMode );
		if ( !pBuffer )
		{
			free( pBufferToken );
			return E_OUTOFMEMORY;
		}

		HRESULT hr = pBuffer->Init( pDesc, pBufferToken );
		if ( SUCCEEDED( hr ) )
			hr = pBuffer->QueryInterface( IID_IDirectSoundBuffer, (void**)ppBuffer );
		if ( FAILED( hr ) )
		{
			delete pBuffer;
			free( pBufferToken );
			return hr;
		}

		DalBufferInfo* pStream;
		hr = m_pDefaultPool->CreateStaticDalBuffer( pDesc, &pStream );
		if ( FAILED( hr ) )
		{
			delete pBuffer;
			free( pBufferToken );
			return hr;
		}

		pBuffer->SetStream( pStream );
		a3d_platform_buffer_node_t* pPrevious = m_HardwareBuffers.pTail;
		if ( !m_HardwareBuffers.pFreeNodes )
		{
			CPlex* pBlock = CPlex::Create(
				(CPlex**)&m_HardwareBuffers.pBlocks, m_HardwareBuffers.cBlockNodes,
				sizeof( a3d_platform_buffer_node_t ) );
			a3d_platform_buffer_node_t* pNodes = (a3d_platform_buffer_node_t*)( pBlock + 1 );
			for ( LONG i = m_HardwareBuffers.cBlockNodes - 1; i >= 0; i-- )
			{
				pNodes[i].pNext = m_HardwareBuffers.pFreeNodes;
				m_HardwareBuffers.pFreeNodes = &pNodes[i];
			}
		}

		a3d_platform_buffer_node_t* pNode = m_HardwareBuffers.pFreeNodes;
		m_HardwareBuffers.pFreeNodes = pNode->pNext;
		pNode->pPrevious = pPrevious;
		pNode->pNext = NULL;
		m_HardwareBuffers.cBuffers++;
		pNode->pBuffer = pBuffer;
		a3d_platform_buffer_node_t* pTail = m_HardwareBuffers.pTail;
		if ( pTail )
			pTail->pNext = pNode;
		else
			m_HardwareBuffers.pHead = pNode;
		m_HardwareBuffers.pTail = pNode;
	}

	return DS_OK;
#endif
}

#define A3D_PLATFORM_RENDER_BUFFER_BYTES		88200
#define A3D_PLATFORM_RENDER_SAMPLE_RATE		44100
#define A3D_PLATFORM_RENDER_SAMPLE_BITS		16
#define A3D_PLATFORM_RENDER_UPDATE_THRESHOLD	44100

[[maybe_unused]] static const CLSID CLSID_A3dPlatformBackend =
	{ 0x442d12a1, 0x2641, 0x11d2, { 0x90, 0xfb, 0x00, 0x60, 0x08, 0xa1, 0xf4, 0x41 } };

HRESULT ResMan::InitResMan( DWORD dwFirst, DWORD dwSecond,
		void* pConfiguration, void* pBackend )
{
#ifndef _WIN32
    (void)dwFirst;
    (void)dwSecond;
    (void)pConfiguration;
    (void)pBackend;
    return E_NOTIMPL;
#else
	if ( m_pDefaultPool )
		return E_FAIL;
	if ( m_pHrtfPool )
		return E_FAIL;
	if ( m_pSecondaryPool )
		return E_FAIL;

	CoInitialize( NULL );
	HRESULT hr = CoCreateInstance( CLSID_A3dPlatformBackend, NULL, CLSCTX_INPROC_SERVER,
		IID_IA3d2, (void**)&m_pA3dBackend );
	if ( FAILED( hr ) )
		return hr;

	IA3dDal* pDeviceBackend;
	hr = m_pA3dBackend->QueryInterface( IID_A3dPlatformDeviceBackend, (void**)&pDeviceBackend );
	if ( FAILED( hr ) )
		return hr;
	hr = pDeviceBackend->InitializeEx( dwFirst, dwSecond, pConfiguration, pBackend );
	if ( FAILED( hr ) )
		return hr;

	a3d_render_description_t description = { 1, A3D_PLATFORM_RENDER_BUFFER_BYTES,
		A3D_PLATFORM_RENDER_SAMPLE_RATE, A3D_PLATFORM_RENDER_SAMPLE_BITS,
		A3D_PLATFORM_RENDER_UPDATE_THRESHOLD };
	m_pDefaultPool = new ( std::nothrow ) DalInfo( pDeviceBackend, &description );
	if ( !m_pDefaultPool )
		return E_OUTOFMEMORY;
	m_pDefaultPool->RefreshCapacity();

	DAL_A2D* pMixer = new ( std::nothrow ) DAL_A2D;
	if ( !pMixer )
		return E_OUTOFMEMORY;
	IDirectSound* pDirectSound = NULL;
	m_pDefaultPool->GetIDal()->GetDS( &pDirectSound );
	pMixer->Init( pDirectSound );

	description = { 1, A3D_PLATFORM_RENDER_BUFFER_BYTES, A3D_PLATFORM_RENDER_SAMPLE_RATE,
		A3D_PLATFORM_RENDER_SAMPLE_BITS, A3D_PLATFORM_RENDER_UPDATE_THRESHOLD };
	m_pHrtfPool = new ( std::nothrow ) DalInfo( (IA3dDal*)pMixer, &description );
	if ( !m_pHrtfPool )
		return E_OUTOFMEMORY;
	m_pHrtfPool->RefreshCapacity();

	DAL_EMU* pTiming = new ( std::nothrow ) DAL_EMU;
	if ( !pTiming )
		return E_OUTOFMEMORY;
	description = { 0, A3D_PLATFORM_RENDER_BUFFER_BYTES, A3D_PLATFORM_RENDER_SAMPLE_RATE,
		A3D_PLATFORM_RENDER_SAMPLE_BITS, A3D_PLATFORM_RENDER_UPDATE_THRESHOLD };
	m_pSecondaryPool = new ( std::nothrow ) DalInfo( (IA3dDal*)pTiming, &description );
	if ( !m_pSecondaryPool )
		return E_OUTOFMEMORY;
	m_pSecondaryPool->RefreshCapacity();
	return S_OK;
#endif
}

void ResMan::DebugBufferCounts()
{
	static DWORD s_dwUpdateCount;
	if ( ++s_dwUpdateCount != 20 )
		return;

	s_dwUpdateCount = 0;
	LONG cSecondaryStreams = m_pSecondaryPool->m_Streams.cStreams;
	LONG cHardwareBuffers = m_HardwareBuffers.cBuffers;
	LONG cSoftwareBuffers = m_SoftwareBuffers.cBuffers;
	LONG cHrtfStreams = m_pHrtfPool->m_Streams.cStreams;
	LONG cDefaultStreams = m_pDefaultPool->m_Streams.cStreams;
	DWORD dwPlaying = CountPlayingBuffers();
	char szCounts[256];
	sprintf( szCounts, "Buffer Counts: STRM=%d STAT=%d P=%d W95=%d A2D=%d EMU=%d\n",
		cSoftwareBuffers, cHardwareBuffers, (LONG)dwPlaying, cDefaultStreams, cHrtfStreams, cSecondaryStreams );
#ifdef _WIN32
	OutputDebugStringA( szCounts );
#else
    fputs(szCounts, stderr);
#endif
}

HRESULT ResMan::FreeReleasedStreams( DWORD dwTime )
{
#ifndef _WIN32
    (void)dwTime;
    // Native final Release removes the weak list entry synchronously on the
    // releasing caller, so no worker can own the last parent reference.
    return S_OK;
#else
	DWORD dwState;
	a3d_platform_buffer_node_t* pNode = m_HardwareBuffers.pHead;
	while ( pNode != NULL )
	{
		a3d_platform_buffer_node_t* pNext = pNode->pNext;
		ResManBuffer* pBuffer = pNode->pBuffer;
		( (IA3dPlatformBufferControl*)pBuffer )->GetBufferState( &dwState );
		if ( dwState == A3D_BUFFER_STATE_RELEASED )
		{
			if ( pNode == m_HardwareBuffers.pHead )
				m_HardwareBuffers.pHead = pNode->pNext;
			else
				pNode->pPrevious->pNext = pNode->pNext;

			if ( pNode == m_HardwareBuffers.pTail )
				m_HardwareBuffers.pTail = pNode->pPrevious;
			else
				pNode->pNext->pPrevious = pNode->pPrevious;

			pNode->pNext = m_HardwareBuffers.pFreeNodes;
			m_HardwareBuffers.pFreeNodes = pNode;
			if ( --m_HardwareBuffers.cBuffers == 0 )
			{
				m_HardwareBuffers.cBuffers = 0;
				m_HardwareBuffers.pFreeNodes = NULL;
				m_HardwareBuffers.pTail = NULL;
				m_HardwareBuffers.pHead = NULL;
				CPlex* pBlocksToFree = (CPlex*)m_HardwareBuffers.pBlocks;
				if ( pBlocksToFree )
					pBlocksToFree->FreeDataChain();
				m_HardwareBuffers.pBlocks = NULL;
			}

			if ( pBuffer != NULL )
			{
				( (ResManStatBuffer*)pBuffer )->~ResManStatBuffer();
				::operator delete( pBuffer );
			}
		}

		pNode = pNext;
	}

	DalBufferInfo* pStream = NULL;
	pNode = m_SoftwareBuffers.pHead;
	while ( pNode != NULL )
	{
		a3d_platform_buffer_node_t* pNext = pNode->pNext;
		ResManBuffer* pBuffer = pNode->pBuffer;
		( (IA3dPlatformBufferControl*)pBuffer )->GetBufferState( &dwState );
		if ( dwState == A3D_BUFFER_STATE_RELEASED )
		{
			pBuffer->GetRenderStream( &pStream );
			if ( pStream != NULL )
				DetachStream( dwTime, pBuffer, pStream );

			if ( pNode == m_SoftwareBuffers.pHead )
				m_SoftwareBuffers.pHead = pNode->pNext;
			else
				pNode->pPrevious->pNext = pNode->pNext;

			if ( pNode == m_SoftwareBuffers.pTail )
				m_SoftwareBuffers.pTail = pNode->pPrevious;
			else
				pNode->pNext->pPrevious = pNode->pPrevious;

			pNode->pNext = m_SoftwareBuffers.pFreeNodes;
			m_SoftwareBuffers.pFreeNodes = pNode;
			if ( --m_SoftwareBuffers.cBuffers == 0 )
			{
				m_SoftwareBuffers.cBuffers = 0;
				m_SoftwareBuffers.pFreeNodes = NULL;
				m_SoftwareBuffers.pTail = NULL;
				m_SoftwareBuffers.pHead = NULL;
				CPlex* pBlocksToFree = (CPlex*)m_SoftwareBuffers.pBlocks;
				if ( pBlocksToFree )
					pBlocksToFree->FreeDataChain();
				m_SoftwareBuffers.pBlocks = NULL;
			}

			if ( pBuffer != NULL )
			{
				( (ResManStreamBuffer*)pBuffer )->~ResManStreamBuffer();
				::operator delete( pBuffer );
			}
		}

		pNode = pNext;
	}

	return S_OK;
#endif
}

HRESULT ResMan::HaltStoppedStreams( DWORD dwTime )
{
	a3d_platform_buffer_node_t* pNode = m_SoftwareBuffers.pHead;
	while ( pNode != NULL )
	{
		ResManBuffer* pBuffer = pNode->pBuffer;
		pNode = pNode->pNext;
		DWORD dwState;
		( (IA3dPlatformBufferControl*)pBuffer )->GetBufferState( &dwState );
		if ( dwState == A3D_BUFFER_STATE_STOPPED )
		{
			DalBufferInfo* pStream;
			pBuffer->GetRenderStream( &pStream );
			if ( pStream != NULL )
				DetachStream( dwTime, pBuffer, pStream );
		}
	}

	return S_OK;
}

HRESULT ResMan::BuildPendingBuffers( DWORD dwTime )
{
#ifndef _WIN32
    (void)dwTime;
    a3d_platform_buffer_list_t pending{};
    auto clear = [](a3d_platform_buffer_list_t& list) {
        while (list.pHead) {
            auto* next = list.pHead->pNext;
            delete list.pHead;
            list.pHead = next;
        }
        list = {};
    };
    for (auto* node = m_SoftwareBuffers.pHead; node; node = node->pNext) {
        DWORD state = 0;
        node->pBuffer->GetBufferState(&state);
        if (state != A3D_BUFFER_STATE_PLAYING) {
            continue;
        }
        auto* entry = new (std::nothrow) a3d_platform_buffer_node_t{};
        if (!entry) {
            clear(pending);
            return E_OUTOFMEMORY;
        }
        entry->pBuffer = node->pBuffer;
        DWORD priority = 0;
        DWORD status = 0;
        entry->pBuffer->GetPriority(&priority);
        entry->pBuffer->GetStatus(&status);
        auto* before = pending.pHead;
        while (before) {
            DWORD otherPriority = 0;
            DWORD otherStatus = 0;
            before->pBuffer->GetPriority(&otherPriority);
            before->pBuffer->GetStatus(&otherStatus);
            const bool loopers = m_dwResourceManagerMode == A3D_RESOURCE_MODE_DYNAMIC_LOOPERS;
            const bool looping = (status & DSBSTATUS_LOOPING) != 0;
            const bool otherLooping = (otherStatus & DSBSTATUS_LOOPING) != 0;
            if ((loopers && looping != otherLooping) ? looping : priority > otherPriority) {
                break;
            }
            before = before->pNext;
        }
        entry->pNext = before;
        entry->pPrevious = before ? before->pPrevious : pending.pTail;
        if (entry->pPrevious) {
            entry->pPrevious->pNext = entry;
        } else {
            pending.pHead = entry;
        }
        if (before) {
            before->pPrevious = entry;
        } else {
            pending.pTail = entry;
        }
        ++pending.cBuffers;
    }
    clear(m_PendingBuffers);
    m_PendingBuffers = pending;
    return S_OK;
#else
	(void)dwTime;

	m_PendingBuffers.cBuffers = 0;
	m_PendingBuffers.pFreeNodes = NULL;
	m_PendingBuffers.pTail = NULL;
	m_PendingBuffers.pHead = NULL;
	CPlex* pBlocksToFree = (CPlex*)m_PendingBuffers.pBlocks;
	if ( pBlocksToFree )
		pBlocksToFree->FreeDataChain();
	m_PendingBuffers.pBlocks = NULL;

	a3d_platform_buffer_node_t* pNode = m_SoftwareBuffers.pHead;
	while ( pNode != NULL )
	{
		ResManBuffer* pBuffer = pNode->pBuffer;
		pNode = pNode->pNext;
		DWORD dwState;
		( (IA3dPlatformBufferControl*)pBuffer )->GetBufferState( &dwState );
		if ( dwState != A3D_BUFFER_STATE_PLAYING )
			continue;

		a3d_platform_buffer_node_t* pTail = m_PendingBuffers.pTail;
		if ( m_PendingBuffers.pFreeNodes == NULL )
		{
			CPlex* pBlock = CPlex::Create(
				(CPlex**)&m_PendingBuffers.pBlocks, m_PendingBuffers.cBlockNodes,
				sizeof( a3d_platform_buffer_node_t ) );
			a3d_platform_buffer_node_t* pNodes = (a3d_platform_buffer_node_t*)( pBlock + 1 );
			for ( LONG i = (LONG)( (DWORD)m_PendingBuffers.cBlockNodes - 1 ); i >= 0; i-- )
			{
				pNodes[i].pNext = m_PendingBuffers.pFreeNodes;
				m_PendingBuffers.pFreeNodes = &pNodes[i];
			}
		}

		a3d_platform_buffer_node_t* pPending = m_PendingBuffers.pFreeNodes;
		m_PendingBuffers.pFreeNodes = pPending->pNext;
		pPending->pPrevious = pTail;
		pPending->pNext = NULL;
		m_PendingBuffers.cBuffers++;
		pPending->pBuffer = pBuffer;
		if ( m_PendingBuffers.pTail != NULL )
			m_PendingBuffers.pTail->pNext = pPending;
		else
			m_PendingBuffers.pHead = pPending;
		m_PendingBuffers.pTail = pPending;
	}

	return S_OK;
#endif
}

HRESULT ResMan::EnsureStreamCapacity( DWORD dwTime )
{
#ifndef _WIN32
    (void)dwTime;
    DWORD allocated = 0;
    return EnsurePoolCapacity(CountPlayingBuffers(), &allocated, m_pDefaultPool);
#else
	(void)dwTime;

	DWORD dwAllocated = 0;
	DWORD dwPlaying = CountPlayingBuffers();
	EnsurePoolCapacity( dwPlaying, &dwAllocated, m_pDefaultPool );
	EnsurePoolCapacity( dwPlaying, &dwAllocated, m_pHrtfPool );
	EnsurePoolCapacity( dwPlaying, &dwAllocated, m_pSecondaryPool );
	return S_OK;
#endif
}

HRESULT ResMan::AssignStreams( DWORD dwTime )
{
#ifndef _WIN32
    LONG remaining = m_PendingBuffers.cBuffers;
    auto* next = m_PendingBuffers.pHead;
    return CommitStreams(dwTime, m_pDefaultPool, &remaining, &next);
#else
	LONG cRemaining = m_PendingBuffers.cBuffers;
	a3d_platform_buffer_node_t* pNext = m_PendingBuffers.pHead;
	CommitStreams( dwTime, m_pDefaultPool, &cRemaining, &pNext );
	CommitStreams( dwTime, m_pHrtfPool, &cRemaining, &pNext );
	CommitStreams( dwTime, m_pSecondaryPool, &cRemaining, &pNext );
	return S_OK;
#endif
}

HRESULT ResMan::ProcessPlayingBuffers( DWORD dwTime )
{
	a3d_platform_buffer_node_t* pNode = m_SoftwareBuffers.pHead;
	while ( pNode != NULL )
	{
		ResManBuffer* pBuffer = pNode->pBuffer;
		pNode = pNode->pNext;
		DWORD dwState;
		( (IA3dPlatformBufferControl*)pBuffer )->GetBufferState( &dwState );
		if ( dwState == A3D_BUFFER_STATE_PLAYING )
			pBuffer->Update( dwTime );
	}

	pNode = m_HardwareBuffers.pHead;
	while ( pNode != NULL )
	{
		ResManStatBuffer* pBuffer = (ResManStatBuffer*)pNode->pBuffer;
		pNode = pNode->pNext;
		DWORD dwState;
		( (IA3dPlatformBufferControl*)pBuffer )->GetBufferState( &dwState );
		if ( dwState == A3D_BUFFER_STATE_PLAYING )
			pBuffer->ProcessNotifications( dwTime );
	}

	return S_OK;
}

#ifndef _WIN32
ResMan::~ResMan() {
    ShutdownNative();
}

void ResMan::StopNativeWorker() noexcept {
    m_nativeWorker.Stop();
}
#endif

#ifndef _WIN32
void ResManBuffer::CaptureNativeStop(A3dNativeNotificationBatch& batch) {
    const bool stopped = m_dwStatus == A3D_BUFFER_STATE_PLAYING;
    m_dwStatus = A3D_BUFFER_STATE_STOPPED;
    m_nativeNotifications.Capture(0, 0, m_Desc.dwBufferBytes, stopped, batch);
}

HRESULT ResManBuffer::CaptureNativeUpdate(DWORD time, A3dNativeNotificationBatch& batch) {
    if (!m_pStream) {
        return E_ABORT;
    }
    if (m_dwStatus != A3D_BUFFER_STATE_PLAYING || (m_nativeSamples && m_nativeSamples->writer)) {
        return S_OK;
    }
    if (m_dwOverridePosition != A3D_BUFFER_POSITION_UNSET) {
        m_dwReadPosition = m_dwOverridePosition;
        m_dwOverridePosition = A3D_BUFFER_POSITION_UNSET;
    }
    DWORD status = 0;
    HRESULT result = m_pStream->GetDSBuffer()->GetStatus(&status);
    if (FAILED(result)) {
        return result;
    }
    if (m_dwReadPosition == m_Desc.dwBufferBytes && !(m_dwFlags & A3D_RENDER_LOOPING)) {
        DWORD remaining = 0;
        result = m_pStream->GetAudioBytesQueued(&remaining);
        if (FAILED(result)) {
            return result;
        }
        if (!remaining || !(status & DSBSTATUS_PLAYING)) {
            result = m_pStream->GetDSBuffer()->Stop();
            if (FAILED(result)) {
                return result;
            }
            CaptureNativeStop(batch);
        }
        return S_OK;
    }
    if (!(status & DSBSTATUS_PLAYING)) {
        result = m_pStream->Reset();
        if (FAILED(result)) {
            return result;
        }
    }
    const DWORD oldPosition = m_dwReadPosition;
    DWORD position = oldPosition;
    result = m_pStream->Write(time, static_cast<const BYTE*>(m_hData), m_Desc.dwBufferBytes,
        &position, m_dwFlags);
    if (FAILED(result)) {
        return result;
    }
    m_dwReadPosition = position;
    m_nativeNotifications.Capture(oldPosition, m_pStream->m_nativeSubmittedBytes,
        m_Desc.dwBufferBytes, false, batch);
    if (!(status & DSBSTATUS_PLAYING)) {
        const bool tail = position == m_Desc.dwBufferBytes && !(m_dwFlags & A3D_RENDER_LOOPING);
        return m_pStream->GetDSBuffer()->Play(0, 0, tail ? 0 : DSBPLAY_LOOPING);
    }
    return S_OK;
}
#endif

#ifndef _WIN32
namespace {
constexpr DWORD kFullConeDegrees = 360;
HRESULT Immediate(DWORD apply) {
    return apply == DS3D_IMMEDIATE ? S_OK : apply == DS3D_DEFERRED ? E_NOTIMPL : E_INVALIDARG;
}
bool ValidVector(const D3DVECTOR& value) {
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}
template<class T> HRESULT ReadStream(const T& input, T* output) {
    if (!output) {
        return E_POINTER;
    }
    std::lock_guard<std::recursive_mutex> lock(A3dNativeResourceMutex());
    *output = input;
    return S_OK;
}
}
HRESULT ResManStreamBuffer::GetAllParameters(LPDS3DBUFFER parameters) {
    if (!parameters || parameters->dwSize != sizeof(*parameters)) {
        return E_INVALIDARG;
    }
    return ReadStream(m_nativeSpatial, parameters);
}

HRESULT ResManStreamBuffer::GetConeAngles(LPDWORD inside, LPDWORD outside) {
    if (!inside || !outside) {
        return E_POINTER;
    }
    std::lock_guard<std::recursive_mutex> lock(A3dNativeResourceMutex());
    *inside = m_nativeSpatial.dwInsideConeAngle;
    *outside = m_nativeSpatial.dwOutsideConeAngle;
    return S_OK;
}

HRESULT ResManStreamBuffer::SetAllParameters(LPCDS3DBUFFER parameters, DWORD apply) {
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
    std::lock_guard<std::recursive_mutex> lock(A3dNativeResourceMutex());
    m_nativeSpatial = *parameters;
    return S_OK;
}

HRESULT ResManStreamBuffer::SetConeAngles(DWORD inside, DWORD outside, DWORD apply) {
    const HRESULT status = Immediate(apply);
    if (FAILED(status)) {
        return status;
    }
    if (inside > outside || outside > kFullConeDegrees) {
        return E_INVALIDARG;
    }
    std::lock_guard<std::recursive_mutex> lock(A3dNativeResourceMutex());
    m_nativeSpatial.dwInsideConeAngle = inside;
    m_nativeSpatial.dwOutsideConeAngle = outside;
    return S_OK;
}

HRESULT ResManStreamBuffer::GetConeOrientation(D3DVECTOR* output) {
    return ReadStream(m_nativeSpatial.vConeOrientation, output);
}

HRESULT ResManStreamBuffer::SetConeOrientation(D3DVALUE x, D3DVALUE y, D3DVALUE z, DWORD apply) {
    const HRESULT status = Immediate(apply);
    if (FAILED(status)) {
        return status;
    }
    const D3DVECTOR value{x, y, z};
    std::lock_guard<std::recursive_mutex> lock(A3dNativeResourceMutex());
    if (!ValidVector(value)) {
        return E_INVALIDARG;
    }
    m_nativeSpatial.vConeOrientation = value;
    return S_OK;
}

HRESULT ResManStreamBuffer::GetPosition(D3DVECTOR* output) {
    return ReadStream(m_nativeSpatial.vPosition, output);
}

HRESULT ResManStreamBuffer::SetPosition(D3DVALUE x, D3DVALUE y, D3DVALUE z, DWORD apply) {
    const HRESULT status = Immediate(apply);
    if (FAILED(status)) {
        return status;
    }
    const D3DVECTOR value{x, y, z};
    std::lock_guard<std::recursive_mutex> lock(A3dNativeResourceMutex());
    if (!ValidVector(value)) {
        return E_INVALIDARG;
    }
    m_nativeSpatial.vPosition = value;
    return S_OK;
}

HRESULT ResManStreamBuffer::GetVelocity(D3DVECTOR* output) {
    return ReadStream(m_nativeSpatial.vVelocity, output);
}

HRESULT ResManStreamBuffer::SetVelocity(D3DVALUE x, D3DVALUE y, D3DVALUE z, DWORD apply) {
    const HRESULT status = Immediate(apply);
    if (FAILED(status)) {
        return status;
    }
    const D3DVECTOR value{x, y, z};
    std::lock_guard<std::recursive_mutex> lock(A3dNativeResourceMutex());
    if (!ValidVector(value)) {
        return E_INVALIDARG;
    }
    m_nativeSpatial.vVelocity = value;
    return S_OK;
}

HRESULT ResManStreamBuffer::GetConeOutsideVolume(LONG* output) {
    return ReadStream(m_nativeSpatial.lConeOutsideVolume, output);
}

HRESULT ResManStreamBuffer::SetConeOutsideVolume(LONG value, DWORD apply) {
    const HRESULT status = Immediate(apply);
    if (FAILED(status)) {
        return status;
    }
    std::lock_guard<std::recursive_mutex> lock(A3dNativeResourceMutex());
    if (value < DSBVOLUME_MIN || value > DSBVOLUME_MAX) {
        return E_INVALIDARG;
    }
    m_nativeSpatial.lConeOutsideVolume = value;
    return S_OK;
}

HRESULT ResManStreamBuffer::GetMaxDistance(D3DVALUE* output) {
    return ReadStream(m_nativeSpatial.flMaxDistance, output);
}

HRESULT ResManStreamBuffer::SetMaxDistance(D3DVALUE value, DWORD apply) {
    const HRESULT status = Immediate(apply);
    if (FAILED(status)) {
        return status;
    }
    std::lock_guard<std::recursive_mutex> lock(A3dNativeResourceMutex());
    if (!std::isfinite(value) || value < m_nativeSpatial.flMinDistance) {
        return E_INVALIDARG;
    }
    m_nativeSpatial.flMaxDistance = value;
    return S_OK;
}

HRESULT ResManStreamBuffer::GetMinDistance(D3DVALUE* output) {
    return ReadStream(m_nativeSpatial.flMinDistance, output);
}

HRESULT ResManStreamBuffer::SetMinDistance(D3DVALUE value, DWORD apply) {
    const HRESULT status = Immediate(apply);
    if (FAILED(status)) {
        return status;
    }
    std::lock_guard<std::recursive_mutex> lock(A3dNativeResourceMutex());
    if (!std::isfinite(value) || value <= 0 || value > m_nativeSpatial.flMaxDistance) {
        return E_INVALIDARG;
    }
    m_nativeSpatial.flMinDistance = value;
    return S_OK;
}

HRESULT ResManStreamBuffer::GetMode(DWORD* output) {
    return ReadStream(m_nativeSpatial.dwMode, output);
}

HRESULT ResManStreamBuffer::SetMode(DWORD value, DWORD apply) {
    const HRESULT status = Immediate(apply);
    if (FAILED(status)) {
        return status;
    }
    std::lock_guard<std::recursive_mutex> lock(A3dNativeResourceMutex());
    if (value > DS3DMODE_DISABLE) {
        return E_INVALIDARG;
    }
    m_nativeSpatial.dwMode = value;
    return S_OK;
}

#endif

#ifndef _WIN32
HRESULT ResMan::AttachNativeBackend(IUnknown* leaf) {
    std::lock_guard<std::recursive_mutex> admission(m_nativeAdmission);
    if (!leaf) {
        return E_POINTER;
    }
    if (m_nativeAttached || m_dwInitialized) {
        return E_INVALIDARG;
    }
    IA3dDal* dal = nullptr;
    IA3d2* a3d = nullptr;
    IA3dNativeDeviceV1* device = nullptr;
    IA3dNativeCapacityV1* capacity = nullptr;
    HRESULT result = leaf->QueryInterface(IID_A3dPlatformDeviceBackend, reinterpret_cast<void**>(&dal));
    if (SUCCEEDED(result) && !A3dNativeAcquisitionSucceeded()) {
        result = E_OUTOFMEMORY;
    }
    if (SUCCEEDED(result)) {
        result = leaf->QueryInterface(IID_IA3d2, reinterpret_cast<void**>(&a3d));
        if (SUCCEEDED(result) && !A3dNativeAcquisitionSucceeded()) {
            result = E_OUTOFMEMORY;
        }
    }
    if (SUCCEEDED(result)) {
        result = leaf->QueryInterface(IID_IA3dNativeDeviceV1, reinterpret_cast<void**>(&device));
        if (SUCCEEDED(result) && !A3dNativeAcquisitionSucceeded()) {
            result = E_OUTOFMEMORY;
        }
    }
    if (SUCCEEDED(result)) {
        result = leaf->QueryInterface(IID_IA3dNativeCapacityV1, reinterpret_cast<void**>(&capacity));
        if (SUCCEEDED(result) && !A3dNativeAcquisitionSucceeded()) {
            result = E_OUTOFMEMORY;
        }
    }
    DWORD total = 0;
    DWORD available = 0;
    if (SUCCEEDED(result)) {
        result = capacity->GetVoiceCapacity(&total, &available);
    }
    DalInfo* pool = nullptr;
    if (SUCCEEDED(result)) {
        const a3d_render_description_t description{A3D_RENDER_WRITE_BUFFER, A3D_PLATFORM_RENDER_BUFFER_BYTES,
            A3D_PLATFORM_RENDER_SAMPLE_RATE, A3D_PLATFORM_RENDER_SAMPLE_BITS, A3D_PLATFORM_RENDER_UPDATE_THRESHOLD};
        pool = new (std::nothrow) DalInfo(dal, &description);
        result = pool && A3dNativeAcquisitionSucceeded() ? pool->RefreshCapacity() : E_OUTOFMEMORY;
    }
    if (capacity) {
        capacity->Release();
    }
    if (device) {
        device->Release();
    }
    if (dal) {
        dal->Release();
    }
    if (FAILED(result)) {
        delete pool;
        if (a3d) {
            a3d->Release();
        }
        return result;
    }
    m_pA3dBackend = a3d;
    m_pDefaultPool = pool;
    m_dwWaitTime = A3D_RENDER_WAIT_MILLISECONDS;
    m_dwUpdateInterval = A3D_RENDER_UPDATE_MILLISECONDS;
    m_nativeAttached = true;
    m_dwInitialized = TRUE;
    result = m_nativeWorker.Start(RenderThread, this);
    if (SUCCEEDED(result) && !A3dNativeAcquisitionSucceeded()) {
        result = E_OUTOFMEMORY;
    }
    if (FAILED(result)) {
        ShutdownNative();
    }
    return result;
}

void ResMan::ShutdownNative() noexcept {
    std::lock_guard<std::recursive_mutex> admission(m_nativeAdmission);
    {
        std::lock_guard<std::recursive_mutex> lock(A3dNativeResourceMutex());
        m_dwInitialized = FALSE;
        m_nativeAttached = false;
    }
    StopNativeWorker();
    if (m_pA3dBackend) {
        A3dShutdownNativeLeaf(m_pA3dBackend);
    }
    for (;;) {
        ResManBuffer* retired = nullptr;
        A3dNativeNotificationBatch batch;
        {
            std::lock_guard<std::recursive_mutex> lock(A3dNativeResourceMutex());
            for (auto* node = m_SoftwareBuffers.pHead; node; node = node->pNext) {
                if (!node->pBuffer->m_nativeRetired) {
                    retired = node->pBuffer;
                    retired->AddRef();
                    retired->m_nativeRetired = true;
                    retired->CaptureNativeStop(batch);
                    retired->SetStream(nullptr);
                    break;
                }
            }
        }
        if (!retired) {
            break;
        }
        batch.Dispatch();
        retired->m_nativeNotifications.Shutdown();
        retired->Release();
    }
    if (m_pDefaultPool) {
        delete m_pDefaultPool;
        m_pDefaultPool = nullptr;
    }
    if (m_pA3dBackend) {
        m_pA3dBackend->Release();
        m_pA3dBackend = nullptr;
    }
    while (m_PendingBuffers.pHead) {
        auto* next = m_PendingBuffers.pHead->pNext;
        delete m_PendingBuffers.pHead;
        m_PendingBuffers.pHead = next;
    }
    m_PendingBuffers = {};
    m_PendingBuffers.cBlockNodes = A3D_PLATFORM_LIST_BLOCK_NODES;
    m_nativeAttached = false;
}

void ResMan::ReleaseNativeBuffer(ResManStreamBuffer* buffer) noexcept {
    bool owned = false;
    {
        std::lock_guard<std::recursive_mutex> lock(A3dNativeResourceMutex());
        for (auto* node = m_SoftwareBuffers.pHead; node; node = node->pNext) {
            if (node->pBuffer != buffer) {
                continue;
            }
            DalBufferInfo* stream = nullptr;
            buffer->GetRenderStream(&stream);
            if (stream) {
                DetachStream(A3dTickMilliseconds(), buffer, stream);
            }
            if (node->pPrevious) {
                node->pPrevious->pNext = node->pNext;
            } else {
                m_SoftwareBuffers.pHead = node->pNext;
            }
            if (node->pNext) {
                node->pNext->pPrevious = node->pPrevious;
            } else {
                m_SoftwareBuffers.pTail = node->pPrevious;
            }
            --m_SoftwareBuffers.cBuffers;
            delete node;
            owned = true;
            break;
        }
    }
    if (owned) {
        delete buffer;
    }
}

HRESULT ResMan::TickNative(DWORD time) {
    A3dNativeNotificationBatch* batches = nullptr;
    DWORD count = 0;
    HRESULT result = S_OK;
    {
        std::lock_guard<std::recursive_mutex> lock(A3dNativeResourceMutex());
        if (!m_nativeAttached) {
            return DSERR_UNINITIALIZED;
        }
        if (time - m_dwPreviousUpdate > m_dwUpdateInterval) {
            m_dwPreviousUpdate = time;
            result = FreeReleasedStreams(time);
            if (SUCCEEDED(result)) {
                result = HaltStoppedStreams(time);
            }
            if (SUCCEEDED(result)) {
                result = EnsureStreamCapacity(time);
            }
            if (SUCCEEDED(result)) {
                result = BuildPendingBuffers(time);
            }
            if (SUCCEEDED(result)) {
                result = AssignStreams(time);
            }
            if (SUCCEEDED(result)) {
                result = m_pDefaultPool->Update(time);
            }
            if (FAILED(result)) {
                return result;
            }
        }
        const DWORD buffers = static_cast<DWORD>(m_SoftwareBuffers.cBuffers);
        batches = new (std::nothrow) A3dNativeNotificationBatch[buffers];
        if (!batches && buffers) {
            return E_OUTOFMEMORY;
        }
        for (auto* node = m_SoftwareBuffers.pHead; node; node = node->pNext) {
            const HRESULT update = node->pBuffer->CaptureNativeUpdate(time, batches[count++]);
            if (FAILED(update) && update != E_ABORT) {
                result = update;
            }
        }
    }
    for (DWORD index = 0; index < count; ++index) {
        batches[index].Dispatch();
    }
    delete[] batches;
    return result;
}
#endif
