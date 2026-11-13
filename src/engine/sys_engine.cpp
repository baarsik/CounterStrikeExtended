#include "iengine.h"
#include "igame.h"
#include "sys.h"
#include "host.h"
#include "cd.h"
#include "keys.h"
#include "client.h"
#include "server.h"
#include "../common/dll_state.h"
#include "native_gameui_retirement.h"
#include "interface.h"
#include <thread>
#if defined(__APPLE__)
#include <pthread.h>
#endif

#if INTPTR_MAX > INT32_MAX
namespace {
std::thread::id nativeGameUIThread;
size_t nativeGameUIUseDepth = 0;
bool nativeGameUIRetiring = false;
const cse::gameui_lifetime::HostV1 nativeGameUIHost{
    sizeof(cse::gameui_lifetime::HostV1), cse::gameui_lifetime::ContractVersion,
    cse::NativeGameUIIsMainThread, cse::NativeGameUIIsRetirementSafePoint};
void* NativeGameUIHostFactory() {
    return const_cast<cse::gameui_lifetime::HostV1*>(&nativeGameUIHost);
}
}
EXPOSE_INTERFACE_FN(NativeGameUIHostFactory, CSEGameUIHost, cse::gameui_lifetime::HostInterface);

bool cse::InitializeNativeGameUIHost() {
#if defined(__APPLE__)
    if (!pthread_main_np()) {
        return false;
    }
#endif
    if (nativeGameUIUseDepth || nativeGameUIRetiring) {
        return false;
    }
    if (nativeGameUIThread != std::thread::id{} && nativeGameUIThread != std::this_thread::get_id()) {
        return false;
    }
    nativeGameUIThread = std::this_thread::get_id();
    return true;
}

bool cse::NativeGameUIIsMainThread() {
#if defined(__APPLE__)
    if (!pthread_main_np()) {
        return false;
    }
#endif
    return nativeGameUIThread != std::thread::id{} && nativeGameUIThread == std::this_thread::get_id();
}

bool cse::NativeGameUIIsRetirementSafePoint() {
    return NativeGameUIIsMainThread() && nativeGameUIRetiring && nativeGameUIUseDepth == 0;
}

void cse::EnterNativeGameUIUse() {
    ++nativeGameUIUseDepth;
}

void cse::LeaveNativeGameUIUse() {
    --nativeGameUIUseDepth;
}

bool cse::EnterNativeGameUIRetirement() {
    if (!NativeGameUIIsMainThread() || nativeGameUIRetiring || nativeGameUIUseDepth) {
        return false;
    }
    nativeGameUIRetiring = true;
    return true;
}

void cse::LeaveNativeGameUIRetirement() {
    nativeGameUIRetiring = false;
}
#endif

#define DLL_RESTARTING	5

#define QUIT_TODESKTOP	1
#define QUIT_RESTART	2

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CEngine : public IEngine
{
public:
	CEngine();
	~CEngine() override;
	bool Load( bool dedicated, char* basedir, char* cmdline ) override;
	void Unload() override;
    bool TryUnload();
	void SetState( int state ) override;
	int GetState() override;
	void SetSubState( int state ) override;
	int GetSubState() override;
	int Frame() override;
	double GetFrameTime() override;
	double GetCurTime() override;
	void TrapKey_Event( int key, bool down ) override;
	void TrapMouse_Event( int buttons, bool down ) override;
	void StartTrapMode() override;
	bool IsTrapping() override;
	bool CheckDoneTrapping( int& buttons, int& key ) override;
	int GetQuitting() override;
	void SetQuitting( int quitting ) override;

private:
	int m_nQuitting;
	int m_nDLLState;
	int m_nSubState;
	double m_fCurTime;
	double m_fFrameTime;
	double m_fOldTime;
	bool m_bTrapMode;
	bool m_bDoneTrapping;
	int m_nTrapKey;
	int m_nTrapButtons;
};

CEngine g_Engine;
IEngine* eng = &g_Engine;

IEngine::~IEngine()
{
}

#ifdef _MSC_VER
#pragma warning( suppress: 26495 )
#endif
//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CEngine::CEngine()
{
	m_fFrameTime = 0.0;
	m_nSubState = 0;
	m_nDLLState = DLL_INACTIVE;
	m_fOldTime = 0.0;
	m_bTrapMode = false;
	m_bDoneTrapping = false;
	m_nTrapKey = 0;
	m_nTrapButtons = 0;
	m_nQuitting = 0;
}

CEngine::~CEngine()
{
}

bool CEngine::Load( bool dedicated, char* basedir, char* cmdline )
{
#if INTPTR_MAX > INT32_MAX
    cse::NativeGameUIUseScope nativeUse;
#endif
	// Activate engine
	SetState( DLL_ACTIVE );
	if ( !Sys_InitGame( cmdline, basedir, game->GetMainWindowAddress(), dedicated ) )
		return false;

	ForceReloadProfile(); // FIXME!!!!!!  Getting rid of this doesn't load the config when loading a map directly, but it is screwed with timedemo
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEngine::Unload()
{
#if INTPTR_MAX > INT32_MAX
    if (!g_bIsDedicatedServer) {
        TryUnload();
        return;
    }
#endif
	Sys_ShutdownGame();
	m_nDLLState = DLL_INACTIVE;
}

bool CEngine::TryUnload() {
#if INTPTR_MAX > INT32_MAX
    if (!g_bIsDedicatedServer && !cse::TryShutdownNativeGame()) {
        return false;
    }
    if (g_bIsDedicatedServer) {
        Sys_ShutdownGame();
    }
#else
    Sys_ShutdownGame();
#endif
    m_nDLLState = DLL_INACTIVE;
    return true;
}

bool cse::TryUnloadNativeEngine() {
    return g_Engine.TryUnload();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : iState - 
//-----------------------------------------------------------------------------
void CEngine::SetState( int state )
{
	m_nDLLState = state;
	GameSetState( state );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : int
//-----------------------------------------------------------------------------
int CEngine::GetState()
{
	return m_nDLLState;
}

void CEngine::SetSubState( int state )
{
	if ( state != ENG_NORMAL )
		GameSetSubState( state );
}

int CEngine::GetSubState()
{
	return m_nSubState;
}

int CEngine::Frame()
{
#if INTPTR_MAX > INT32_MAX
    cse::NativeGameUIUseScope nativeUse;
#endif
	cdaudio->Frame();

	if ( !game->IsActiveApp() )
		game->SleepUntilInput( m_nDLLState == DLL_PAUSED ? 50 : 20 );

	m_fCurTime = Sys_FloatTime();
	// Set frame time
	m_fFrameTime = m_fCurTime - m_fOldTime;
	m_fOldTime = m_fCurTime;
	if ( m_fFrameTime < 0.0 )
		m_fFrameTime = 0.01f;

	if ( m_nDLLState != DLL_INACTIVE )
	{
		int dummy;
		int iState = Host_Frame( (float)m_fFrameTime, m_nDLLState, &dummy );
		// Has the state changed?
		if ( iState != m_nDLLState )
		{
			SetState( iState );

			if ( m_nDLLState == DLL_CLOSE )
				SetQuitting( QUIT_TODESKTOP );
			else if ( m_nDLLState == DLL_RESTARTING )
				SetQuitting( QUIT_RESTART );
		}
	} // profile scope

	return m_nDLLState;
}
//-----------------------------------------------------------------------------
// Purpose: 
// Output : double
//-----------------------------------------------------------------------------
double CEngine::GetFrameTime()
{
	return m_fFrameTime;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : double
//-----------------------------------------------------------------------------
double CEngine::GetCurTime()
{
	return m_fCurTime;
}

void CEngine::TrapKey_Event( int key, bool down )
{
	// Only key down events are trapped
	if ( m_bTrapMode && down )
	{
		m_bTrapMode = false;
		m_bDoneTrapping = true;
		m_nTrapKey = key;
		m_nTrapButtons = 0;
	}
	else
	{
		Key_Event( key, down );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : buttons - 
//			down - 
//-----------------------------------------------------------------------------
void CEngine::TrapMouse_Event( int buttons, bool down )
{
	// buttons == 0 for mouse move events
	if ( m_bTrapMode && buttons != 0 && !down )
	{
		m_bTrapMode = false;
		m_bDoneTrapping = true;
		m_nTrapKey = 0;
		m_nTrapButtons = buttons;
	}
	else
	{
		ClientDLL_MouseEvent( buttons );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEngine::StartTrapMode()
{
	if ( !m_bTrapMode )
	{
		m_bTrapMode = true;
		m_bDoneTrapping = false;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CEngine::IsTrapping()
{
	return m_bTrapMode;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : buttons - 
//			key - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CEngine::CheckDoneTrapping( int& buttons, int& key )
{
	if ( m_bTrapMode || !m_bDoneTrapping )
		return false;

	key = m_nTrapKey;
	buttons = m_nTrapButtons;
	// Reset since we retrieved the results
	m_bDoneTrapping = false;
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Check whether we are ready to exit
//-----------------------------------------------------------------------------
int CEngine::GetQuitting()
{
	return m_nQuitting;
}

//-----------------------------------------------------------------------------
// Purpose: Flag that we are in the process of quiting
//-----------------------------------------------------------------------------
void CEngine::SetQuitting( int quitting )
{
	m_nQuitting = quitting;
}
