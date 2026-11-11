#pragma once
#ifndef _WIN32
#include "native_platform.h"
#include "native_notifications.h"
#include "A3dSource.h"
#include <mutex>

std::recursive_mutex& A3dNativeResourceMutex() noexcept;
#ifdef A3D_EVENT_TESTING
void A3dNativeSetFinalStreamReleaseHook(void (*hook)(void*), void* context) noexcept;
#endif
class ResManBuffer;
struct A3dNativeStreamSamples {
    A3dNativeOwnerLease m_nativeSamplesLease{A3dNativeOwnerKind::Samples};
    BYTE* data = nullptr;
    DWORD bytes = 0;
    ULONG references = 1;
    ResManBuffer* writer = nullptr;
};
#endif

#include "backend_legacy.h"

#include <ia3dapi.h>

class DalInfo;
class DalBufferInfo;

typedef struct a3d_platform_buffer_desc_s
{
	DWORD			dwSize;
	DWORD			dwFlags;
	DWORD			dwBufferBytes;
	DWORD			dwReserved;
	LPWAVEFORMATEX	lpwfxFormat;
} a3d_platform_buffer_desc_t;

class IA3dPlatformRenderBuffer : public IA3dDalBuffer
{
public:
	virtual HRESULT STDMETHODCALLTYPE SetA3dSuperCtrl( void* pData, DWORD dwFlags ) = 0;
	virtual HRESULT STDMETHODCALLTYPE GetAllocationStatus( DWORD dwValue ) = 0;
	virtual HRESULT STDMETHODCALLTYPE GetWave( DWORD dwValue ) = 0;
	virtual HRESULT STDMETHODCALLTYPE GetDriverInfo( DWORD dwFirst, DWORD dwSecond ) = 0;
	virtual HRESULT STDMETHODCALLTYPE SetNewBuffer( DWORD dwFirst, DWORD dwSecond ) = 0;
	virtual HRESULT STDMETHODCALLTYPE SetA3dDirectCtrl( DWORD dwFirst, DWORD dwSecond ) = 0;
};

class IA3dPlatformBufferControl : public IResManBuffer
{
public:
	virtual HRESULT STDMETHODCALLTYPE GetStatusEx( DWORD dwValue ) = 0;
	virtual HRESULT STDMETHODCALLTYPE GetPriority( LPDWORD pdwControl ) = 0;
	virtual HRESULT STDMETHODCALLTYPE GetBufferState( LPDWORD pdwStatus ) = 0;
	virtual HRESULT STDMETHODCALLTYPE SetPriority( DWORD dwFlags ) = 0;
	virtual HRESULT STDMETHODCALLTYPE Mute() = 0;
	virtual HRESULT STDMETHODCALLTYPE UnMute() = 0;
};

class ResManBuffer : public IDirectSoundBuffer,
	public IA3dPlatformRenderBuffer, public IA3dPlatformBufferControl
{
public:
	HRESULT STDMETHODCALLTYPE QueryInterface( REFIID iid, void** ppObject );
	ULONG STDMETHODCALLTYPE AddRef();
	ULONG STDMETHODCALLTYPE Release();
	HRESULT STDMETHODCALLTYPE GetCaps( LPDSBCAPS pCaps );
	HRESULT STDMETHODCALLTYPE GetCurrentPosition( LPDWORD pdwReadPosition, LPDWORD pdwWritePosition );
	HRESULT STDMETHODCALLTYPE GetFormat( LPWAVEFORMATEX pFormat, DWORD dwBytes, LPDWORD pdwWritten );
	HRESULT STDMETHODCALLTYPE GetVolume( LPLONG plVolume );
	HRESULT STDMETHODCALLTYPE GetPan( LPLONG plPan );
	HRESULT STDMETHODCALLTYPE GetFrequency( LPDWORD pdwFrequency );
	HRESULT STDMETHODCALLTYPE GetStatus( LPDWORD pdwStatus );
	HRESULT STDMETHODCALLTYPE Initialize( LPDIRECTSOUND pDirectSound, LPCDSBUFFERDESC pDesc );
	HRESULT STDMETHODCALLTYPE Lock( DWORD dwOffset, DWORD dwBytes, LPVOID* ppFirst, LPDWORD pdwFirstBytes,
		LPVOID* ppSecond, LPDWORD pdwSecondBytes, DWORD dwFlags );
	HRESULT STDMETHODCALLTYPE Play( DWORD dwReserved, DWORD dwPriority, DWORD dwFlags );
	HRESULT STDMETHODCALLTYPE SetCurrentPosition( DWORD dwPosition );
	HRESULT STDMETHODCALLTYPE SetFormat( LPCWAVEFORMATEX pFormat );
	HRESULT STDMETHODCALLTYPE SetVolume( LONG lVolume );
	HRESULT STDMETHODCALLTYPE SetPan( LONG lPan );
	HRESULT STDMETHODCALLTYPE SetFrequency( DWORD dwFrequency );
	HRESULT STDMETHODCALLTYPE Stop();
	HRESULT STDMETHODCALLTYPE Unlock( LPVOID pFirst, DWORD dwFirstBytes, LPVOID pSecond, DWORD dwSecondBytes );
	HRESULT STDMETHODCALLTYPE Restore();
	virtual HRESULT STDMETHODCALLTYPE SetNotificationPositions( DWORD dwCount, LPCDSBPOSITIONNOTIFY pNotifications );

	HRESULT STDMETHODCALLTYPE SetA3dSuperCtrl( void* pData, DWORD dwFlags );
	HRESULT STDMETHODCALLTYPE GetAllocationStatus( DWORD dwValue );
	HRESULT STDMETHODCALLTYPE GetWave( DWORD dwValue );
	HRESULT STDMETHODCALLTYPE GetDriverInfo( DWORD dwFirst, DWORD dwSecond );
	HRESULT STDMETHODCALLTYPE SetNewBuffer( DWORD dwFirst, DWORD dwSecond );
	HRESULT STDMETHODCALLTYPE SetA3dDirectCtrl( DWORD dwFirst, DWORD dwSecond );
	HRESULT STDMETHODCALLTYPE GetStatusEx( DWORD dwValue );
	HRESULT STDMETHODCALLTYPE GetPriority( LPDWORD pdwControl );
	HRESULT STDMETHODCALLTYPE GetBufferState( LPDWORD pdwStatus );
	HRESULT STDMETHODCALLTYPE SetPriority( DWORD dwFlags );
	HRESULT STDMETHODCALLTYPE Mute();
	HRESULT STDMETHODCALLTYPE UnMute();

	HRESULT Init( const a3d_platform_buffer_desc_t* pDesc, void* pBufferToken );
	HRESULT GetRenderStream( DalBufferInfo** ppStream );
	HRESULT SetStream( DalBufferInfo* pStream );
	HRESULT Duplicate( const ResManBuffer* pBuffer );
	HRESULT Update( DWORD dwTime );
#ifndef _WIN32
    HRESULT CaptureNativeUpdate(DWORD time, A3dNativeNotificationBatch& batch);
    void CaptureNativeStop(A3dNativeNotificationBatch& batch);
#endif

protected:
#ifndef _WIN32
    A3dNativeOwnerLease m_nativeStreamBufferLease{A3dNativeOwnerKind::StreamBuffer};
    friend class ResMan;
    bool m_nativeRetired = false;
    WAVEFORMATEX m_nativeFormat{};
    A3DCTRL_SRC_SUPER m_nativeControl{};
    bool m_nativeHasControl = false;
    DS3DBUFFER m_nativeSpatial{};
    DWORD m_nativeLockOffset = 0;
    DWORD m_nativeLockBytes = 0;
    A3dNativeStreamSamples* m_nativeSamples = nullptr;
    A3dNativeNotifications m_nativeNotifications;
#endif
	LONG							m_cReferences;
	void*							m_pBufferToken;
	DalBufferInfo*			m_pStream;
	void*							m_pRenderData;
	IUnknown*						m_pOwner;
	DWORD							m_dwBufferKind;
	a3d_platform_buffer_desc_t	m_Desc;
	HGLOBAL							m_hData;
	LONG							m_lVolume;
	LONG							m_lPan;
	DWORD							m_dwControl;
	DWORD							m_dwOriginalFrequency;
	DWORD							m_dwFrequency;
	DWORD							m_dwReadPosition;
	DWORD							m_dwOverridePosition;
	DWORD							m_dwStatus;
	DWORD							m_dwFlags;
	DWORD							m_dwNotificationCount;
	DSBPOSITIONNOTIFY*			m_pNotifications;
	HANDLE							m_hNotificationMutex;
};

class ResManStreamBuffer
#ifndef _WIN32
    final
#endif
    : public ResManBuffer
#ifndef _WIN32
    , public IA3dNativeNotifyV1, public IDirectSound3DBuffer
#endif
{
public:
	ResManStreamBuffer( IUnknown* pOwner, DWORD dwBufferKind );
	~ResManStreamBuffer();
#ifndef _WIN32
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void** object) override;
    ULONG STDMETHODCALLTYPE AddRef() override {
        return ResManBuffer::AddRef();
    }
    ULONG STDMETHODCALLTYPE Release() override {
        return ResManBuffer::Release();
    }
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
    using ResManBuffer::SetNotificationPositions;
    HRESULT STDMETHODCALLTYPE GetNotificationCaps(A3dNativeNotificationCapsV1* caps) override;
    HRESULT STDMETHODCALLTYPE SetNotificationPositions(DWORD count,
        const A3dNativeNotificationPositionV1* positions) override;
#endif
};

typedef struct a3d_platform_buffer_node_s
{
	struct a3d_platform_buffer_node_s*	pNext;
	struct a3d_platform_buffer_node_s*	pPrevious;
	ResManBuffer*					pBuffer;
} a3d_platform_buffer_node_t;

typedef struct a3d_platform_buffer_list_s
{
	a3d_platform_buffer_node_t*	pHead;
	a3d_platform_buffer_node_t*	pTail;
	LONG						cBuffers;
	a3d_platform_buffer_node_t*	pFreeNodes;
	void*						pBlocks;
	LONG						cBlockNodes;
} a3d_platform_buffer_list_t;

class IA3dDal : public IUnknown
{
public:
	virtual HRESULT STDMETHODCALLTYPE InitializeEx( DWORD dwFirst, DWORD dwSecond,
		void* pConfiguration, void* pBackend ) = 0;
	virtual HRESULT STDMETHODCALLTYPE CreateSoundBufferEx( const a3d_platform_buffer_desc_t* pDesc,
		BYTE* pbMode, IDirectSoundBuffer** ppBuffer, IUnknown* pOuter ) = 0;
	virtual HRESULT STDMETHODCALLTYPE GetA3dCaps( void* pFirst, void* pSecond ) = 0;
	virtual HRESULT STDMETHODCALLTYPE GetDriverInfo( void* pFirst, void* pSecond,
		void* pThird, void* pFourth ) = 0;
	virtual HRESULT STDMETHODCALLTYPE GetDS( IDirectSound** ppDirectSound ) = 0;
	virtual HRESULT STDMETHODCALLTYPE GetDSDriverDesc( void* pFirst, void* pSecond ) = 0;
	virtual HRESULT STDMETHODCALLTYPE QueryFunctionality( DWORD dwFirst, void* pSecond ) = 0;
	virtual HRESULT STDMETHODCALLTYPE DeviceCompatibility5( void* pFirst, void* pSecond, void* pThird ) = 0;
};

class ResMan : public IDirectSound, public IA3dDal, public IA3d2
{
public:
	ResMan();
#ifndef _WIN32
    ~ResMan();
    void StopNativeWorker() noexcept;
    HRESULT AttachNativeBackend(IUnknown* leaf);
    void ShutdownNative() noexcept;
    // Caller holds the resource mutex across the zero-reference transition.
    bool UnlinkNativeBuffer(ResManStreamBuffer* buffer) noexcept;
    HRESULT TickNative(DWORD time);

#endif

	HRESULT STDMETHODCALLTYPE QueryInterface( REFIID iid, void** ppObject );
	ULONG STDMETHODCALLTYPE AddRef();
	ULONG STDMETHODCALLTYPE Release();
	HRESULT STDMETHODCALLTYPE GetCaps( LPDSCAPS pCaps );
	HRESULT STDMETHODCALLTYPE CreateSoundBuffer( LPCDSBUFFERDESC pDesc, IDirectSoundBuffer** ppBuffer,
		IUnknown* pOuter );
	HRESULT STDMETHODCALLTYPE DuplicateSoundBuffer( IDirectSoundBuffer* pBuffer,
		IDirectSoundBuffer** ppDuplicate );
	HRESULT STDMETHODCALLTYPE SetCooperativeLevel( HWND hWindow, DWORD dwLevel );
	HRESULT STDMETHODCALLTYPE Compact();
	HRESULT STDMETHODCALLTYPE GetSpeakerConfig( LPDWORD pdwConfig );
	HRESULT STDMETHODCALLTYPE SetSpeakerConfig( DWORD dwConfig );
	HRESULT STDMETHODCALLTYPE Initialize( LPCGUID pDevice );

	HRESULT STDMETHODCALLTYPE InitializeEx( DWORD dwFirst, DWORD dwSecond,
		void* pConfiguration, void* pBackend );
	HRESULT STDMETHODCALLTYPE CreateSoundBufferEx( const a3d_platform_buffer_desc_t* pDesc,
		BYTE* pbMode, IDirectSoundBuffer** ppBuffer, IUnknown* pOuter );
	HRESULT STDMETHODCALLTYPE GetA3dCaps( void* pFirst, void* pSecond );
	HRESULT STDMETHODCALLTYPE GetDriverInfo( void* pFirst, void* pSecond,
		void* pThird, void* pFourth );
	HRESULT STDMETHODCALLTYPE GetDS( IDirectSound** ppDirectSound );
	HRESULT STDMETHODCALLTYPE GetDSDriverDesc( void* pFirst, void* pSecond );
	HRESULT STDMETHODCALLTYPE QueryFunctionality( DWORD dwFirst, void* pSecond );
	HRESULT STDMETHODCALLTYPE DeviceCompatibility5( void* pFirst, void* pSecond, void* pThird );

	HRESULT STDMETHODCALLTYPE SetOutputMode( DWORD dwFront, DWORD dwBack, DWORD dwQuad );
	HRESULT STDMETHODCALLTYPE GetOutputMode( LPDWORD pdwFront, LPDWORD pdwBack, LPDWORD pdwQuad );
	HRESULT STDMETHODCALLTYPE SetResourceManagerMode( DWORD dwMode );
	HRESULT STDMETHODCALLTYPE GetResourceManagerMode( LPDWORD pdwMode );
	HRESULT STDMETHODCALLTYPE SetHFAbsorbFactor( FLOAT flFactor );
	HRESULT STDMETHODCALLTYPE GetHFAbsorbFactor( FLOAT* pflFactor );
	HRESULT STDMETHODCALLTYPE RegisterVersion( DWORD dwVersion );
	HRESULT STDMETHODCALLTYPE GetSoftwareCaps( LPA3DCAPS_SOFTWARE pCaps );
	HRESULT STDMETHODCALLTYPE GetHardwareCaps( LPA3DCAPS_HARDWARE pCaps );

	HRESULT StartRenderer( DWORD dwFirst, DWORD dwSecond, void* pConfiguration, void* pBackend );
	HRESULT InitResMan( DWORD dwFirst, DWORD dwSecond, void* pConfiguration, void* pBackend );
	HRESULT CreatePrimaryBuffer( a3d_platform_buffer_desc_t* pDesc, IDirectSoundBuffer** ppBuffer, IUnknown* pOuter );
#ifndef _WIN32
    HRESULT CreateSecondaryBuffer(a3d_platform_buffer_desc_t* pDesc, IDirectSoundBuffer** ppBuffer,
        IUnknown* pOuter, DWORD mode);
#else
	HRESULT CreateSecondaryBuffer( a3d_platform_buffer_desc_t* pDesc, IDirectSoundBuffer** ppBuffer, IUnknown* pOuter );
#endif
	DWORD GetSoftwareStreamCount();
	DWORD CountPlayingSoftwareVoices();
	DWORD GetFreeSoftwareVoices();
	DWORD CountPlayingBuffers();
	static DWORD WINAPI RenderThread( LPVOID pContext );
	HRESULT FreeReleasedStreams( DWORD dwTime );
	HRESULT HaltStoppedStreams( DWORD dwTime );
	HRESULT EnsureStreamCapacity( DWORD dwTime );
	HRESULT BuildPendingBuffers( DWORD dwTime );
	HRESULT AssignStreams( DWORD dwTime );
	HRESULT ProcessPlayingBuffers( DWORD dwTime );
	void DebugBufferCounts();
	HRESULT DetachStream( DWORD dwTime, ResManBuffer* pBuffer, DalBufferInfo* pStream );
	HRESULT EnsurePoolCapacity( DWORD dwRequired, LPDWORD pdwCommitted, DalInfo* pPool );
	HRESULT CommitStreams( DWORD dwTime, DalInfo* pPool, LPLONG plRemaining,
		a3d_platform_buffer_node_t** ppNext );

protected:
	a3d_platform_buffer_list_t	m_SoftwareBuffers;
	a3d_platform_buffer_list_t	m_PendingBuffers;
	a3d_platform_buffer_list_t	m_HardwareBuffers;
	DWORD							m_dwReserved;
	IA3d2*							m_pA3dBackend;
	DalInfo*				m_pDefaultPool;
	DalInfo*				m_pHrtfPool;
	DalInfo*				m_pSecondaryPool;
	DWORD							m_dwResourceManagerMode;
	DWORD							m_dwThreadId;
#ifndef _WIN32
    A3dNativeOwnerLease m_nativeResourceManagerLease{A3dNativeOwnerKind::ResourceManager};
    std::recursive_mutex m_nativeAdmission;
    bool m_nativeAttached = false;
    A3dNativeWorker m_nativeWorker;
#endif
	HANDLE							m_hThread;
	HANDLE							m_hUpdateEvent;
	DWORD							m_dwWaitTime;
	DWORD							m_dwUpdateInterval;
	DWORD							m_dwPreviousUpdate;
	LONG							m_cReferences;
	DWORD							m_dwInitialized;
};
