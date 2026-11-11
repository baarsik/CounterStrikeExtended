#pragma once

#include "platform_com.h"

class CA3dClassFactory
#ifndef _WIN32
    final
#endif
    : public IClassFactory
{
public:
	CA3dClassFactory();
	~CA3dClassFactory();

	HRESULT STDMETHODCALLTYPE QueryInterface( REFIID iid, void** ppObject );
	ULONG STDMETHODCALLTYPE AddRef();
	ULONG STDMETHODCALLTYPE Release();
	HRESULT STDMETHODCALLTYPE CreateInstance( IUnknown* pOuter, REFIID iid, void** ppObject );
	HRESULT STDMETHODCALLTYPE LockServer( BOOL bLock );

private:
	LONG	m_cReferences;
};

extern LONG	g_cA3dObjects;
extern LONG	g_cA3dLocks;
