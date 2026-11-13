#pragma once

#include "platform_types.h"
#ifdef _WIN32
#include <unknwn.h>
#else
// Complete consumed COM views. No virtual destructor may precede or extend
// these public slots. Native objects are rebuilt together, never shared with
// Microsoft C++ objects across the platform boundary.
#define DECLARE_INTERFACE_(name, base) struct name : public base
#define STDMETHOD(name) virtual HRESULT STDMETHODCALLTYPE name
#define STDMETHOD_(type, name) virtual type STDMETHODCALLTYPE name
#define PURE = 0
#define THIS void
#define THIS_
#define STDAPI extern "C" HRESULT STDAPICALLTYPE

struct IUnknown {
    virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void** object) = 0;
    virtual ULONG STDMETHODCALLTYPE AddRef() = 0;
    virtual ULONG STDMETHODCALLTYPE Release() = 0;
};
using LPUNKNOWN = IUnknown*;

struct IClassFactory : public IUnknown {
    virtual HRESULT STDMETHODCALLTYPE CreateInstance(IUnknown* outer, REFIID iid, void** object) = 0;
    virtual HRESULT STDMETHODCALLTYPE LockServer(BOOL lock) = 0;
};
DEFINE_GUID(IID_IUnknown, 0x00000000, 0x0000, 0x0000, 0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46);
DEFINE_GUID(IID_IClassFactory, 0x00000001, 0x0000, 0x0000, 0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46);
#endif
