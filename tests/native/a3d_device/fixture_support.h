#pragma once

// Declaration adapters for exact Init/Setup/Shutdown bodies, not a complete
// CA3d ABI or source/renderer implementation. Only empty-source shutdown is used.
#include "LinkList.h"
class IA3d : public IUnknown {};
class IA3d2 : public IUnknown {};
class CA3dSource {
public:
    BOOL m_bRegistered = FALSE;
};
class CA3d {
public:
    // This component fixture covers the external typed-device/fallback boundary.
    // Owned native provider behavior is exercised by the actual-module suites.
    bool UsesNativeProvider() const {
        return false;
    }
    DWORD m_dwEnabledFeatures = 0;
    HRESULT SetupBackend(IDirectSound*, IA3d2*, IA3d*);
    HRESULT ShutdownBackend();
    HRESULT STDMETHODCALLTYPE Init(LPGUID, DWORD, DWORD);
    HRESULT Compat(DWORD operation, DWORD value) {
        compatibilityOperation = operation;
        compatibilityValue = value;
        m_bCompat1011 = TRUE;
        return S_OK;
    }
    HRESULT SetResourceManagerMode(DWORD mode) {
        resourceMode = mode;
        return S_OK;
    }
    IDirectSound* m_pDirectSound = nullptr;
    IA3d2* m_pA3d2 = nullptr;
    IA3d* m_pA3d = nullptr;
    IA3dDal* m_pBackend = nullptr;
    IUnknown* m_pPrimaryBuffer = nullptr;
    IUnknown* m_pDirectSoundListener = nullptr;
    BOOL m_bBackendInitialized = FALSE;
    DWORD m_dwBackendState = 0;
    DWORD m_dwCooperativeLevel = 0;
    DWORD m_dwRequestedFeatures = 0;
    DWORD m_dwFeaturesAvailable = 0;
    BOOL m_bCompat1011 = FALSE;
    BOOL m_bCompat1010 = FALSE;
    CA3dObjectList m_Sources{};
    DWORD compatibilityOperation = 0;
    DWORD compatibilityValue = 0;
    DWORD resourceMode = 0;
};
