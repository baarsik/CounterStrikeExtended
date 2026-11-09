#include "cd.h"
#include "mpg123_music.h"
#include "ithread.h"
#include "sound_backend_config.h"
#include "voice.h"

#include "cmd.h"
#include "common.h"
#include "conprint.h"
#include "cvar.h"
#include "filesystem.h"
#include "mem.h"
#include "sys.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <chrono>

#if defined( _WIN32 )
#include <windows.h>
#include <mmsystem.h>
#include <cassert>
#if defined( NDEBUG )
extern "C" _ACRTIMP void __cdecl _wassert( const wchar_t* message, const wchar_t* file, unsigned int line );
#endif

static void usleep( unsigned int microseconds )
{
	Sleep( ( microseconds + 999u ) / 1000u );
}
#else
#include <unistd.h>
#include "../common/legacy_text_to_integer.h"

static int ParseNativeMp3Track(const char* argument) {
    const int parsed = CseLegacyAtoi(argument);
    constexpr int kMaximumTrackWord = (std::numeric_limits<std::int32_t>::max)();
    constexpr int kMinimumTrackWord = (std::numeric_limits<std::int32_t>::min)();
    // The retained Windows command increments a 32-bit atoi result, including wrap.
    return parsed == kMaximumTrackWord ? kMinimumTrackWord : parsed + 1;
}
#endif

#define MAX_MP3_TRACKS 200

#define NUM_BUILTIN_MP3_TRACKS 29

extern cvar_t bgmvolume;
extern cvar_t MP3Volume;
extern cvar_t MP3FadeTime;

struct SDL_Window;
extern SDL_Window** pmainwindow;

static cse::Mpg123MusicPlayer musicPlayer;

char* g_pszMP3trackFileMap[MAX_MP3_TRACKS];
int g_iMP3NumTracks;
int g_iMP3FirstMalloc;

static void SetStreamVolumePan(float volume) {
    musicPlayer.SetVolume(volume);
}

const float CCDAudio::TRACK_EXTRA_TIME = 2.0f;

static CCDAudio g_CDAudio;
ICDAudio* cdaudio = &g_CDAudio;

void CDAudio_Init()
{
    if (!thread->Init()) {
        Con_Printf("CDAudio: command worker initialization failed.\n");
    }
}

void CDAudio_Shutdown()
{
	thread->Shutdown();
}

#if CSE_SOUND_USE_SDL
extern "C" void SNDDMA_Shutdown();
#endif

bool CDAudio_ShutdownForModuleUnload() {
    if (!thread->Shutdown()) {
        return false;
    }
    // The worker performs full controller cleanup once. Only these independent
    // owners may be repeated after normal host shutdown or partial initialization.
    Voice_ReleaseCaptureForModuleUnload();
    Voice_ReleaseMixerForModuleUnload();
    musicPlayer.Shutdown();
#if CSE_SOUND_USE_SDL
    SNDDMA_Shutdown();
#endif
    return true;
}

cse::AudioModuleShutdown::~AudioModuleShutdown() noexcept {
    try {
        if (CDAudio_ShutdownForModuleUnload()) {
            return;
        }
    } catch (...) {
        // Returning to the launcher could unload code still used by a worker.
    }
    fputs("Fatal: owned audio cleanup failed before engine module unload.\n", stderr);
    fflush(stderr);
#if defined(_WIN32)
    // A failed join is unrecoverable. Bypass DLL detach rather than deadlocking
    // or allowing running code to outlive its module. Passing shutdowns join normally.
    TerminateProcess(GetCurrentProcess(), ERROR_INVALID_STATE);
#endif
    abort();
}

void CDAudio_Play( int track, int looping )
{
	cdaudio->Play( track, looping != 0 );
}

void CDAudio_Pause()
{
	cdaudio->Pause();
}

void CDAudio_Resume()
{
	cdaudio->Resume();
}

#ifdef _MSC_VER
#pragma warning( suppress: 26495 )
#endif
CCDAudio::CCDAudio()
{
	m_nMaxCDTrack = 0;
	m_uiDeviceID = 0;
	m_flPlayTime = 0.0f;
	m_dStartTime = 0.0;
	m_dPauseTime = 0.0;
	m_dFadeOutTime = 0.0;
	m_bIsCDValid = false;
	m_bIsPlaying = false;
	m_bIsPrimed = false;
	m_bIsInMiddleOfPriming = false;
	m_bWasPlaying = false;
	m_bInitialized = false;
	m_bEnabled = false;
	m_bIsLooping = false;
	m_flVolume = 1.0f;
	m_flMP3Volume = 1.0f;
	m_nPlayTrack = 0;
	m_szPendingPlayFilename[0] = '\0';
	m_bResumeOnSwitch = false;
	memset( m_rgRemapCD, 0, sizeof( m_rgRemapCD ) );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CCDAudio::~CCDAudio()
{
}

//-----------------------------------------------------------------------------
// Purpose: Reset clock
//-----------------------------------------------------------------------------
void CCDAudio::ResetCDTimes()
{
	m_flPlayTime = 0.0f;
	m_dStartTime = 0.0;
	m_dPauseTime = 0.0;
}

void CCDAudio::FreeAllocatedTracks()
{
	for ( int i = g_iMP3FirstMalloc; i < MAX_MP3_TRACKS; i++ )
	{
		if ( g_pszMP3trackFileMap[i] )
			Mem_Free( g_pszMP3trackFileMap[i] );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Close the cd drive door
//-----------------------------------------------------------------------------
void CCDAudio::_CloseDoor( int, int )
{
#if defined( _WIN32 )
	mciSendCommandA( m_uiDeviceID, MCI_SET, MCI_SET_DOOR_CLOSED, 0 );
#endif
	ResetCDTimes();
}

//-----------------------------------------------------------------------------
// Purpose: Eject the disk
//-----------------------------------------------------------------------------
void CCDAudio::_Eject( int, int )
{
#if defined( _WIN32 )
	_Stop( 0, 0 );
	mciSendCommandA( m_uiDeviceID, MCI_SET, MCI_SET_DOOR_OPEN, 0 );
#endif
	ResetCDTimes();
}

//-----------------------------------------------------------------------------
// Purpose: Retrieve validity and max track data
//-----------------------------------------------------------------------------
void CCDAudio::_GetAudioDiskInfo( int, int )
{
	m_bIsCDValid = false;
#if defined( _WIN32 )
	MCI_STATUS_PARMS status = {};
	status.dwItem = MCI_STATUS_READY;
	if ( mciSendCommandA( m_uiDeviceID, MCI_STATUS, MCI_STATUS_ITEM | MCI_WAIT, (DWORD_PTR)&status ) || !status.dwReturn )
		return;

	status.dwItem = MCI_STATUS_NUMBER_OF_TRACKS;
	if ( mciSendCommandA( m_uiDeviceID, MCI_STATUS, MCI_STATUS_ITEM | MCI_WAIT, (DWORD_PTR)&status ) || !status.dwReturn )
		return;

	m_bIsCDValid = true;
	m_nMaxCDTrack = (int)status.dwReturn;
#else
	m_nMaxCDTrack = 0;
#endif
}

void CCDAudio::_Init( int, int )
{
#if defined( _WIN32 )
	if ( COM_CheckParm( "-nocdaudio" ) )
		return;

	MCI_OPEN_PARMSA open = {};
	open.lpstrDeviceType = "cdaudio";
	if ( mciSendCommandA( 0, MCI_OPEN, MCI_OPEN_TYPE | MCI_OPEN_SHAREABLE, (DWORD_PTR)&open ) )
		return;

	m_uiDeviceID = open.wDeviceID;
	MCI_SET_PARMS set = {};
	set.dwTimeFormat = MCI_FORMAT_TMSF;
	if ( mciSendCommandA( m_uiDeviceID, MCI_SET, MCI_SET_TIME_FORMAT, (DWORD_PTR)&set ) )
	{
		mciSendCommandA( m_uiDeviceID, MCI_CLOSE, 0, 0 );
		return;
	}
#endif

	for ( int i = 0; i < CD_REMAP_TRACKS; i++ )
		m_rgRemapCD[i] = i;

	thread->AddThreadItem( &CCDAudio::_GetAudioDiskInfo, 0, 0 );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : track - 
//			looping - 
//-----------------------------------------------------------------------------
void CCDAudio::Play( int track, bool looping )
{
	m_dFadeOutTime = 0.0;
	thread->AddThreadItem( &CCDAudio::_Play, track, looping );
}

void CCDAudio::PlayFile(const char *filename, bool looping) {
    thread->AddFileThreadItem(&CCDAudio::_PlayFile, filename, looping);
}

void CCDAudio::_PlayFile(const char *filename, int looping) {
    if (!m_bEnabled) {
        return;
    }
    m_dFadeOutTime = 0.0;
    MP3_PlayTrack(filename, looping != 0);
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCDAudio::Pause()
{
	if ( m_bInitialized )
		thread->AddThreadItem( &CCDAudio::_Pause, 0, 0 );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCDAudio::Resume()
{
	if ( m_bInitialized )
		thread->AddThreadItem( &CCDAudio::_Resume, 0, 0 );
}

void CCDAudio::_Pause( int, int )
{
	if ( !m_bEnabled || !m_bIsPlaying )
		return;

	if ( m_MP3.inuse )
		MP3_SetPause( true );
#if defined( _WIN32 )
	else
	{
		MCI_GENERIC_PARMS pause = {};
		pause.dwCallback = (DWORD_PTR)*pmainwindow;
		mciSendCommandA( m_uiDeviceID, MCI_PAUSE, 0, (DWORD_PTR)&pause );
	}
#endif

	m_bWasPlaying = m_bIsPlaying;
	m_dPauseTime = realtime;
	m_bIsPlaying = false;
	m_szPendingPlayFilename[0] = '\0';
}

void CCDAudio::_Resume( int, int )
{
	if ( !m_bEnabled || !m_bWasPlaying )
		return;

	if ( m_MP3.inuse )
	{
		MP3_SetPause( false );
	}
	else
	{
#if defined( _WIN32 )
		MCI_PLAY_PARMS play = {};
		play.dwCallback = (DWORD_PTR)*pmainwindow;
		play.dwFrom = (BYTE)m_nPlayTrack;
		play.dwTo = (BYTE)( (BYTE)m_nPlayTrack + 1 );
		if ( mciSendCommandA( m_uiDeviceID, MCI_PLAY, MCI_NOTIFY | MCI_TO, (DWORD_PTR)&play ) )
		{
			ResetCDTimes();
			return;
		}
#else
		m_dStartTime += realtime - m_dPauseTime;
		m_dPauseTime = 0.0;
#endif
	}

	m_bIsPlaying = true;
#if defined( _WIN32 )
	if ( !m_MP3.inuse )
	{
		m_dStartTime += realtime - m_dPauseTime;
		m_dPauseTime = 0.0;
	}
#endif
}

//-----------------------------------------------------------------------------
// Purpose: Save state when going into launcher
//-----------------------------------------------------------------------------
void CCDAudio::_SwitchToLauncher( int, int )
{
	if ( !m_bEnabled || !m_bIsPlaying )
		return;

	m_bResumeOnSwitch = true;
	// Pause device
	_Pause( 0, 0 );
}

//-----------------------------------------------------------------------------
// Purpose: Restore cd playback if needed
//-----------------------------------------------------------------------------
void CCDAudio::_SwitchToEngine( int, int )
{
	if ( !m_bResumeOnSwitch )
		return;

	m_bResumeOnSwitch = false;
	_Resume( 0, 0 );
}

void CCDAudio::MP3_Suspend_Audio() {
    m_MP3.playing = m_bIsPlaying;
    if (musicPlayer.IsPlaying() && m_bIsPlaying) {
        m_MP3.suspended = true;
    }
    MP3_StopStream();
    MP3_ReleaseDriver();
}

void CCDAudio::_Stop( int, int )
{
	if ( !m_bEnabled || ( !m_bIsPlaying && !m_bWasPlaying ) )
		return;

	m_bIsPlaying = false;
	m_bWasPlaying = false;
	m_szPendingPlayFilename[0] = '\0';
	MP3_StopStream();

	m_MP3.inuse = false;
	m_MP3.volume = m_flMP3Volume;
	m_MP3.suspended = false;
	m_MP3.playing = false;
	m_MP3.trackname[0] = '\0';
	m_MP3.tracknum = 0;
	m_MP3.looping = false;
#if defined( _WIN32 )
	mciSendCommandA( m_uiDeviceID, MCI_STOP, 0, 0 );
#endif
	ResetCDTimes();
}

void CCDAudio::Shutdown()
{
	if ( m_MP3.inuse )
		MP3_SetPause( true );

	MP3_Shutdown();

	if ( m_bInitialized )
	{
#if defined( _WIN32 )
		_Stop( 0, 0 );
		mciSendCommandA( m_uiDeviceID, MCI_CLOSE, MCI_WAIT, 0 );
#endif
		ResetCDTimes();
	}
}

//-----------------------------------------------------------------------------
// Purpose: Shutown and restart system
//-----------------------------------------------------------------------------
void CCDAudio::_CDReset( int, int )
{
	Shutdown();
	usleep( 1000000 );
	Init();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCDAudio::Frame()
{
	if ( !m_bEnabled )
		return;

	if ( m_flVolume != bgmvolume.value )
	{
		if ( m_flVolume )
		{
			m_flVolume = 0.0f;
			Pause();
		}
		else
		{
			m_flVolume = 1.0f;
			Resume();
		}

		Cvar_DirectSet( &bgmvolume, va( "%f", m_flVolume ) );
	}

	if ( !m_dFadeOutTime && MP3Volume.value != m_flMP3Volume )
		m_flMP3Volume = MP3_SetVolume( MP3Volume.value );

	thread->AddThreadItem( &CCDAudio::_CDUpdate, 0, 0 );
}

void CCDAudio::_CDUpdate( int, int )
{
	if ( !m_bIsPlaying )
		return;

	if ( m_dFadeOutTime )
	{
		float volume;
		double curTime = Sys_FloatTime();
		if ( curTime < m_dFadeOutTime )
		{
			volume = (float)( ( m_dFadeOutTime - curTime ) / MP3FadeTime.value * MP3Volume.value );
		}
		else
		{
			_Stop( 0, 0 );
			volume = MP3Volume.value;
			m_dFadeOutTime = 0.0;
		}

		MP3_SetVolume( volume );

		if ( thread->AddThreadItem( &CCDAudio::_CDUpdate, 0, 0 ) )
		{
#if defined( _WIN32 )
			Sleep( 0 );
#else
			usleep( 100 );
#endif
		}
	}

	if ( m_MP3.inuse )
	{
		m_bIsPlaying = musicPlayer.IsPlaying();
		if ( !m_bIsPlaying )
			m_szPendingPlayFilename[0] = '\0';
	}

	if ( m_flPlayTime && m_dStartTime )
	{
		float elapsed = (float)( realtime - m_dStartTime );
		if ( elapsed >= m_flPlayTime + TRACK_EXTRA_TIME && m_bIsPlaying )
		{
			m_bIsPlaying = false;
			m_szPendingPlayFilename[0] = '\0';
			if ( m_bIsLooping )
				Play( m_nPlayTrack, true );
		}
	}
}

SDL_Window* GetMainWindow()
{
	return *pmainwindow;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : CCDAudio
//-----------------------------------------------------------------------------
CCDAudio* GetInteralCDAudio()
{
	return &g_CDAudio;
}

int MP3_GetTrack( const char* pszTrack )
{
	if ( !pszTrack || Q_strstr( pszTrack, ":" ) || Q_strstr( pszTrack, ".." ) )
		return 0;

	const char* format = "%s.mp3";
	if ( Q_strstr( pszTrack, ".mp3" ) )
		format = "%s";

#if defined( _WIN32 )
	char szTemp[MAX_PATH];
	_snprintf( szTemp, sizeof( szTemp ) - 1, format, pszTrack );
#else
	char szTemp[4096];
	snprintf( szTemp, sizeof( szTemp ) - 1, format, pszTrack );
#endif
	szTemp[sizeof( szTemp ) - 1] = '\0';
	COM_FixSlashes( szTemp );

	for ( int i = 0; i < MAX_MP3_TRACKS; i++ )
	{
		if ( !g_pszMP3trackFileMap[i] )
		{
			g_pszMP3trackFileMap[i] = Mem_Strdup( szTemp );
			g_iMP3NumTracks = i + 1;
			return i;
		}

		if ( !strcmp( g_pszMP3trackFileMap[i], szTemp ) )
			return i;
	}

	return 0;
}

void MP3_Command_f()
{
	if ( Cmd_Argc() <= 1 )
		return;

	const char* command = Cmd_Argv( 1 );
	const char* argument = Cmd_Argv( 2 );

	if ( Q_strcasecmp( command, "play" ) == 0 )
	{
		g_CDAudio.Stop();
		g_CDAudio.Play( MP3_GetTrack( argument ), false );
	}

	if ( Q_strcasecmp( command, "playfile" ) == 0 )
	{
		g_CDAudio.Stop();
		g_CDAudio.PlayFile( argument, false );
	}

	if ( Q_strcasecmp( command, "loop" ) == 0 )
	{
		g_CDAudio.Stop();
		g_CDAudio.Play( MP3_GetTrack( argument ), true );
	}

	if ( Q_strcasecmp( command, "loopfile" ) == 0 )
	{
		g_CDAudio.Stop();
		g_CDAudio.PlayFile( argument, true );
	}

	if ( Q_strcasecmp( command, "stop" ) == 0 )
		g_CDAudio.Stop();
}

void PrimeMusicStream( char* filename, int looping )
{
	g_CDAudio.PrimeTrack( filename, looping );
}

void CCDAudio::PrimeTrack(char *filename, int looping) {
    m_bIsInMiddleOfPriming = true;
    if (!thread->AddFileThreadItem(&CCDAudio::_PrimeFile, filename, looping)) {
        m_bIsInMiddleOfPriming = false;
        return;
    }
    constexpr auto primeTimeout = std::chrono::milliseconds(2000);
    const auto deadline = std::chrono::steady_clock::now() + primeTimeout;
    while (m_bIsInMiddleOfPriming && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::yield();
    }
}

void CCDAudio::_PrimeFile(const char *filename, int looping) {
    m_dFadeOutTime = 0.0;
    m_bIsPrimed = MP3_InitStream(filename, looping != 0);
    m_bIsInMiddleOfPriming = false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCDAudio::Reset()
{
	thread->AddThreadItem( &CCDAudio::_CDReset, 0, 0 );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCDAudio::Stop()
{
	thread->AddThreadItem( &CCDAudio::_Stop, 0, 0 );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCDAudio::Eject()
{
	thread->AddThreadItem( &CCDAudio::_Eject, 0, 0 );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCDAudio::CloseDoor()
{
	thread->AddThreadItem( &CCDAudio::_CloseDoor, 0, 0 );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCDAudio::GetAudioDiskInfo()
{
	thread->AddThreadItem( &CCDAudio::_GetAudioDiskInfo, 0, 0 );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCDAudio::SwitchToLauncher()
{
	thread->AddThreadItem( &CCDAudio::_SwitchToLauncher, 0, 0 );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCDAudio::SwitchToEngine()
{
	thread->AddThreadItem( &CCDAudio::_SwitchToEngine, 0, 0 );
}

void CCDAudio::FadeOut()
{
	if ( !m_bIsPlaying )
		return;

	m_dFadeOutTime = realtime + MP3FadeTime.value;
	thread->AddThreadItem( &CCDAudio::_CDUpdate, 0, 0 );
}

bool CCDAudio::MP3_PlayTrackFinalize(int trackNum, bool looping) {
    if (!MP3_StartStream()) {
        return false;
    }
    m_MP3.inuse = true;
    m_flPlayTime = 0.0f;
    m_MP3.playing = true;
    m_MP3.tracknum = trackNum;
    m_MP3.looping = looping;
    m_dStartTime = 0.0;
    m_dPauseTime = 0.0;
    m_bIsLooping = looping;
    m_nPlayTrack = trackNum;
    m_bIsPlaying = true;
    if (m_flMP3Volume == 0.0f) {
        _Pause(0, 0);
    }
    return true;
}

bool CCDAudio::MP3_Init() {
    return musicPlayer.Init();
}

int CCDAudio::Init()
{
	m_MP3.inuse = false;
	m_MP3.suspended = false;
	m_MP3.playing = false;
	m_MP3.trackname[0] = '\0';
	m_MP3.tracknum = 0;
	m_MP3.looping = false;
	m_MP3.volume = 100.0f;

	for ( int i = 0; i < MAX_MP3_TRACKS; i++ )
		g_pszMP3trackFileMap[i] = NULL;

	static const char* const tracks[NUM_BUILTIN_MP3_TRACKS] = { com_empty_string, com_empty_string,
		"media\\Half-Life01.mp3", "media\\Prospero01.mp3", "media\\Half-Life12.mp3", "media\\Half-Life07.mp3",
		"media\\Half-Life10.mp3", "media\\Suspense01.mp3", "media\\Suspense03.mp3", "media\\Half-Life09.mp3",
		"media\\Half-Life02.mp3", "media\\Half-Life13.mp3", "media\\Half-Life04.mp3", "media\\Half-Life15.mp3",
		"media\\Half-Life14.mp3", "media\\Half-Life16.mp3", "media\\Suspense02.mp3", "media\\Half-Life03.mp3",
		"media\\Half-Life08.mp3", "media\\Prospero02.mp3", "media\\Half-Life05.mp3", "media\\Prospero04.mp3",
		"media\\Half-Life11.mp3", "media\\Half-Life06.mp3", "media\\Prospero03.mp3", "media\\Half-Life17.mp3",
		"media\\Prospero05.mp3", "media\\Suspense05.mp3", "media\\Suspense07.mp3" };

	for ( int i = 0; i < NUM_BUILTIN_MP3_TRACKS; i++ )
		g_pszMP3trackFileMap[i] = (char*)tracks[i];

	g_iMP3FirstMalloc = NUM_BUILTIN_MP3_TRACKS;
	g_iMP3NumTracks = NUM_BUILTIN_MP3_TRACKS;
	ResetCDTimes();

	if ( MP3_Init() )
		MP3_SetVolume( MP3Volume.value );

	m_bInitialized = true;
	m_bEnabled = true;
	if ( COM_CheckParm( "-nocdaudio" ) || COM_CheckParm( "-nosound" ) )
		m_bEnabled = false;

	thread->AddThreadItem( &CCDAudio::_Init, 0, 0 );
	return 0;
}

void CCDAudio::MP3_Shutdown()
{
	MP3_StopStream();
	MP3_ReleaseDriver();
	FreeAllocatedTracks();
}

void CCDAudio::MP3_ReleaseDriver() {
    musicPlayer.Shutdown();
}

float CCDAudio::MP3_SetVolume( float newVolume )
{
	if ( newVolume < 0.0f )
		newVolume = 0.0f;
	else if ( newVolume > 1.0f )
		newVolume = 1.0f;

	if ( newVolume != m_MP3.volume )
	{
		m_MP3.volume = newVolume;
		SetStreamVolumePan( m_MP3.volume );
	}

	return m_MP3.volume;
}

bool CCDAudio::MP3_InitStream(int trackNum, bool looping) {
    if (trackNum <= 1 || trackNum >= g_iMP3NumTracks) {
        return false;
    }
    m_bIsPrimed = false;
    if (!MP3_Init()) {
        return false;
    }
    char fullPath[512]{};
    if (!FS_GetLocalPath(g_pszMP3trackFileMap[trackNum], fullPath, sizeof(fullPath))) {
        return false;
    }
    FS_GetLocalCopy(fullPath);
    _Stop(0, 0);
    m_dFadeOutTime = 0.0;
    if (!musicPlayer.Open(fullPath, looping)) {
        Con_Printf("Unable to open MP3 music: %s\n", g_pszMP3trackFileMap[trackNum]);
        return false;
    }
    SetStreamVolumePan(m_MP3.volume);
    Q_strncpy(m_MP3.trackname, g_pszMP3trackFileMap[trackNum], sizeof(m_MP3.trackname));
    return true;
}

void CCDAudio::CD_f()
{
	if ( Cmd_Argc() <= 1 )
		return;

	const char* command = Cmd_Argv( 1 );

	if ( Q_strcasecmp( command, "on" ) == 0 )
	{
		m_bEnabled = true;
		return;
	}

	if ( Q_strcasecmp( command, "off" ) == 0 )
	{
		if ( m_bIsPlaying )
			thread->AddThreadItem( &CCDAudio::_Stop, 0, 0 );
		m_bEnabled = false;
		return;
	}

	if ( Q_strcasecmp( command, "reset" ) == 0 )
	{
		m_bEnabled = true;
		if ( m_bIsPlaying )
			thread->AddThreadItem( &CCDAudio::_Stop, 0, 0 );
#if defined( _WIN32 )
		for ( int i = 0; i < CD_REMAP_TRACKS; i++ )
			m_rgRemapCD[i] = i;
		thread->AddThreadItem( &CCDAudio::_GetAudioDiskInfo, 0, 0 );
#else
		_Init( 0, 0 );
#endif
		return;
	}

	if ( Q_strcasecmp( command, "remap" ) == 0 )
	{
		int argc = Cmd_Argc();
		for ( int n = 1; n < argc - 1; n++ )
		{
#if defined( _WIN32 )
			m_rgRemapCD[n] = atoi( Cmd_Argv( n + 1 ) );
#else
            m_rgRemapCD[n] = CseLegacyAtoi(Cmd_Argv(n + 1));
#endif
		}
		return;
	}

	if ( Q_strcasecmp( command, "close" ) == 0 )
	{
		CloseDoor();
		return;
	}

	if ( Q_strcasecmp( command, "mp3info" ) == 0 )
	{
		Con_Printf( "Current MP3 Title: %s\n", m_MP3.trackname );
		Con_Printf( "Current MP3 Track: %i\n", m_MP3.tracknum );
		Con_Printf( "Current MP3 Volume: %f\n", m_MP3.volume );
		return;
	}

	if ( Cmd_Argc() > 2 && Q_strcasecmp( command, "mp3track" ) == 0 )
	{
#if defined( _WIN32 )
		int track = atoi( Cmd_Argv( 2 ) ) + 1;
#else
        int track = ParseNativeMp3Track(Cmd_Argv(2));
#endif
		if ( MP3_PlayTrack( track, false ) )
		{
			m_flPlayTime = 0.0f;
			m_dStartTime = 0.0;
			m_dPauseTime = 0.0;
			m_bIsLooping = false;
			m_nPlayTrack = track;
			m_bIsPlaying = true;
		}
		return;
	}

	if ( Q_strcasecmp( command, "play" ) == 0 )
	{
#if defined( _WIN32 )
		Play( atoi( Cmd_Argv( 2 ) ), false );
#else
        Play(CseLegacyAtoi(Cmd_Argv(2)), false);
#endif
		return;
	}

	if ( Q_strcasecmp( command, "playfile" ) == 0 )
	{
		PlayFile( Cmd_Argv( 2 ), false );
		return;
	}

	if ( Q_strcasecmp( command, "loop" ) == 0 )
	{
#if defined( _WIN32 )
		Play( atoi( Cmd_Argv( 2 ) ), true );
#else
        Play(CseLegacyAtoi(Cmd_Argv(2)), true);
#endif
		return;
	}

	if ( Q_strcasecmp( command, "loopfile" ) == 0 )
	{
		PlayFile( Cmd_Argv( 2 ), true );
		return;
	}

	if ( Q_strcasecmp( command, "stop" ) == 0 )
	{
		Stop();
		return;
	}

	if ( Q_strcasecmp( command, "fadeout" ) == 0 )
	{
		FadeOut();
		return;
	}

	if ( Q_strcasecmp( command, "pause" ) == 0 )
	{
		Pause();
		return;
	}

	if ( Q_strcasecmp( command, "resume" ) == 0 )
	{
		Resume();
		return;
	}

	if ( Q_strcasecmp( command, "eject" ) == 0 )
	{
		if ( m_bIsPlaying )
			thread->AddThreadItem( &CCDAudio::_Stop, 0, 0 );
		Eject();
		m_bIsCDValid = false;
		return;
	}

	if ( Q_strcasecmp( command, "info" ) == 0 )
	{
		Con_Printf( "%u tracks\n", g_iMP3NumTracks - 1 );
		if ( m_bIsPlaying )
			Con_Printf( "Currently %s track %u\n", m_bIsLooping ? "looping" : "playing", m_nPlayTrack );
		else if ( m_bWasPlaying )
			Con_Printf( "Paused %s track %u\n", m_bIsLooping ? "looping" : "playing", m_nPlayTrack );

		Con_Printf( "Volume is %f\n", m_flVolume );
	}
}

void CD_Command_f()
{
	if ( Cmd_Argc() > 1 )
		g_CDAudio.CD_f();
}

void CCDAudio::_PrimeTrack( int track, int looping )
{
	m_bIsInMiddleOfPriming = true;

	if ( track )
	{
		if ( track > 1 && track < g_iMP3NumTracks )
		{
			m_bIsPrimed = MP3_InitStream(track, looping != 0);
		}
	}
	else if ( m_szPendingPlayFilename[0] )
	{
		m_bIsPrimed = MP3_InitStream(m_szPendingPlayFilename, looping != 0);
	}

	m_bIsInMiddleOfPriming = false;
}

bool CCDAudio::MP3_PlayTrack( const char* filename, bool looping )
{
	if ( !m_bIsPrimed && !MP3_InitStream( filename, looping ) )
		return false;

	m_bIsPrimed = false;
	return MP3_PlayTrackFinalize(0, looping);
}

bool CCDAudio::MP3_PlayTrack( int trackNum, bool looping )
{
	if ( trackNum <= 1 || trackNum >= g_iMP3NumTracks )
		return false;

	if ( !m_bIsPrimed && !MP3_InitStream( trackNum, looping ) )
		return false;

	m_bIsPrimed = false;
	return MP3_PlayTrackFinalize(trackNum, looping);
}

//-----------------------------------------------------------------------------
// Purpose: Play cd
// Input  : track - 
//			looping - 
//-----------------------------------------------------------------------------
void CCDAudio::_Play( int track, int looping )
{
	if ( !m_bEnabled )
		return;

	if ( !track && m_szPendingPlayFilename[0] )
	{
		MP3_PlayTrack( m_szPendingPlayFilename, looping != 0 );
		m_szPendingPlayFilename[0] = '\0';
		return;
	}

	m_szPendingPlayFilename[0] = '\0';
#if defined( _WIN32 )
	_GetAudioDiskInfo( 0, 0 );
	if ( !m_bIsCDValid )
	{
		MP3_PlayTrack( track, looping != 0 );
		return;
	}

	if ( track > m_nMaxCDTrack )
		MP3_PlayTrack( track, looping != 0 );

	const int remappedTrack = m_rgRemapCD[track];
	if ( remappedTrack < 1 )
		return;

	MCI_STATUS_PARMS status = {};
	status.dwItem = MCI_CDA_STATUS_TYPE_TRACK;
	status.dwTrack = remappedTrack;
	MCIERROR error = mciSendCommandA( m_uiDeviceID, MCI_STATUS, MCI_STATUS_ITEM | MCI_TRACK | MCI_WAIT, (DWORD_PTR)&status );
	if ( error )
	{
		char errorText[256];
		mciGetErrorStringA( error, errorText, sizeof( errorText ) );
		return;
	}
	if ( status.dwReturn != MCI_CDA_TRACK_AUDIO )
		return;

	status.dwItem = MCI_STATUS_LENGTH;
	if ( mciSendCommandA( m_uiDeviceID, MCI_STATUS, MCI_STATUS_ITEM | MCI_TRACK | MCI_WAIT, (DWORD_PTR)&status ) )
		return;

	if ( m_bIsPlaying )
	{
		if ( m_nPlayTrack == remappedTrack )
			return;
		_Stop( 0, 0 );
	}

	MCI_PLAY_PARMS play = {};
	play.dwCallback = (DWORD_PTR)*pmainwindow;
	play.dwFrom = (BYTE)remappedTrack;
	play.dwTo = static_cast<DWORD>(remappedTrack) | (static_cast<DWORD>(status.dwReturn) << 8);
	if ( mciSendCommandA( m_uiDeviceID, MCI_PLAY, MCI_NOTIFY | MCI_FROM | MCI_TO, (DWORD_PTR)&play ) )
		return;

	ResetCDTimes();
	m_nPlayTrack = remappedTrack;
	m_bIsPlaying = true;
	m_dStartTime = realtime;
	m_bIsLooping = looping != 0;
	m_flPlayTime = (float)MCI_MSF_MINUTE( status.dwReturn ) * 60.0f + (float)MCI_MSF_SECOND( status.dwReturn );
	if ( m_flVolume == 0.0f )
		_Pause( 0, 0 );
#else
	m_bIsCDValid = false;
	m_nMaxCDTrack = 0;

	if ( track )
		MP3_PlayTrack( track, looping != 0 );
#endif
}

void MP3_Suspend_Audio()
{
	cdaudio->MP3_Suspend_Audio();
}

void MP3_Resume_Audio()
{
	cdaudio->MP3_Resume_Audio();
}

void CCDAudio::MP3_Resume_Audio()
{
#if defined( _WIN32 )
	MP3_Init();
	if ( m_MP3.playing && m_MP3.suspended )
	{
		if ( m_MP3.tracknum )
			MP3_PlayTrack( m_MP3.tracknum, m_MP3.looping );
		else
			MP3_PlayTrack( m_MP3.trackname, m_MP3.looping );
	}
#else
	if ( !m_MP3.playing || !m_MP3.suspended )
		return;

	if ( !MP3_Init() )
		return;

	if ( m_MP3.tracknum )
	{
		if ( m_MP3.tracknum > 1 && m_MP3.tracknum < g_iMP3NumTracks )
			m_MP3.suspended = !MP3_PlayTrack( m_MP3.tracknum, m_MP3.looping );
		return;
	}

	m_MP3.suspended = !MP3_PlayTrack( m_MP3.trackname, m_MP3.looping );
#endif
}

bool CCDAudio::MP3_InitStream( const char* filename, bool looping )
{
	if ( !filename || !filename[0] )
		return false;

	return MP3_InitStream( MP3_GetTrack( filename ), looping );
}

bool CCDAudio::MP3_StartStream() {
    return musicPlayer.Start();
}

void CCDAudio::MP3_StopStream() {
    musicPlayer.Stop();
    m_bIsPrimed = false;
}

void CCDAudio::MP3_Loop() {
    musicPlayer.SetLooping(true);
}

void CCDAudio::MP3_SetPause(bool onOff) {
    musicPlayer.Pause(onOff);
}
