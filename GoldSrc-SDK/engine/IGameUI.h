#pragma once

#include "interface.h"

struct cl_enginefuncs_s;
class IBaseSystem;

// MSVC novtable interfaces have no base implementation. Native Itanium RTTI
// needs that interface-only contract expressed without an undefined key function.
#if defined(_WIN32)
#define CSE_GAMEUI_ABSTRACT_METHOD
#else
#define CSE_GAMEUI_ABSTRACT_METHOD = 0
#endif

#ifdef _MSC_VER
class __declspec( novtable ) IGameUI : public IBaseInterface
#else
class IGameUI : public IBaseInterface
#endif
{
public:
    virtual void Initialize( CreateInterfaceFn* factories, int count ) CSE_GAMEUI_ABSTRACT_METHOD;
    virtual void Start(cl_enginefuncs_s* engineFunctions, int interfaceVersion, IBaseSystem* system)
        CSE_GAMEUI_ABSTRACT_METHOD;
    virtual void Shutdown() CSE_GAMEUI_ABSTRACT_METHOD;

    virtual int ActivateGameUI() CSE_GAMEUI_ABSTRACT_METHOD;
    virtual int ActivateDemoUI() CSE_GAMEUI_ABSTRACT_METHOD;
    virtual int HasExclusiveInput() CSE_GAMEUI_ABSTRACT_METHOD;
    virtual void RunFrame() CSE_GAMEUI_ABSTRACT_METHOD;

    virtual void ConnectToServer( const char* game, int address, int port ) CSE_GAMEUI_ABSTRACT_METHOD;
    virtual void DisconnectFromServer() CSE_GAMEUI_ABSTRACT_METHOD;

    virtual void HideGameUI() CSE_GAMEUI_ABSTRACT_METHOD;
    virtual bool IsGameUIActive() CSE_GAMEUI_ABSTRACT_METHOD;

    virtual void LoadingStarted( const char* resourceType, const char* resourceName ) CSE_GAMEUI_ABSTRACT_METHOD;
    virtual void LoadingFinished( const char* resourceType, const char* resourceName ) CSE_GAMEUI_ABSTRACT_METHOD;
    virtual void StartProgressBar( const char* progressType, int steps ) CSE_GAMEUI_ABSTRACT_METHOD;
    virtual int ContinueProgressBar( int point, float fraction ) CSE_GAMEUI_ABSTRACT_METHOD;
    virtual void StopProgressBar(bool error, const char* failureReason, const char* extendedReason)
        CSE_GAMEUI_ABSTRACT_METHOD;
    virtual int SetProgressBarStatusText( const char* statusText ) CSE_GAMEUI_ABSTRACT_METHOD;
    virtual void SetSecondaryProgressBar( float progress ) CSE_GAMEUI_ABSTRACT_METHOD;
    virtual void SetSecondaryProgressBarText( const char* statusText ) CSE_GAMEUI_ABSTRACT_METHOD;

    virtual void ValidateCDKey( bool force, bool connect ) CSE_GAMEUI_ABSTRACT_METHOD;
    virtual void OnDisconnectFromServer( int failureCode, const char* username ) CSE_GAMEUI_ABSTRACT_METHOD;

    virtual void SetIsRunningOnSteamDeck( bool running ) CSE_GAMEUI_ABSTRACT_METHOD;
    virtual bool IsRunningOnSteamDeck() const CSE_GAMEUI_ABSTRACT_METHOD;
};

#undef CSE_GAMEUI_ABSTRACT_METHOD
