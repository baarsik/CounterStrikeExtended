#pragma once

#include "interface.h"
#include "cse_vgui.h"

// MSVC novtable interfaces have no base implementation. Native Itanium RTTI
// needs that interface-only contract expressed without an undefined key function.
#if defined(_WIN32)
#define CSE_GAMECONSOLE_ABSTRACT_METHOD
#else
#define CSE_GAMECONSOLE_ABSTRACT_METHOD = 0
#endif

#ifdef _MSC_VER
class __declspec( novtable ) IGameConsole : public IBaseInterface
#else
class IGameConsole : public IBaseInterface
#endif
{
public:
    virtual void Activate() CSE_GAMECONSOLE_ABSTRACT_METHOD;
    virtual void Initialize() CSE_GAMECONSOLE_ABSTRACT_METHOD;
    virtual void Hide() CSE_GAMECONSOLE_ABSTRACT_METHOD;
    virtual void Clear() CSE_GAMECONSOLE_ABSTRACT_METHOD;
    virtual bool IsConsoleVisible() CSE_GAMECONSOLE_ABSTRACT_METHOD;

    virtual void Printf( const char* format, ... ) CSE_GAMECONSOLE_ABSTRACT_METHOD;
    virtual void DPrintf( const char* format, ... ) CSE_GAMECONSOLE_ABSTRACT_METHOD;

    virtual void SetParent( vgui2::VPANEL parent ) CSE_GAMECONSOLE_ABSTRACT_METHOD;
};

#undef CSE_GAMECONSOLE_ABSTRACT_METHOD
