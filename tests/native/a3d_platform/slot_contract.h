// Independent frozen slot contract: SDK DirectSound/COM and retained A3D headers.
// DirectSound declarations checked against Windows SDK 10.0.26100.0 dsound.h.
// Test-only marker providers implement production interfaces; C caller tables use
// the independently frozen slot order. No native provider behavior is qualified here.

#define SLOTS_IUnknown(X) \
    X(0, HRESULT, QueryInterface, (REFIID, void**), (void*, REFIID, void**), (object, IID_IUnknown, nullptr)) \
    X(1, ULONG, AddRef, (), (void*), (object)) \
    X(2, ULONG, Release, (), (void*), (object))
DECLARE_PROBE(IUnknown)

#define SLOTS_IClassFactory(X) \
    X(0, HRESULT, QueryInterface, (REFIID, void**), (void*, REFIID, void**), (object, IID_IUnknown, nullptr)) \
    X(1, ULONG, AddRef, (), (void*), (object)) \
    X(2, ULONG, Release, (), (void*), (object)) \
    X(3, HRESULT, CreateInstance, (IUnknown*, REFIID, void**), \
        (void*, IUnknown*, REFIID, void**), \
        (object, nullptr, IID_IUnknown, nullptr)) \
    X(4, HRESULT, LockServer, (BOOL), (void*, BOOL), (object, 0))
DECLARE_PROBE(IClassFactory)

#define SLOTS_IDirectSound(X) \
    X(0, HRESULT, QueryInterface, (REFIID, void**), (void*, REFIID, void**), (object, IID_IUnknown, nullptr)) \
    X(1, ULONG, AddRef, (), (void*), (object)) \
    X(2, ULONG, Release, (), (void*), (object)) \
    X(3, HRESULT, CreateSoundBuffer, (LPCDSBUFFERDESC, LPDIRECTSOUNDBUFFER*, IUnknown*), \
        (void*, LPCDSBUFFERDESC, LPDIRECTSOUNDBUFFER*, IUnknown*), \
        (object, nullptr, nullptr, nullptr)) \
    X(4, HRESULT, GetCaps, (LPDSCAPS), (void*, LPDSCAPS), (object, nullptr)) \
    X(5, HRESULT, DuplicateSoundBuffer, (LPDIRECTSOUNDBUFFER, LPDIRECTSOUNDBUFFER*), \
        (void*, LPDIRECTSOUNDBUFFER, LPDIRECTSOUNDBUFFER*), \
        (object, nullptr, nullptr)) \
    X(6, HRESULT, SetCooperativeLevel, (HWND, DWORD), (void*, HWND, DWORD), (object, nullptr, 0)) \
    X(7, HRESULT, Compact, (), (void*), (object)) \
    X(8, HRESULT, GetSpeakerConfig, (LPDWORD), (void*, LPDWORD), (object, nullptr)) \
    X(9, HRESULT, SetSpeakerConfig, (DWORD), (void*, DWORD), (object, 0)) \
    X(10, HRESULT, Initialize, (LPCGUID), (void*, LPCGUID), (object, nullptr))
DECLARE_PROBE(IDirectSound)

#define SLOTS_IDirectSoundBuffer(X) \
    X(0, HRESULT, QueryInterface, (REFIID, void**), (void*, REFIID, void**), (object, IID_IUnknown, nullptr)) \
    X(1, ULONG, AddRef, (), (void*), (object)) \
    X(2, ULONG, Release, (), (void*), (object)) \
    X(3, HRESULT, GetCaps, (LPDSBCAPS), (void*, LPDSBCAPS), (object, nullptr)) \
    X(4, HRESULT, GetCurrentPosition, (LPDWORD, LPDWORD), (void*, LPDWORD, LPDWORD), (object, nullptr, nullptr)) \
    X(5, HRESULT, GetFormat, (LPWAVEFORMATEX, DWORD, LPDWORD), \
        (void*, LPWAVEFORMATEX, DWORD, LPDWORD), \
        (object, nullptr, 0, nullptr)) \
    X(6, HRESULT, GetVolume, (LPLONG), (void*, LPLONG), (object, nullptr)) \
    X(7, HRESULT, GetPan, (LPLONG), (void*, LPLONG), (object, nullptr)) \
    X(8, HRESULT, GetFrequency, (LPDWORD), (void*, LPDWORD), (object, nullptr)) \
    X(9, HRESULT, GetStatus, (LPDWORD), (void*, LPDWORD), (object, nullptr)) \
    X(10, HRESULT, Initialize, (LPDIRECTSOUND, LPCDSBUFFERDESC), \
        (void*, LPDIRECTSOUND, LPCDSBUFFERDESC), \
        (object, nullptr, nullptr)) \
    X(11, HRESULT, Lock, (DWORD, DWORD, LPVOID*, LPDWORD, LPVOID*, LPDWORD, DWORD), \
        (void*, DWORD, DWORD, LPVOID*, LPDWORD, LPVOID*, LPDWORD, DWORD), \
        (object, 0, 0, nullptr, nullptr, nullptr, nullptr, 0)) \
    X(12, HRESULT, Play, (DWORD, DWORD, DWORD), (void*, DWORD, DWORD, DWORD), (object, 0, 0, 0)) \
    X(13, HRESULT, SetCurrentPosition, (DWORD), (void*, DWORD), (object, 0)) \
    X(14, HRESULT, SetFormat, (LPCWAVEFORMATEX), (void*, LPCWAVEFORMATEX), (object, nullptr)) \
    X(15, HRESULT, SetVolume, (LONG), (void*, LONG), (object, 0)) \
    X(16, HRESULT, SetPan, (LONG), (void*, LONG), (object, 0)) \
    X(17, HRESULT, SetFrequency, (DWORD), (void*, DWORD), (object, 0)) \
    X(18, HRESULT, Stop, (), (void*), (object)) \
    X(19, HRESULT, Unlock, (LPVOID, DWORD, LPVOID, DWORD), \
        (void*, LPVOID, DWORD, LPVOID, DWORD), \
        (object, nullptr, 0, nullptr, 0)) \
    X(20, HRESULT, Restore, (), (void*), (object))
DECLARE_PROBE(IDirectSoundBuffer)

#define SLOTS_IDirectSound3DListener(X) \
    X(0, HRESULT, QueryInterface, (REFIID, void**), (void*, REFIID, void**), (object, IID_IUnknown, nullptr)) \
    X(1, ULONG, AddRef, (), (void*), (object)) \
    X(2, ULONG, Release, (), (void*), (object)) \
    X(3, HRESULT, GetAllParameters, (LPDS3DLISTENER), (void*, LPDS3DLISTENER), (object, nullptr)) \
    X(4, HRESULT, GetDistanceFactor, (D3DVALUE*), (void*, D3DVALUE*), (object, nullptr)) \
    X(5, HRESULT, GetDopplerFactor, (D3DVALUE*), (void*, D3DVALUE*), (object, nullptr)) \
    X(6, HRESULT, GetOrientation, (LPD3DVECTOR, LPD3DVECTOR), \
        (void*, LPD3DVECTOR, LPD3DVECTOR), \
        (object, nullptr, nullptr)) \
    X(7, HRESULT, GetPosition, (LPD3DVECTOR), (void*, LPD3DVECTOR), (object, nullptr)) \
    X(8, HRESULT, GetRolloffFactor, (D3DVALUE*), (void*, D3DVALUE*), (object, nullptr)) \
    X(9, HRESULT, GetVelocity, (LPD3DVECTOR), (void*, LPD3DVECTOR), (object, nullptr)) \
    X(10, HRESULT, SetAllParameters, (LPCDS3DLISTENER, DWORD), (void*, LPCDS3DLISTENER, DWORD), (object, nullptr, 0)) \
    X(11, HRESULT, SetDistanceFactor, (D3DVALUE, DWORD), (void*, D3DVALUE, DWORD), (object, 0, 0)) \
    X(12, HRESULT, SetDopplerFactor, (D3DVALUE, DWORD), (void*, D3DVALUE, DWORD), (object, 0, 0)) \
    X(13, HRESULT, SetOrientation, (D3DVALUE, D3DVALUE, D3DVALUE, D3DVALUE, D3DVALUE, D3DVALUE, DWORD), \
        (void*, D3DVALUE, D3DVALUE, D3DVALUE, D3DVALUE, D3DVALUE, D3DVALUE, DWORD), \
        (object, 0, 0, 0, 0, 0, 0, 0)) \
    X(14, HRESULT, SetPosition, (D3DVALUE, D3DVALUE, D3DVALUE, DWORD), \
        (void*, D3DVALUE, D3DVALUE, D3DVALUE, DWORD), \
        (object, 0, 0, 0, 0)) \
    X(15, HRESULT, SetRolloffFactor, (D3DVALUE, DWORD), (void*, D3DVALUE, DWORD), (object, 0, 0)) \
    X(16, HRESULT, SetVelocity, (D3DVALUE, D3DVALUE, D3DVALUE, DWORD), \
        (void*, D3DVALUE, D3DVALUE, D3DVALUE, DWORD), \
        (object, 0, 0, 0, 0)) \
    X(17, HRESULT, CommitDeferredSettings, (), (void*), (object))
DECLARE_PROBE(IDirectSound3DListener)

#define SLOTS_IDirectSound3DBuffer(X) \
    X(0, HRESULT, QueryInterface, (REFIID, void**), (void*, REFIID, void**), (object, IID_IUnknown, nullptr)) \
    X(1, ULONG, AddRef, (), (void*), (object)) \
    X(2, ULONG, Release, (), (void*), (object)) \
    X(3, HRESULT, GetAllParameters, (LPDS3DBUFFER), (void*, LPDS3DBUFFER), (object, nullptr)) \
    X(4, HRESULT, GetConeAngles, (LPDWORD, LPDWORD), (void*, LPDWORD, LPDWORD), (object, nullptr, nullptr)) \
    X(5, HRESULT, GetConeOrientation, (LPD3DVECTOR), (void*, LPD3DVECTOR), (object, nullptr)) \
    X(6, HRESULT, GetConeOutsideVolume, (LPLONG), (void*, LPLONG), (object, nullptr)) \
    X(7, HRESULT, GetMaxDistance, (D3DVALUE*), (void*, D3DVALUE*), (object, nullptr)) \
    X(8, HRESULT, GetMinDistance, (D3DVALUE*), (void*, D3DVALUE*), (object, nullptr)) \
    X(9, HRESULT, GetMode, (LPDWORD), (void*, LPDWORD), (object, nullptr)) \
    X(10, HRESULT, GetPosition, (LPD3DVECTOR), (void*, LPD3DVECTOR), (object, nullptr)) \
    X(11, HRESULT, GetVelocity, (LPD3DVECTOR), (void*, LPD3DVECTOR), (object, nullptr)) \
    X(12, HRESULT, SetAllParameters, (LPCDS3DBUFFER, DWORD), (void*, LPCDS3DBUFFER, DWORD), (object, nullptr, 0)) \
    X(13, HRESULT, SetConeAngles, (DWORD, DWORD, DWORD), (void*, DWORD, DWORD, DWORD), (object, 0, 0, 0)) \
    X(14, HRESULT, SetConeOrientation, (D3DVALUE, D3DVALUE, D3DVALUE, DWORD), \
        (void*, D3DVALUE, D3DVALUE, D3DVALUE, DWORD), \
        (object, 0, 0, 0, 0)) \
    X(15, HRESULT, SetConeOutsideVolume, (LONG, DWORD), (void*, LONG, DWORD), (object, 0, 0)) \
    X(16, HRESULT, SetMaxDistance, (D3DVALUE, DWORD), (void*, D3DVALUE, DWORD), (object, 0, 0)) \
    X(17, HRESULT, SetMinDistance, (D3DVALUE, DWORD), (void*, D3DVALUE, DWORD), (object, 0, 0)) \
    X(18, HRESULT, SetMode, (DWORD, DWORD), (void*, DWORD, DWORD), (object, 0, 0)) \
    X(19, HRESULT, SetPosition, (D3DVALUE, D3DVALUE, D3DVALUE, DWORD), \
        (void*, D3DVALUE, D3DVALUE, D3DVALUE, DWORD), \
        (object, 0, 0, 0, 0)) \
    X(20, HRESULT, SetVelocity, (D3DVALUE, D3DVALUE, D3DVALUE, DWORD), \
        (void*, D3DVALUE, D3DVALUE, D3DVALUE, DWORD), \
        (object, 0, 0, 0, 0))
DECLARE_PROBE(IDirectSound3DBuffer)

#define SLOTS_IDirectSoundNotify(X) \
    X(0, HRESULT, QueryInterface, (REFIID, void**), (void*, REFIID, void**), (object, IID_IUnknown, nullptr)) \
    X(1, ULONG, AddRef, (), (void*), (object)) \
    X(2, ULONG, Release, (), (void*), (object)) \
    X(3, HRESULT, SetNotificationPositions, (DWORD, LPCDSBPOSITIONNOTIFY), \
        (void*, DWORD, LPCDSBPOSITIONNOTIFY), \
        (object, 0, nullptr))
DECLARE_PROBE(IDirectSoundNotify)

#define SLOTS_IA3d(X) \
    X(0, HRESULT, QueryInterface, (REFIID, LPVOID FAR *), \
        (void*, REFIID, LPVOID FAR *), \
        (object, IID_IUnknown, nullptr)) \
    X(1, ULONG, AddRef, (), (void*), (object)) \
    X(2, ULONG, Release, (), (void*), (object)) \
    X(3, HRESULT, SetOutputMode, (DWORD, DWORD, DWORD), (void*, DWORD, DWORD, DWORD), (object, 0, 0, 0)) \
    X(4, HRESULT, GetOutputMode, (LPDWORD, LPDWORD, LPDWORD), \
        (void*, LPDWORD, LPDWORD, LPDWORD), \
        (object, nullptr, nullptr, nullptr)) \
    X(5, HRESULT, SetResourceManagerMode, (DWORD), (void*, DWORD), (object, 0)) \
    X(6, HRESULT, GetResourceManagerMode, (LPDWORD), (void*, LPDWORD), (object, nullptr)) \
    X(7, HRESULT, SetHFAbsorbFactor, (FLOAT), (void*, FLOAT), (object, 0)) \
    X(8, HRESULT, GetHFAbsorbFactor, (FLOAT *), (void*, FLOAT *), (object, nullptr))
DECLARE_PROBE(IA3d)

#define SLOTS_IA3d2(X) \
    X(0, HRESULT, QueryInterface, (REFIID, LPVOID FAR *), \
        (void*, REFIID, LPVOID FAR *), \
        (object, IID_IUnknown, nullptr)) \
    X(1, ULONG, AddRef, (), (void*), (object)) \
    X(2, ULONG, Release, (), (void*), (object)) \
    X(3, HRESULT, SetOutputMode, (DWORD, DWORD, DWORD), (void*, DWORD, DWORD, DWORD), (object, 0, 0, 0)) \
    X(4, HRESULT, GetOutputMode, (LPDWORD, LPDWORD, LPDWORD), \
        (void*, LPDWORD, LPDWORD, LPDWORD), \
        (object, nullptr, nullptr, nullptr)) \
    X(5, HRESULT, SetResourceManagerMode, (DWORD), (void*, DWORD), (object, 0)) \
    X(6, HRESULT, GetResourceManagerMode, (LPDWORD), (void*, LPDWORD), (object, nullptr)) \
    X(7, HRESULT, SetHFAbsorbFactor, (FLOAT), (void*, FLOAT), (object, 0)) \
    X(8, HRESULT, GetHFAbsorbFactor, (FLOAT *), (void*, FLOAT *), (object, nullptr)) \
    X(9, HRESULT, RegisterVersion, (DWORD), (void*, DWORD), (object, 0)) \
    X(10, HRESULT, GetSoftwareCaps, (LPA3DCAPS_SOFTWARE), (void*, LPA3DCAPS_SOFTWARE), (object, nullptr)) \
    X(11, HRESULT, GetHardwareCaps, (LPA3DCAPS_HARDWARE), (void*, LPA3DCAPS_HARDWARE), (object, nullptr))
DECLARE_PROBE(IA3d2)

#define SLOTS_IA3d3(X) \
    X(0, HRESULT, QueryInterface, (REFIID, LPVOID FAR *), \
        (void*, REFIID, LPVOID FAR *), \
        (object, IID_IUnknown, nullptr)) \
    X(1, ULONG, AddRef, (), (void*), (object)) \
    X(2, ULONG, Release, (), (void*), (object)) \
    X(3, HRESULT, SetOutputMode, (DWORD, DWORD, DWORD), (void*, DWORD, DWORD, DWORD), (object, 0, 0, 0)) \
    X(4, HRESULT, GetOutputMode, (LPDWORD, LPDWORD, LPDWORD), \
        (void*, LPDWORD, LPDWORD, LPDWORD), \
        (object, nullptr, nullptr, nullptr)) \
    X(5, HRESULT, SetResourceManagerMode, (DWORD), (void*, DWORD), (object, 0)) \
    X(6, HRESULT, GetResourceManagerMode, (LPDWORD), (void*, LPDWORD), (object, nullptr)) \
    X(7, HRESULT, SetHFAbsorbFactor, (FLOAT), (void*, FLOAT), (object, 0)) \
    X(8, HRESULT, GetHFAbsorbFactor, (FLOAT *), (void*, FLOAT *), (object, nullptr)) \
    X(9, HRESULT, RegisterVersion, (DWORD), (void*, DWORD), (object, 0)) \
    X(10, HRESULT, GetSoftwareCaps, (LPA3DCAPS_SOFTWARE), (void*, LPA3DCAPS_SOFTWARE), (object, nullptr)) \
    X(11, HRESULT, GetHardwareCaps, (LPA3DCAPS_HARDWARE), (void*, LPA3DCAPS_HARDWARE), (object, nullptr)) \
    X(12, HRESULT, Clear, (), (void*), (object)) \
    X(13, HRESULT, Flush, (), (void*), (object)) \
    X(14, HRESULT, Compat, (DWORD, DWORD), (void*, DWORD, DWORD), (object, 0, 0)) \
    X(15, HRESULT, Init, (LPGUID, DWORD, DWORD), (void*, LPGUID, DWORD, DWORD), (object, nullptr, 0, 0)) \
    X(16, HRESULT, IsFeatureAvailable, (DWORD), (void*, DWORD), (object, 0)) \
    X(17, HRESULT, NewSource, (DWORD, LPA3DSOURCE *), (void*, DWORD, LPA3DSOURCE *), (object, 0, nullptr)) \
    X(18, HRESULT, DuplicateSource, (LPA3DSOURCE, LPA3DSOURCE *), \
        (void*, LPA3DSOURCE, LPA3DSOURCE *), \
        (object, nullptr, nullptr)) \
    X(19, HRESULT, SetCooperativeLevel, (HWND, DWORD), (void*, HWND, DWORD), (object, nullptr, 0)) \
    X(20, HRESULT, GetCooperativeLevel, (LPDWORD), (void*, LPDWORD), (object, nullptr)) \
    X(21, HRESULT, SetMaxReflectionDelayTime, (A3DVAL), (void*, A3DVAL), (object, 0)) \
    X(22, HRESULT, GetMaxReflectionDelayTime, (LPA3DVAL), (void*, LPA3DVAL), (object, nullptr)) \
    X(23, HRESULT, SetCoordinateSystem, (DWORD), (void*, DWORD), (object, 0)) \
    X(24, HRESULT, GetCoordinateSystem, (LPDWORD), (void*, LPDWORD), (object, nullptr)) \
    X(25, HRESULT, SetOutputGain, (A3DVAL), (void*, A3DVAL), (object, 0)) \
    X(26, HRESULT, GetOutputGain, (LPA3DVAL), (void*, LPA3DVAL), (object, nullptr)) \
    X(27, HRESULT, SetNumFallbackSources, (DWORD), (void*, DWORD), (object, 0)) \
    X(28, HRESULT, GetNumFallbackSources, (LPDWORD), (void*, LPDWORD), (object, nullptr))
DECLARE_PROBE(IA3d3)

#define SLOTS_IA3d4(X) \
    X(0, HRESULT, QueryInterface, (REFIID, LPVOID FAR *), \
        (void*, REFIID, LPVOID FAR *), \
        (object, IID_IUnknown, nullptr)) \
    X(1, ULONG, AddRef, (), (void*), (object)) \
    X(2, ULONG, Release, (), (void*), (object)) \
    X(3, HRESULT, SetOutputMode, (DWORD, DWORD, DWORD), (void*, DWORD, DWORD, DWORD), (object, 0, 0, 0)) \
    X(4, HRESULT, GetOutputMode, (LPDWORD, LPDWORD, LPDWORD), \
        (void*, LPDWORD, LPDWORD, LPDWORD), \
        (object, nullptr, nullptr, nullptr)) \
    X(5, HRESULT, SetResourceManagerMode, (DWORD), (void*, DWORD), (object, 0)) \
    X(6, HRESULT, GetResourceManagerMode, (LPDWORD), (void*, LPDWORD), (object, nullptr)) \
    X(7, HRESULT, SetHFAbsorbFactor, (FLOAT), (void*, FLOAT), (object, 0)) \
    X(8, HRESULT, GetHFAbsorbFactor, (FLOAT *), (void*, FLOAT *), (object, nullptr)) \
    X(9, HRESULT, RegisterVersion, (DWORD), (void*, DWORD), (object, 0)) \
    X(10, HRESULT, GetSoftwareCaps, (LPA3DCAPS_SOFTWARE), (void*, LPA3DCAPS_SOFTWARE), (object, nullptr)) \
    X(11, HRESULT, GetHardwareCaps, (LPA3DCAPS_HARDWARE), (void*, LPA3DCAPS_HARDWARE), (object, nullptr)) \
    X(12, HRESULT, Clear, (), (void*), (object)) \
    X(13, HRESULT, Flush, (), (void*), (object)) \
    X(14, HRESULT, Compat, (DWORD, DWORD), (void*, DWORD, DWORD), (object, 0, 0)) \
    X(15, HRESULT, Init, (LPGUID, DWORD, DWORD), (void*, LPGUID, DWORD, DWORD), (object, nullptr, 0, 0)) \
    X(16, HRESULT, IsFeatureAvailable, (DWORD), (void*, DWORD), (object, 0)) \
    X(17, HRESULT, NewSource, (DWORD, LPA3DSOURCE *), (void*, DWORD, LPA3DSOURCE *), (object, 0, nullptr)) \
    X(18, HRESULT, DuplicateSource, (LPA3DSOURCE, LPA3DSOURCE *), \
        (void*, LPA3DSOURCE, LPA3DSOURCE *), \
        (object, nullptr, nullptr)) \
    X(19, HRESULT, SetCooperativeLevel, (HWND, DWORD), (void*, HWND, DWORD), (object, nullptr, 0)) \
    X(20, HRESULT, GetCooperativeLevel, (LPDWORD), (void*, LPDWORD), (object, nullptr)) \
    X(21, HRESULT, SetMaxReflectionDelayTime, (A3DVAL), (void*, A3DVAL), (object, 0)) \
    X(22, HRESULT, GetMaxReflectionDelayTime, (LPA3DVAL), (void*, LPA3DVAL), (object, nullptr)) \
    X(23, HRESULT, SetCoordinateSystem, (DWORD), (void*, DWORD), (object, 0)) \
    X(24, HRESULT, GetCoordinateSystem, (LPDWORD), (void*, LPDWORD), (object, nullptr)) \
    X(25, HRESULT, SetOutputGain, (A3DVAL), (void*, A3DVAL), (object, 0)) \
    X(26, HRESULT, GetOutputGain, (LPA3DVAL), (void*, LPA3DVAL), (object, nullptr)) \
    X(27, HRESULT, SetNumFallbackSources, (DWORD), (void*, DWORD), (object, 0)) \
    X(28, HRESULT, GetNumFallbackSources, (LPDWORD), (void*, LPDWORD), (object, nullptr)) \
    X(29, HRESULT, SetRMPriorityBias, (A3DVAL), (void*, A3DVAL), (object, 0)) \
    X(30, HRESULT, GetRMPriorityBias, (LPA3DVAL), (void*, LPA3DVAL), (object, nullptr)) \
    X(31, HRESULT, DisableViewer, (), (void*), (object)) \
    X(32, HRESULT, SetUnitsPerMeter, (A3DVAL), (void*, A3DVAL), (object, 0)) \
    X(33, HRESULT, GetUnitsPerMeter, (LPA3DVAL), (void*, LPA3DVAL), (object, nullptr)) \
    X(34, HRESULT, SetDopplerScale, (A3DVAL), (void*, A3DVAL), (object, 0)) \
    X(35, HRESULT, GetDopplerScale, (LPA3DVAL), (void*, LPA3DVAL), (object, nullptr)) \
    X(36, HRESULT, SetDistanceModelScale, (A3DVAL), (void*, A3DVAL), (object, 0)) \
    X(37, HRESULT, GetDistanceModelScale, (LPA3DVAL), (void*, LPA3DVAL), (object, nullptr)) \
    X(38, HRESULT, SetEq, (A3DVAL), (void*, A3DVAL), (object, 0)) \
    X(39, HRESULT, GetEq, (LPA3DVAL), (void*, LPA3DVAL), (object, nullptr)) \
    X(40, HRESULT, Shutdown, (), (void*), (object)) \
    X(41, HRESULT, RegisterApp, (REFIID), (void*, REFIID), (object, IID_IUnknown))
DECLARE_PROBE(IA3d4)

#define SLOTS_IA3dGeom(X) \
    X(0, HRESULT, QueryInterface, (REFIID, LPVOID FAR *), \
        (void*, REFIID, LPVOID FAR *), \
        (object, IID_IUnknown, nullptr)) \
    X(1, ULONG, AddRef, (), (void*), (object)) \
    X(2, ULONG, Release, (), (void*), (object)) \
    X(3, HRESULT, Enable, (DWORD), (void*, DWORD), (object, 0)) \
    X(4, HRESULT, Disable, (DWORD), (void*, DWORD), (object, 0)) \
    X(5, BOOL, IsEnabled, (DWORD), (void*, DWORD), (object, 0)) \
    X(6, HRESULT, SetOcclusionMode, (DWORD), (void*, DWORD), (object, 0)) \
    X(7, HRESULT, GetOcclusionMode, (LPDWORD), (void*, LPDWORD), (object, nullptr)) \
    X(8, HRESULT, SetReflectionMode, (DWORD), (void*, DWORD), (object, 0)) \
    X(9, HRESULT, GetReflectionMode, (LPDWORD), (void*, LPDWORD), (object, nullptr)) \
    X(10, HRESULT, SetReflectionGainScale, (A3DVAL), (void*, A3DVAL), (object, 0)) \
    X(11, HRESULT, GetReflectionGainScale, (LPA3DVAL), (void*, LPA3DVAL), (object, nullptr)) \
    X(12, HRESULT, SetReflectionDelayScale, (A3DVAL), (void*, A3DVAL), (object, 0)) \
    X(13, HRESULT, GetReflectionDelayScale, (LPA3DVAL), (void*, LPA3DVAL), (object, nullptr)) \
    X(14, ULONG, PushMatrix, (), (void*), (object)) \
    X(15, ULONG, PopMatrix, (), (void*), (object)) \
    X(16, HRESULT, LoadIdentity, (), (void*), (object)) \
    X(17, HRESULT, LoadMatrix, (A3DMATRIX), (void*, A3DMATRIX), (object, nullptr)) \
    X(18, HRESULT, GetMatrix, (A3DMATRIX), (void*, A3DMATRIX), (object, nullptr)) \
    X(19, HRESULT, MultMatrix, (A3DMATRIX), (void*, A3DMATRIX), (object, nullptr)) \
    X(20, HRESULT, Translate3f, (A3DVAL, A3DVAL, A3DVAL), (void*, A3DVAL, A3DVAL, A3DVAL), (object, 0, 0, 0)) \
    X(21, HRESULT, Translate3fv, (LPA3DVAL), (void*, LPA3DVAL), (object, nullptr)) \
    X(22, HRESULT, Rotate3f, (A3DVAL, A3DVAL, A3DVAL, A3DVAL), \
        (void*, A3DVAL, A3DVAL, A3DVAL, A3DVAL), \
        (object, 0, 0, 0, 0)) \
    X(23, HRESULT, Rotate3fv, (A3DVAL, LPA3DVAL), (void*, A3DVAL, LPA3DVAL), (object, 0, nullptr)) \
    X(24, HRESULT, Scale3f, (A3DVAL, A3DVAL, A3DVAL), (void*, A3DVAL, A3DVAL, A3DVAL), (object, 0, 0, 0)) \
    X(25, HRESULT, Scale3fv, (LPA3DVAL), (void*, LPA3DVAL), (object, nullptr)) \
    X(26, HRESULT, Begin, (DWORD), (void*, DWORD), (object, 0)) \
    X(27, HRESULT, End, (), (void*), (object)) \
    X(28, HRESULT, Vertex3f, (A3DVAL, A3DVAL, A3DVAL), (void*, A3DVAL, A3DVAL, A3DVAL), (object, 0, 0, 0)) \
    X(29, HRESULT, Vertex3fv, (LPA3DVAL), (void*, LPA3DVAL), (object, nullptr)) \
    X(30, HRESULT, Normal3f, (A3DVAL, A3DVAL, A3DVAL), (void*, A3DVAL, A3DVAL, A3DVAL), (object, 0, 0, 0)) \
    X(31, HRESULT, Normal3fv, (LPA3DVAL), (void*, LPA3DVAL), (object, nullptr)) \
    X(32, HRESULT, Tag, (DWORD), (void*, DWORD), (object, 0)) \
    X(33, HRESULT, SetOpeningFactorf, (A3DVAL), (void*, A3DVAL), (object, 0)) \
    X(34, HRESULT, SetOpeningFactorfv, (LPA3DVAL), (void*, LPA3DVAL), (object, nullptr)) \
    X(35, HRESULT, NewMaterial, (LPA3DMATERIAL *), (void*, LPA3DMATERIAL *), (object, nullptr)) \
    X(36, HRESULT, BindMaterial, (LPA3DMATERIAL), (void*, LPA3DMATERIAL), (object, nullptr)) \
    X(37, HRESULT, NewList, (LPA3DLIST *), (void*, LPA3DLIST *), (object, nullptr)) \
    X(38, HRESULT, BindListener, (), (void*), (object)) \
    X(39, HRESULT, BindSource, (LPA3DSOURCE), (void*, LPA3DSOURCE), (object, nullptr)) \
    X(40, HRESULT, NewEnvironment, (LPA3DENVIRONMENT *), (void*, LPA3DENVIRONMENT *), (object, nullptr)) \
    X(41, HRESULT, BindEnvironment, (LPA3DENVIRONMENT), (void*, LPA3DENVIRONMENT), (object, nullptr)) \
    X(42, HRESULT, SetRenderMode, (DWORD), (void*, DWORD), (object, 0)) \
    X(43, HRESULT, GetRenderMode, (LPDWORD), (void*, LPDWORD), (object, nullptr)) \
    X(44, HRESULT, SetPolygonBloatFactor, (A3DVAL), (void*, A3DVAL), (object, 0)) \
    X(45, HRESULT, GetPolygonBloatFactor, (LPA3DVAL), (void*, LPA3DVAL), (object, nullptr)) \
    X(46, HRESULT, SetReflectionUpdateInterval, (DWORD), (void*, DWORD), (object, 0)) \
    X(47, HRESULT, GetReflectionUpdateInterval, (LPDWORD), (void*, LPDWORD), (object, nullptr)) \
    X(48, HRESULT, SetOcclusionUpdateInterval, (DWORD), (void*, DWORD), (object, 0)) \
    X(49, HRESULT, GetOcclusionUpdateInterval, (LPDWORD), (void*, LPDWORD), (object, nullptr))
DECLARE_PROBE(IA3dGeom)

#define SLOTS_IA3dSource(X) \
    X(0, HRESULT, QueryInterface, (REFIID, LPVOID FAR *), \
        (void*, REFIID, LPVOID FAR *), \
        (object, IID_IUnknown, nullptr)) \
    X(1, ULONG, AddRef, (), (void*), (object)) \
    X(2, ULONG, Release, (), (void*), (object)) \
    X(3, HRESULT, LoadWaveFile, (LPSTR), (void*, LPSTR), (object, nullptr)) \
    X(4, HRESULT, LoadWaveData, (LPVOID, DWORD), (void*, LPVOID, DWORD), (object, nullptr, 0)) \
    X(5, HRESULT, AllocateWaveData, (INT), (void*, INT), (object, 0)) \
    X(6, HRESULT, FreeWaveData, (), (void*), (object)) \
    X(7, HRESULT, SetWaveFormat, (LPVOID), (void*, LPVOID), (object, nullptr)) \
    X(8, HRESULT, GetWaveFormat, (LPVOID), (void*, LPVOID), (object, nullptr)) \
    X(9, HRESULT, GetWaveSize, (), (void*), (object)) \
    X(10, HRESULT, GetType, (LPDWORD), (void*, LPDWORD), (object, nullptr)) \
    X(11, HRESULT, Lock, (DWORD, DWORD, LPVOID *, LPDWORD, LPVOID *, LPDWORD, DWORD), \
        (void*, DWORD, DWORD, LPVOID *, LPDWORD, LPVOID *, LPDWORD, DWORD), \
        (object, 0, 0, nullptr, nullptr, nullptr, nullptr, 0)) \
    X(12, HRESULT, Unlock, (LPVOID, DWORD, LPVOID, DWORD), \
        (void*, LPVOID, DWORD, LPVOID, DWORD), \
        (object, nullptr, 0, nullptr, 0)) \
    X(13, HRESULT, Play, (INT), (void*, INT), (object, 0)) \
    X(14, HRESULT, Stop, (), (void*), (object)) \
    X(15, HRESULT, Rewind, (), (void*), (object)) \
    X(16, HRESULT, SetWaveTime, (A3DVAL), (void*, A3DVAL), (object, 0)) \
    X(17, HRESULT, GetWaveTime, (LPA3DVAL), (void*, LPA3DVAL), (object, nullptr)) \
    X(18, HRESULT, SetWavePosition, (DWORD), (void*, DWORD), (object, 0)) \
    X(19, HRESULT, GetWavePosition, (LPDWORD), (void*, LPDWORD), (object, nullptr)) \
    X(20, HRESULT, SetPosition3f, (A3DVAL, A3DVAL, A3DVAL), (void*, A3DVAL, A3DVAL, A3DVAL), (object, 0, 0, 0)) \
    X(21, HRESULT, GetPosition3f, (LPA3DVAL, LPA3DVAL, LPA3DVAL), \
        (void*, LPA3DVAL, LPA3DVAL, LPA3DVAL), \
        (object, nullptr, nullptr, nullptr)) \
    X(22, HRESULT, SetPosition3fv, (LPA3DVAL), (void*, LPA3DVAL), (object, nullptr)) \
    X(23, HRESULT, GetPosition3fv, (LPA3DVAL), (void*, LPA3DVAL), (object, nullptr)) \
    X(24, HRESULT, SetOrientationAngles3f, (A3DVAL, A3DVAL, A3DVAL), \
        (void*, A3DVAL, A3DVAL, A3DVAL), \
        (object, 0, 0, 0)) \
    X(25, HRESULT, GetOrientationAngles3f, (LPA3DVAL, LPA3DVAL, LPA3DVAL), \
        (void*, LPA3DVAL, LPA3DVAL, LPA3DVAL), \
        (object, nullptr, nullptr, nullptr)) \
    X(26, HRESULT, SetOrientationAngles3fv, (LPA3DVAL), (void*, LPA3DVAL), (object, nullptr)) \
    X(27, HRESULT, GetOrientationAngles3fv, (LPA3DVAL), (void*, LPA3DVAL), (object, nullptr)) \
    X(28, HRESULT, SetOrientation6f, (A3DVAL, A3DVAL, A3DVAL, A3DVAL, A3DVAL, A3DVAL), \
        (void*, A3DVAL, A3DVAL, A3DVAL, A3DVAL, A3DVAL, A3DVAL), \
        (object, 0, 0, 0, 0, 0, 0)) \
    X(29, HRESULT, GetOrientation6f, (LPA3DVAL, LPA3DVAL, LPA3DVAL, LPA3DVAL, LPA3DVAL, LPA3DVAL), \
        (void*, LPA3DVAL, LPA3DVAL, LPA3DVAL, LPA3DVAL, LPA3DVAL, LPA3DVAL), \
        (object, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr)) \
    X(30, HRESULT, SetOrientation6fv, (LPA3DVAL), (void*, LPA3DVAL), (object, nullptr)) \
    X(31, HRESULT, GetOrientation6fv, (LPA3DVAL), (void*, LPA3DVAL), (object, nullptr)) \
    X(32, HRESULT, SetVelocity3f, (A3DVAL, A3DVAL, A3DVAL), (void*, A3DVAL, A3DVAL, A3DVAL), (object, 0, 0, 0)) \
    X(33, HRESULT, GetVelocity3f, (LPA3DVAL, LPA3DVAL, LPA3DVAL), \
        (void*, LPA3DVAL, LPA3DVAL, LPA3DVAL), \
        (object, nullptr, nullptr, nullptr)) \
    X(34, HRESULT, SetVelocity3fv, (LPA3DVAL), (void*, LPA3DVAL), (object, nullptr)) \
    X(35, HRESULT, GetVelocity3fv, (LPA3DVAL), (void*, LPA3DVAL), (object, nullptr)) \
    X(36, HRESULT, SetCone, (A3DVAL, A3DVAL, A3DVAL), (void*, A3DVAL, A3DVAL, A3DVAL), (object, 0, 0, 0)) \
    X(37, HRESULT, GetCone, (LPA3DVAL, LPA3DVAL, LPA3DVAL), \
        (void*, LPA3DVAL, LPA3DVAL, LPA3DVAL), \
        (object, nullptr, nullptr, nullptr)) \
    X(38, HRESULT, SetMinMaxDistance, (A3DVAL, A3DVAL, DWORD), (void*, A3DVAL, A3DVAL, DWORD), (object, 0, 0, 0)) \
    X(39, HRESULT, GetMinMaxDistance, (LPA3DVAL, LPA3DVAL, LPDWORD), \
        (void*, LPA3DVAL, LPA3DVAL, LPDWORD), \
        (object, nullptr, nullptr, nullptr)) \
    X(40, HRESULT, SetGain, (A3DVAL), (void*, A3DVAL), (object, 0)) \
    X(41, HRESULT, GetGain, (LPA3DVAL), (void*, LPA3DVAL), (object, nullptr)) \
    X(42, HRESULT, SetPitch, (A3DVAL), (void*, A3DVAL), (object, 0)) \
    X(43, HRESULT, GetPitch, (LPA3DVAL), (void*, LPA3DVAL), (object, nullptr)) \
    X(44, HRESULT, SetDopplerScale, (A3DVAL), (void*, A3DVAL), (object, 0)) \
    X(45, HRESULT, GetDopplerScale, (LPA3DVAL), (void*, LPA3DVAL), (object, nullptr)) \
    X(46, HRESULT, SetDistanceModelScale, (A3DVAL), (void*, A3DVAL), (object, 0)) \
    X(47, HRESULT, GetDistanceModelScale, (LPA3DVAL), (void*, LPA3DVAL), (object, nullptr)) \
    X(48, HRESULT, SetEq, (A3DVAL), (void*, A3DVAL), (object, 0)) \
    X(49, HRESULT, GetEq, (LPA3DVAL), (void*, LPA3DVAL), (object, nullptr)) \
    X(50, HRESULT, SetPriority, (A3DVAL), (void*, A3DVAL), (object, 0)) \
    X(51, HRESULT, GetPriority, (LPA3DVAL), (void*, LPA3DVAL), (object, nullptr)) \
    X(52, HRESULT, SetRenderMode, (DWORD), (void*, DWORD), (object, 0)) \
    X(53, HRESULT, GetRenderMode, (LPDWORD), (void*, LPDWORD), (object, nullptr)) \
    X(54, HRESULT, GetAudibility, (LPA3DVAL), (void*, LPA3DVAL), (object, nullptr)) \
    X(55, HRESULT, GetOcclusionFactor, (LPA3DVAL), (void*, LPA3DVAL), (object, nullptr)) \
    X(56, HRESULT, GetStatus, (LPDWORD), (void*, LPDWORD), (object, nullptr)) \
    X(57, HRESULT, SetPanValues, (DWORD, LPA3DVAL), (void*, DWORD, LPA3DVAL), (object, 0, nullptr)) \
    X(58, HRESULT, GetPanValues, (DWORD, LPA3DVAL), (void*, DWORD, LPA3DVAL), (object, 0, nullptr)) \
    X(59, HRESULT, SetWaveEvent, (DWORD, HANDLE), (void*, DWORD, HANDLE), (object, 0, nullptr)) \
    X(60, HRESULT, ClearWaveEvents, (), (void*), (object)) \
    X(61, HRESULT, SetTransformMode, (DWORD), (void*, DWORD), (object, 0)) \
    X(62, HRESULT, GetTransformMode, (LPDWORD), (void*, LPDWORD), (object, nullptr)) \
    X(63, HRESULT, SetReflectionDelayScale, (A3DVAL), (void*, A3DVAL), (object, 0)) \
    X(64, HRESULT, GetReflectionDelayScale, (LPA3DVAL), (void*, LPA3DVAL), (object, nullptr)) \
    X(65, HRESULT, SetReflectionGainScale, (A3DVAL), (void*, A3DVAL), (object, 0)) \
    X(66, HRESULT, GetReflectionGainScale, (LPA3DVAL), (void*, LPA3DVAL), (object, nullptr))
DECLARE_PROBE(IA3dSource)

#define SLOTS_IA3dListener(X) \
    X(0, HRESULT, QueryInterface, (REFIID, LPVOID FAR *), \
        (void*, REFIID, LPVOID FAR *), \
        (object, IID_IUnknown, nullptr)) \
    X(1, ULONG, AddRef, (), (void*), (object)) \
    X(2, ULONG, Release, (), (void*), (object)) \
    X(3, HRESULT, SetPosition3f, (A3DVAL, A3DVAL, A3DVAL), (void*, A3DVAL, A3DVAL, A3DVAL), (object, 0, 0, 0)) \
    X(4, HRESULT, GetPosition3f, (LPA3DVAL, LPA3DVAL, LPA3DVAL), \
        (void*, LPA3DVAL, LPA3DVAL, LPA3DVAL), \
        (object, nullptr, nullptr, nullptr)) \
    X(5, HRESULT, SetPosition3fv, (LPA3DVAL), (void*, LPA3DVAL), (object, nullptr)) \
    X(6, HRESULT, GetPosition3fv, (LPA3DVAL), (void*, LPA3DVAL), (object, nullptr)) \
    X(7, HRESULT, SetOrientationAngles3f, (A3DVAL, A3DVAL, A3DVAL), \
        (void*, A3DVAL, A3DVAL, A3DVAL), \
        (object, 0, 0, 0)) \
    X(8, HRESULT, GetOrientationAngles3f, (LPA3DVAL, LPA3DVAL, LPA3DVAL), \
        (void*, LPA3DVAL, LPA3DVAL, LPA3DVAL), \
        (object, nullptr, nullptr, nullptr)) \
    X(9, HRESULT, SetOrientationAngles3fv, (LPA3DVAL), (void*, LPA3DVAL), (object, nullptr)) \
    X(10, HRESULT, GetOrientationAngles3fv, (LPA3DVAL), (void*, LPA3DVAL), (object, nullptr)) \
    X(11, HRESULT, SetOrientation6f, (A3DVAL, A3DVAL, A3DVAL, A3DVAL, A3DVAL, A3DVAL), \
        (void*, A3DVAL, A3DVAL, A3DVAL, A3DVAL, A3DVAL, A3DVAL), \
        (object, 0, 0, 0, 0, 0, 0)) \
    X(12, HRESULT, GetOrientation6f, (LPA3DVAL, LPA3DVAL, LPA3DVAL, LPA3DVAL, LPA3DVAL, LPA3DVAL), \
        (void*, LPA3DVAL, LPA3DVAL, LPA3DVAL, LPA3DVAL, LPA3DVAL, LPA3DVAL), \
        (object, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr)) \
    X(13, HRESULT, SetOrientation6fv, (LPA3DVAL), (void*, LPA3DVAL), (object, nullptr)) \
    X(14, HRESULT, GetOrientation6fv, (LPA3DVAL), (void*, LPA3DVAL), (object, nullptr)) \
    X(15, HRESULT, SetVelocity3f, (A3DVAL, A3DVAL, A3DVAL), (void*, A3DVAL, A3DVAL, A3DVAL), (object, 0, 0, 0)) \
    X(16, HRESULT, GetVelocity3f, (LPA3DVAL, LPA3DVAL, LPA3DVAL), \
        (void*, LPA3DVAL, LPA3DVAL, LPA3DVAL), \
        (object, nullptr, nullptr, nullptr)) \
    X(17, HRESULT, SetVelocity3fv, (LPA3DVAL), (void*, LPA3DVAL), (object, nullptr)) \
    X(18, HRESULT, GetVelocity3fv, (LPA3DVAL), (void*, LPA3DVAL), (object, nullptr))
DECLARE_PROBE(IA3dListener)

#define SLOTS_IA3dList(X) \
    X(0, HRESULT, QueryInterface, (REFIID, LPVOID FAR *), \
        (void*, REFIID, LPVOID FAR *), \
        (object, IID_IUnknown, nullptr)) \
    X(1, ULONG, AddRef, (), (void*), (object)) \
    X(2, ULONG, Release, (), (void*), (object)) \
    X(3, HRESULT, Begin, (), (void*), (object)) \
    X(4, HRESULT, End, (), (void*), (object)) \
    X(5, HRESULT, Call, (), (void*), (object)) \
    X(6, HRESULT, EnableBoundingVol, (), (void*), (object))
DECLARE_PROBE(IA3dList)

#define SLOTS_IA3dMaterial(X) \
    X(0, HRESULT, QueryInterface, (REFIID, LPVOID FAR *), \
        (void*, REFIID, LPVOID FAR *), \
        (object, IID_IUnknown, nullptr)) \
    X(1, ULONG, AddRef, (), (void*), (object)) \
    X(2, ULONG, Release, (), (void*), (object)) \
    X(3, HRESULT, Load, (LPSTR), (void*, LPSTR), (object, nullptr)) \
    X(4, HRESULT, Save, (LPSTR), (void*, LPSTR), (object, nullptr)) \
    X(5, HRESULT, UnSerialize, (LPVOID, UINT), (void*, LPVOID, UINT), (object, nullptr, 0)) \
    X(6, HRESULT, Serialize, (LPVOID *, UINT *), (void*, LPVOID *, UINT *), (object, nullptr, nullptr)) \
    X(7, HRESULT, Duplicate, (LPA3DMATERIAL *), (void*, LPA3DMATERIAL *), (object, nullptr)) \
    X(8, HRESULT, SetNameID, (LPSTR), (void*, LPSTR), (object, nullptr)) \
    X(9, HRESULT, GetNameID, (LPSTR, INT), (void*, LPSTR, INT), (object, nullptr, 0)) \
    X(10, HRESULT, SelectPreset, (DWORD), (void*, DWORD), (object, 0)) \
    X(11, HRESULT, GetClosestPreset, (LPDWORD), (void*, LPDWORD), (object, nullptr)) \
    X(12, HRESULT, SetReflectance, (A3DVAL, A3DVAL), (void*, A3DVAL, A3DVAL), (object, 0, 0)) \
    X(13, HRESULT, GetReflectance, (LPA3DVAL, LPA3DVAL), (void*, LPA3DVAL, LPA3DVAL), (object, nullptr, nullptr)) \
    X(14, HRESULT, SetTransmittance, (A3DVAL, A3DVAL), (void*, A3DVAL, A3DVAL), (object, 0, 0)) \
    X(15, HRESULT, GetTransmittance, (LPA3DVAL, LPA3DVAL), (void*, LPA3DVAL, LPA3DVAL), (object, nullptr, nullptr))
DECLARE_PROBE(IA3dMaterial)

#define SLOTS_IA3dGeomTarget(X) \
    X(0, HRESULT, QueryInterface, (REFIID, void**), (void*, REFIID, void**), (object, IID_IUnknown, nullptr)) \
    X(1, ULONG, AddRef, (), (void*), (object)) \
    X(2, ULONG, Release, (), (void*), (object)) \
    X(3, HRESULT, Enable, (DWORD), (void*, DWORD), (object, 0)) \
    X(4, HRESULT, Disable, (DWORD), (void*, DWORD), (object, 0)) \
    X(5, BOOL, IsEnabled, (DWORD), (void*, DWORD), (object, 0)) \
    X(6, HRESULT, SetOcclusionMode, (DWORD), (void*, DWORD), (object, 0)) \
    X(7, HRESULT, GetOcclusionMode, (LPDWORD), (void*, LPDWORD), (object, nullptr)) \
    X(8, HRESULT, SetReflectionMode, (DWORD), (void*, DWORD), (object, 0)) \
    X(9, HRESULT, GetReflectionMode, (LPDWORD), (void*, LPDWORD), (object, nullptr)) \
    X(10, HRESULT, SetReflectionGainScale, (A3DVAL), (void*, A3DVAL), (object, 0)) \
    X(11, HRESULT, GetReflectionGainScale, (LPA3DVAL), (void*, LPA3DVAL), (object, nullptr)) \
    X(12, HRESULT, SetReflectionDelayScale, (A3DVAL), (void*, A3DVAL), (object, 0)) \
    X(13, HRESULT, GetReflectionDelayScale, (LPA3DVAL), (void*, LPA3DVAL), (object, nullptr)) \
    X(14, ULONG, PushMatrix, (), (void*), (object)) \
    X(15, ULONG, PopMatrix, (), (void*), (object)) \
    X(16, HRESULT, LoadIdentity, (), (void*), (object)) \
    X(17, HRESULT, LoadMatrix, (A3DMATRIX), (void*, A3DMATRIX), (object, nullptr)) \
    X(18, HRESULT, GetMatrix, (A3DMATRIX), (void*, A3DMATRIX), (object, nullptr)) \
    X(19, HRESULT, MultMatrix, (A3DMATRIX), (void*, A3DMATRIX), (object, nullptr)) \
    X(20, HRESULT, Translate3f, (A3DVAL, A3DVAL, A3DVAL), (void*, A3DVAL, A3DVAL, A3DVAL), (object, 0, 0, 0)) \
    X(21, HRESULT, Translate3fv, (LPA3DVAL), (void*, LPA3DVAL), (object, nullptr)) \
    X(22, HRESULT, Rotate3f, (A3DVAL, A3DVAL, A3DVAL, A3DVAL), \
        (void*, A3DVAL, A3DVAL, A3DVAL, A3DVAL), \
        (object, 0, 0, 0, 0)) \
    X(23, HRESULT, Rotate3fv, (A3DVAL, LPA3DVAL), (void*, A3DVAL, LPA3DVAL), (object, 0, nullptr)) \
    X(24, HRESULT, Scale3f, (A3DVAL, A3DVAL, A3DVAL), (void*, A3DVAL, A3DVAL, A3DVAL), (object, 0, 0, 0)) \
    X(25, HRESULT, Scale3fv, (LPA3DVAL), (void*, LPA3DVAL), (object, nullptr)) \
    X(26, HRESULT, Begin, (DWORD), (void*, DWORD), (object, 0)) \
    X(27, HRESULT, End, (), (void*), (object)) \
    X(28, HRESULT, Vertex3f, (A3DVAL, A3DVAL, A3DVAL), (void*, A3DVAL, A3DVAL, A3DVAL), (object, 0, 0, 0)) \
    X(29, HRESULT, Vertex3fv, (LPA3DVAL), (void*, LPA3DVAL), (object, nullptr)) \
    X(30, HRESULT, Normal3f, (A3DVAL, A3DVAL, A3DVAL), (void*, A3DVAL, A3DVAL, A3DVAL), (object, 0, 0, 0)) \
    X(31, HRESULT, Normal3fv, (LPA3DVAL), (void*, LPA3DVAL), (object, nullptr)) \
    X(32, HRESULT, Tag, (DWORD), (void*, DWORD), (object, 0)) \
    X(33, HRESULT, SetOpeningFactorf, (A3DVAL), (void*, A3DVAL), (object, 0)) \
    X(34, HRESULT, SetOpeningFactorfv, (LPA3DVAL), (void*, LPA3DVAL), (object, nullptr)) \
    X(35, HRESULT, NewMaterial, (LPA3DMATERIAL*), (void*, LPA3DMATERIAL*), (object, nullptr)) \
    X(36, HRESULT, BindMaterial, (LPA3DMATERIAL), (void*, LPA3DMATERIAL), (object, nullptr)) \
    X(37, HRESULT, NewList, (LPA3DLIST*), (void*, LPA3DLIST*), (object, nullptr)) \
    X(38, HRESULT, BindListener, (), (void*), (object)) \
    X(39, HRESULT, BindSource, (LPA3DSOURCE), (void*, LPA3DSOURCE), (object, nullptr)) \
    X(40, HRESULT, NewEnvironment, (LPA3DENVIRONMENT*), (void*, LPA3DENVIRONMENT*), (object, nullptr)) \
    X(41, HRESULT, BindEnvironment, (LPA3DENVIRONMENT), (void*, LPA3DENVIRONMENT), (object, nullptr))
DECLARE_PROBE(IA3dGeomTarget)

#define SLOTS_IA3dListenerTarget(X) \
    X(0, HRESULT, QueryInterface, (REFIID, void**), (void*, REFIID, void**), (object, IID_IUnknown, nullptr)) \
    X(1, ULONG, AddRef, (), (void*), (object)) \
    X(2, ULONG, Release, (), (void*), (object)) \
    X(3, HRESULT, ListenerSetPosition3f, (A3DVAL, A3DVAL, A3DVAL), (void*, A3DVAL, A3DVAL, A3DVAL), (object, 0, 0, 0)) \
    X(4, HRESULT, ListenerGetPosition3f, (LPA3DVAL, LPA3DVAL, LPA3DVAL), \
        (void*, LPA3DVAL, LPA3DVAL, LPA3DVAL), \
        (object, nullptr, nullptr, nullptr)) \
    X(5, HRESULT, ListenerSetPosition3fv, (LPA3DVAL), (void*, LPA3DVAL), (object, nullptr)) \
    X(6, HRESULT, ListenerGetPosition3fv, (LPA3DVAL), (void*, LPA3DVAL), (object, nullptr)) \
    X(7, HRESULT, ListenerSetOrientationAngles3f, (A3DVAL, A3DVAL, A3DVAL), \
        (void*, A3DVAL, A3DVAL, A3DVAL), \
        (object, 0, 0, 0)) \
    X(8, HRESULT, ListenerGetOrientationAngles3f, (LPA3DVAL, LPA3DVAL, LPA3DVAL), \
        (void*, LPA3DVAL, LPA3DVAL, LPA3DVAL), \
        (object, nullptr, nullptr, nullptr)) \
    X(9, HRESULT, ListenerSetOrientationAngles3fv, (LPA3DVAL), (void*, LPA3DVAL), (object, nullptr)) \
    X(10, HRESULT, ListenerGetOrientationAngles3fv, (LPA3DVAL), (void*, LPA3DVAL), (object, nullptr)) \
    X(11, HRESULT, ListenerSetOrientation6f, (A3DVAL, A3DVAL, A3DVAL, A3DVAL, A3DVAL, A3DVAL), \
        (void*, A3DVAL, A3DVAL, A3DVAL, A3DVAL, A3DVAL, A3DVAL), \
        (object, 0, 0, 0, 0, 0, 0)) \
    X(12, HRESULT, ListenerGetOrientation6f, (LPA3DVAL, LPA3DVAL, LPA3DVAL, LPA3DVAL, LPA3DVAL, LPA3DVAL), \
        (void*, LPA3DVAL, LPA3DVAL, LPA3DVAL, LPA3DVAL, LPA3DVAL, LPA3DVAL), \
        (object, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr)) \
    X(13, HRESULT, ListenerSetOrientation6fv, (LPA3DVAL), (void*, LPA3DVAL), (object, nullptr)) \
    X(14, HRESULT, ListenerGetOrientation6fv, (LPA3DVAL), (void*, LPA3DVAL), (object, nullptr)) \
    X(15, HRESULT, ListenerSetVelocity3f, (A3DVAL, A3DVAL, A3DVAL), \
        (void*, A3DVAL, A3DVAL, A3DVAL), \
        (object, 0, 0, 0)) \
    X(16, HRESULT, ListenerGetVelocity3f, (LPA3DVAL, LPA3DVAL, LPA3DVAL), \
        (void*, LPA3DVAL, LPA3DVAL, LPA3DVAL), \
        (object, nullptr, nullptr, nullptr)) \
    X(17, HRESULT, ListenerSetVelocity3fv, (LPA3DVAL), (void*, LPA3DVAL), (object, nullptr)) \
    X(18, HRESULT, ListenerGetVelocity3fv, (LPA3DVAL), (void*, LPA3DVAL), (object, nullptr))
DECLARE_PROBE(IA3dListenerTarget)

#define SLOTS_IA3dSceneFactoryTarget(X) \
    X(0, HRESULT, QueryInterface, (REFIID, void**), (void*, REFIID, void**), (object, IID_IUnknown, nullptr)) \
    X(1, ULONG, AddRef, (), (void*), (object)) \
    X(2, ULONG, Release, (), (void*), (object)) \
    X(3, HRESULT, CreateScene, (DWORD, void**), (void*, DWORD, void**), (object, 0, nullptr)) \
    X(4, HRESULT, ProcessScene, (void*), (void*, void*), (object, nullptr)) \
    X(5, HRESULT, CreateGeometryGroup, (DWORD, void**), (void*, DWORD, void**), (object, 0, nullptr)) \
    X(6, HRESULT, CreateWallBuilder, (DWORD, void**), (void*, DWORD, void**), (object, 0, nullptr)) \
    X(7, HRESULT, CreatePortal, (DWORD, void**), (void*, DWORD, void**), (object, 0, nullptr))
DECLARE_PROBE(IA3dSceneFactoryTarget)

#define SLOTS_IA3dCompatibilityTarget(X) \
    X(0, HRESULT, QueryInterface, (REFIID, void**), (void*, REFIID, void**), (object, IID_IUnknown, nullptr)) \
    X(1, ULONG, AddRef, (), (void*), (object)) \
    X(2, ULONG, Release, (), (void*), (object)) \
    X(3, HRESULT, CompatibilityOperation, (DWORD), (void*, DWORD), (object, 0)) \
    X(4, HRESULT, CompatibilityOperationPair, (DWORD, DWORD), (void*, DWORD, DWORD), (object, 0, 0)) \
    X(5, HRESULT, CompatibilityOperationAlternate, (DWORD), (void*, DWORD), (object, 0))
DECLARE_PROBE(IA3dCompatibilityTarget)

#define SLOTS_IA3dSource202(X) \
    X(0, HRESULT, QueryInterface, (REFIID, void**), (void*, REFIID, void**), (object, IID_IUnknown, nullptr)) \
    X(1, ULONG, AddRef, (), (void*), (object)) \
    X(2, ULONG, Release, (), (void*), (object)) \
    X(3, HRESULT, LoadWaveFile, (LPSTR), (void*, LPSTR), (object, nullptr)) \
    X(4, HRESULT, LoadWaveData, (LPVOID, DWORD), (void*, LPVOID, DWORD), (object, nullptr, 0)) \
    X(5, HRESULT, AllocateWaveData, (INT), (void*, INT), (object, 0)) \
    X(6, HRESULT, FreeWaveData, (), (void*), (object)) \
    X(7, HRESULT, SetWaveFormat, (LPVOID), (void*, LPVOID), (object, nullptr)) \
    X(8, HRESULT, GetWaveFormat, (LPVOID), (void*, LPVOID), (object, nullptr)) \
    X(9, HRESULT, GetWaveSize, (), (void*), (object)) \
    X(10, HRESULT, GetType, (LPDWORD), (void*, LPDWORD), (object, nullptr)) \
    X(11, HRESULT, Lock, (DWORD, DWORD, LPVOID*, LPDWORD, LPVOID*, LPDWORD, DWORD), \
        (void*, DWORD, DWORD, LPVOID*, LPDWORD, LPVOID*, LPDWORD, DWORD), \
        (object, 0, 0, nullptr, nullptr, nullptr, nullptr, 0)) \
    X(12, HRESULT, Unlock, (LPVOID, DWORD, LPVOID, DWORD), \
        (void*, LPVOID, DWORD, LPVOID, DWORD), \
        (object, nullptr, 0, nullptr, 0)) \
    X(13, HRESULT, Play, (INT), (void*, INT), (object, 0)) \
    X(14, HRESULT, Stop, (), (void*), (object)) \
    X(15, HRESULT, Rewind, (), (void*), (object)) \
    X(16, HRESULT, SetWaveTime, (A3DVAL), (void*, A3DVAL), (object, 0)) \
    X(17, HRESULT, GetWaveTime, (LPA3DVAL), (void*, LPA3DVAL), (object, nullptr)) \
    X(18, HRESULT, SetWavePosition, (DWORD), (void*, DWORD), (object, 0)) \
    X(19, HRESULT, GetWavePosition, (LPDWORD), (void*, LPDWORD), (object, nullptr)) \
    X(20, HRESULT, SetPosition3f, (A3DVAL, A3DVAL, A3DVAL), (void*, A3DVAL, A3DVAL, A3DVAL), (object, 0, 0, 0)) \
    X(21, HRESULT, GetPosition3f, (LPA3DVAL, LPA3DVAL, LPA3DVAL), \
        (void*, LPA3DVAL, LPA3DVAL, LPA3DVAL), \
        (object, nullptr, nullptr, nullptr)) \
    X(22, HRESULT, SetPosition3fv, (LPA3DVAL), (void*, LPA3DVAL), (object, nullptr)) \
    X(23, HRESULT, GetPosition3fv, (LPA3DVAL), (void*, LPA3DVAL), (object, nullptr)) \
    X(24, HRESULT, SetOrientationAngles3f, (A3DVAL, A3DVAL, A3DVAL), \
        (void*, A3DVAL, A3DVAL, A3DVAL), \
        (object, 0, 0, 0)) \
    X(25, HRESULT, GetOrientationAngles3f, (LPA3DVAL, LPA3DVAL, LPA3DVAL), \
        (void*, LPA3DVAL, LPA3DVAL, LPA3DVAL), \
        (object, nullptr, nullptr, nullptr)) \
    X(26, HRESULT, SetOrientationAngles3fv, (LPA3DVAL), (void*, LPA3DVAL), (object, nullptr)) \
    X(27, HRESULT, GetOrientationAngles3fv, (LPA3DVAL), (void*, LPA3DVAL), (object, nullptr)) \
    X(28, HRESULT, SetOrientation6f, (A3DVAL, A3DVAL, A3DVAL, A3DVAL, A3DVAL, A3DVAL), \
        (void*, A3DVAL, A3DVAL, A3DVAL, A3DVAL, A3DVAL, A3DVAL), \
        (object, 0, 0, 0, 0, 0, 0)) \
    X(29, HRESULT, GetOrientation6f, (LPA3DVAL, LPA3DVAL, LPA3DVAL, LPA3DVAL, LPA3DVAL, LPA3DVAL), \
        (void*, LPA3DVAL, LPA3DVAL, LPA3DVAL, LPA3DVAL, LPA3DVAL, LPA3DVAL), \
        (object, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr)) \
    X(30, HRESULT, SetOrientation6fv, (LPA3DVAL), (void*, LPA3DVAL), (object, nullptr)) \
    X(31, HRESULT, GetOrientation6fv, (LPA3DVAL), (void*, LPA3DVAL), (object, nullptr)) \
    X(32, HRESULT, SetVelocity3f, (A3DVAL, A3DVAL, A3DVAL), (void*, A3DVAL, A3DVAL, A3DVAL), (object, 0, 0, 0)) \
    X(33, HRESULT, GetVelocity3f, (LPA3DVAL, LPA3DVAL, LPA3DVAL), \
        (void*, LPA3DVAL, LPA3DVAL, LPA3DVAL), \
        (object, nullptr, nullptr, nullptr)) \
    X(34, HRESULT, SetVelocity3fv, (LPA3DVAL), (void*, LPA3DVAL), (object, nullptr)) \
    X(35, HRESULT, GetVelocity3fv, (LPA3DVAL), (void*, LPA3DVAL), (object, nullptr)) \
    X(36, HRESULT, SetCone, (A3DVAL, A3DVAL, A3DVAL), (void*, A3DVAL, A3DVAL, A3DVAL), (object, 0, 0, 0)) \
    X(37, HRESULT, GetCone, (LPA3DVAL, LPA3DVAL, LPA3DVAL), \
        (void*, LPA3DVAL, LPA3DVAL, LPA3DVAL), \
        (object, nullptr, nullptr, nullptr)) \
    X(38, HRESULT, SetMinMaxDistance, (A3DVAL, A3DVAL, DWORD), (void*, A3DVAL, A3DVAL, DWORD), (object, 0, 0, 0)) \
    X(39, HRESULT, GetMinMaxDistance, (LPA3DVAL, LPA3DVAL, LPDWORD), \
        (void*, LPA3DVAL, LPA3DVAL, LPDWORD), \
        (object, nullptr, nullptr, nullptr)) \
    X(40, HRESULT, SetGain, (A3DVAL), (void*, A3DVAL), (object, 0)) \
    X(41, HRESULT, GetGain, (LPA3DVAL), (void*, LPA3DVAL), (object, nullptr)) \
    X(42, HRESULT, SetPitch, (A3DVAL), (void*, A3DVAL), (object, 0)) \
    X(43, HRESULT, GetPitch, (LPA3DVAL), (void*, LPA3DVAL), (object, nullptr)) \
    X(44, HRESULT, SetDopplerScale, (A3DVAL), (void*, A3DVAL), (object, 0)) \
    X(45, HRESULT, GetDopplerScale, (LPA3DVAL), (void*, LPA3DVAL), (object, nullptr)) \
    X(46, HRESULT, SetDistanceModelScale, (A3DVAL), (void*, A3DVAL), (object, 0)) \
    X(47, HRESULT, GetDistanceModelScale, (LPA3DVAL), (void*, LPA3DVAL), (object, nullptr)) \
    X(48, HRESULT, SetEq, (A3DVAL), (void*, A3DVAL), (object, 0)) \
    X(49, HRESULT, GetEq, (LPA3DVAL), (void*, LPA3DVAL), (object, nullptr)) \
    X(50, HRESULT, SetPriority, (A3DVAL), (void*, A3DVAL), (object, 0)) \
    X(51, HRESULT, GetPriority, (LPA3DVAL), (void*, LPA3DVAL), (object, nullptr)) \
    X(52, HRESULT, SetRenderMode, (DWORD), (void*, DWORD), (object, 0)) \
    X(53, HRESULT, GetRenderMode, (LPDWORD), (void*, LPDWORD), (object, nullptr)) \
    X(54, HRESULT, GetAudibility, (LPA3DVAL), (void*, LPA3DVAL), (object, nullptr)) \
    X(55, HRESULT, GetOcclusionFactor, (LPA3DVAL), (void*, LPA3DVAL), (object, nullptr)) \
    X(56, HRESULT, GetStatus, (LPDWORD), (void*, LPDWORD), (object, nullptr))
DECLARE_PROBE(IA3dSource202)

#define SLOTS_IA3dSourcePrivate(X) \
    X(0, HRESULT, QueryInterface, (REFIID, void**), (void*, REFIID, void**), (object, IID_IUnknown, nullptr)) \
    X(1, ULONG, AddRef, (), (void*), (object)) \
    X(2, ULONG, Release, (), (void*), (object)) \
    X(3, HRESULT, CompatibilityOperation, (DWORD), (void*, DWORD), (object, 0)) \
    X(4, HRESULT, InitializeReflectionSlots, (A3dSourceReflection**), (void*, A3dSourceReflection**), (object, nullptr))
DECLARE_PROBE(IA3dSourcePrivate)

#define SLOTS_IA3dSourceRenderBuffer(X) \
    X(0, HRESULT, QueryInterface, (REFIID, void**), (void*, REFIID, void**), (object, IID_IUnknown, nullptr)) \
    X(1, ULONG, AddRef, (), (void*), (object)) \
    X(2, ULONG, Release, (), (void*), (object)) \
    X(3, HRESULT, SetA3dSuperCtrl, (A3DCTRL_SRC_SUPER*, DWORD), \
        (void*, A3DCTRL_SRC_SUPER*, DWORD), \
        (object, nullptr, 0))
DECLARE_PROBE(IA3dSourceRenderBuffer)

#define SLOTS_IA3dOpeningTarget(X) \
    X(0, HRESULT, QueryInterface, (REFIID, void**), (void*, REFIID, void**), (object, IID_IUnknown, nullptr)) \
    X(1, ULONG, AddRef, (), (void*), (object)) \
    X(2, ULONG, Release, (), (void*), (object)) \
    X(3, HRESULT, GetParent, (IUnknown**), (void*, IUnknown**), (object, nullptr)) \
    X(4, HRESULT, SetOpeningFactor, (A3DVAL), (void*, A3DVAL), (object, 0)) \
    X(5, HRESULT, GetOpeningFactor, (A3DVAL*), (void*, A3DVAL*), (object, nullptr)) \
    X(6, HRESULT, SetPosition, (CA3dTransform*, const A3DVAL*), \
        (void*, CA3dTransform*, const A3DVAL*), \
        (object, nullptr, nullptr)) \
    X(7, HRESULT, GetPosition, (CA3dTransform*, A3DVAL*), (void*, CA3dTransform*, A3DVAL*), (object, nullptr, nullptr))
DECLARE_PROBE(IA3dOpeningTarget)

#define SLOTS_IA3dDalBuffer(X) \
    X(0, HRESULT, QueryInterface, (REFIID, void**), (void*, REFIID, void**), (object, IID_IUnknown, nullptr)) \
    X(1, ULONG, AddRef, (), (void*), (object)) \
    X(2, ULONG, Release, (), (void*), (object)) \
    X(3, HRESULT, SetA3dSuperCtrl, (void*, DWORD), (void*, void*, DWORD), (object, nullptr, 0)) \
    X(4, HRESULT, GetAllocationStatus, (DWORD), (void*, DWORD), (object, 0)) \
    X(5, HRESULT, GetWave, (DWORD), (void*, DWORD), (object, 0)) \
    X(6, HRESULT, GetDriverInfo, (DWORD, DWORD), (void*, DWORD, DWORD), (object, 0, 0)) \
    X(7, HRESULT, SetNewBuffer, (DWORD, DWORD), (void*, DWORD, DWORD), (object, 0, 0)) \
    X(8, HRESULT, SetA3dDirectCtrl, (DWORD, DWORD), (void*, DWORD, DWORD), (object, 0, 0))
DECLARE_PROBE(IA3dDalBuffer)

#define SLOTS_IResManBuffer(X) \
    X(0, HRESULT, QueryInterface, (REFIID, void**), (void*, REFIID, void**), (object, IID_IUnknown, nullptr)) \
    X(1, ULONG, AddRef, (), (void*), (object)) \
    X(2, ULONG, Release, (), (void*), (object)) \
    X(3, HRESULT, GetStatusEx, (DWORD), (void*, DWORD), (object, 0)) \
    X(4, HRESULT, GetPriority, (LPDWORD), (void*, LPDWORD), (object, nullptr)) \
    X(5, HRESULT, GetBufferState, (LPDWORD), (void*, LPDWORD), (object, nullptr)) \
    X(6, HRESULT, SetPriority, (DWORD), (void*, DWORD), (object, 0)) \
    X(7, HRESULT, Mute, (), (void*), (object)) \
    X(8, HRESULT, UnMute, (), (void*), (object))
DECLARE_PROBE(IResManBuffer)

#define SLOTS_IA3dDal(X) \
    X(0, HRESULT, QueryInterface, (REFIID, void**), (void*, REFIID, void**), (object, IID_IUnknown, nullptr)) \
    X(1, ULONG, AddRef, (), (void*), (object)) \
    X(2, ULONG, Release, (), (void*), (object)) \
    X(3, HRESULT, InitializeEx, (DWORD, DWORD, void*, void*), \
        (void*, DWORD, DWORD, void*, void*), \
        (object, 0, 0, nullptr, nullptr)) \
    X(4, HRESULT, CreateSoundBufferEx, (const a3d_platform_buffer_desc_t*, BYTE*, IDirectSoundBuffer**, IUnknown*), \
        (void*, const a3d_platform_buffer_desc_t*, BYTE*, IDirectSoundBuffer**, IUnknown*), \
        (object, nullptr, nullptr, nullptr, nullptr)) \
    X(5, HRESULT, GetA3dCaps, (void*, void*), (void*, void*, void*), (object, nullptr, nullptr)) \
    X(6, HRESULT, GetDriverInfo, (void*, void*, void*, void*), \
        (void*, void*, void*, void*, void*), \
        (object, nullptr, nullptr, nullptr, nullptr)) \
    X(7, HRESULT, GetDS, (IDirectSound**), (void*, IDirectSound**), (object, nullptr)) \
    X(8, HRESULT, GetDSDriverDesc, (void*, void*), (void*, void*, void*), (object, nullptr, nullptr)) \
    X(9, HRESULT, QueryFunctionality, (DWORD, void*), (void*, DWORD, void*), (object, 0, nullptr)) \
    X(10, HRESULT, DeviceCompatibility5, (void*, void*, void*), \
        (void*, void*, void*, void*), \
        (object, nullptr, nullptr, nullptr))
DECLARE_PROBE(IA3dDal)

#define SLOTS_IA3dNativeDeviceV1(X) \
    X(0, HRESULT, QueryInterface, (REFIID, void**), (void*, REFIID, void**), (object, IID_IUnknown, nullptr)) \
    X(1, ULONG, AddRef, (), (void*), (object)) \
    X(2, ULONG, Release, (), (void*), (object)) \
    X(3, HRESULT, InitializeDevice, (const GUID*, DWORD, DWORD, DWORD*), \
        (void*, const GUID*, DWORD, DWORD, DWORD*), \
        (object, nullptr, 0, 0, nullptr))
DECLARE_PROBE(IA3dNativeDeviceV1)
