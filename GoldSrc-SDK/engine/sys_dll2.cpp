#include "native_gameui_retirement.h"
#include <atomic>
#include <exception>
#if INTPTR_MAX > INT32_MAX
cse::NativeGameInitState cse::nativeGameInit;
#endif

#if defined(__APPLE__)
#include "native_mouse_window.h"
#include <pthread.h>
#endif

#if defined(__APPLE__) && !defined(_WIN32)
#include "../common/NativeEngineMemory.h"
#include "../public/native_engine_arguments.h"
#endif

#if defined(CSE_OWNED_STEAM_PROVIDER)
#include "../public/steam_service.h"
#endif

#if defined( SW_RENDERER )

#if defined(_WIN32)
#include <winsock2.h>
#include <Windows.h>
#else
#include <unistd.h>
#include <netdb.h>
#endif
#include "video_platform.h"

#include "interface.h"
#include "keys.h"
#include "engine_apiproxy.h"
#include "igameuifuncs.h"
#include "ivideomode.h"
#include "client.h"
#include "kbutton.h"
#include "buildnum.h"
#include "cmd.h"
#include "common.h"
#include "conprint.h"
#include "crc.h"
#include "cvar.h"
#include "filesystem.h"
#include "host.h"
#include "cd.h"
#include "iengine.h"
#include "igame.h"
#include "modinfo.h"
#include "registry.h"
#include "sys.h"
#include "traceinit.h"
#include "zone.h"

#include <SDL3/SDL.h>

#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <climits>
#include <cstdint>
#include <cstring>
#if defined(_WIN32)
#include <eh.h>
#endif
#include "surface_backend_config.h"
#if defined(__APPLE__)
#include "../public/native_dedicated_lifetime.h"
#include "../public/native_dedicated_abi.h"
#include "../public/native_filesystem_module.h"
#endif

#define MINIMUM_HEAP_SIZE	0xE00000
#define MAXIMUM_HEAP_SIZE	0x8000000

vgui2::KeyCode	GetVGUI2KeyCodeForBind( const char* binding );
void			SetLoadingProgressBarStatusText( const char* statusText );
void VID_ReleaseBuffers();

class CGameUIFuncs : public IGameUIFuncs
{
public:
	~CGameUIFuncs() override;
	bool IsKeyDown( const char* keyName, bool& isDown ) override;
	const char* Key_NameForKey( int keynum ) override;
	const char* Key_BindingForKey( int keynum ) override;
	vgui2::KeyCode GetVGUI2KeyCodeForBind( const char* binding ) override;
	void GetVideoModes( vmode_s** modes, int* count ) override;
	void GetCurrentVideoMode( int* width, int* height, int* bitsPerPixel ) override;
	void GetCurrentRenderer( char* name, int nameLength, int* windowed, int* hdModels, int* addons ) override;
	bool IsConnectedToVACSecureServer() override;
	int Key_KeyStringToKeyNum( const char* keyName ) override;
};

extern "C" void* F( void** engineInterface )
{
	CreateInterfaceFn factory = Sys_GetFactoryThis();
	void* instance = factory( VENGINE_LAUNCHER_API_VERSION, NULL );
	*engineInterface = instance;
	return instance;
}

void ClearIOStates()
{
	for ( int key = 0; key < KEY_BINDING_COUNT; ++key )
		Key_Event( key, 0 );
	Key_ClearStates();
	ClientDLL_ClearStates();
}

bool CGameUIFuncs::IsKeyDown( const char* keyName, bool& isDown )
{
	kbutton_t* key = ClientDLL_FindKey( keyName );
	if ( !key )
		return false;
	isDown = ( key->state & BUTTON_DOWN ) != 0;
	return true;
}

const char* CGameUIFuncs::Key_NameForKey( int keynum )
{
	return Key_KeynumToString( keynum );
}

const char* CGameUIFuncs::Key_BindingForKey( int keynum )
{
	return ::Key_BindingForKey( keynum );
}

vgui2::KeyCode CGameUIFuncs::GetVGUI2KeyCodeForBind( const char* binding )
{
	return ::GetVGUI2KeyCodeForBind( binding );
}

int CGameUIFuncs::Key_KeyStringToKeyNum( const char* keyName )
{
	return Key_StringToKeynum( (char*)keyName );
}

void CGameUIFuncs::GetVideoModes( vmode_s** modes, int* count )
{
	VideoMode_GetVideoModes( modes, count );
}

void CGameUIFuncs::GetCurrentVideoMode( int* width, int* height, int* bitsPerPixel )
{
	VideoMode_GetCurrentVideoMode( width, height, bitsPerPixel );
}

void CGameUIFuncs::GetCurrentRenderer( char* name, int nameLength, int* windowed, int* hdModels, int* addons )
{
	VideoMode_GetCurrentRenderer( name, nameLength, windowed, hdModels, addons );
}

bool CGameUIFuncs::IsConnectedToVACSecureServer()
{
	return ( cls.state == ca_connected || cls.state == ca_active ) && cls.isVAC2Secure != 0;
}

#if !defined(__APPLE__)
class IDedicatedExports : public IBaseInterface
{
public:
	virtual void Sys_Printf( char* text ) = 0;
};
#endif

class IEngineAPI : public IBaseInterface
{
public:
	virtual int Run( void* instance, char* basedir, char* cmdline, char* postRestartCmdLineArgs,
		CreateInterfaceFn launcherFactory, CreateInterfaceFn filesystemFactory ) = 0;
};

#include "../public/dedicated_api.h"

class CEngineAPI : public IEngineAPI
{
public:
	~CEngineAPI() override;
	int Run( void* instance, char* basedir, char* cmdline, char* postRestartCmdLineArgs,
		CreateInterfaceFn launcherFactory, CreateInterfaceFn filesystemFactory ) override;
};

#ifdef _MSC_VER
#pragma warning( push )
#pragma warning( disable: 26495 )
#endif
//-----------------------------------------------------------------------------
// Purpose: Expose engine interface to launcher
//-----------------------------------------------------------------------------
class CDedicatedServerAPI : public IDedicatedServerAPI
{
public:
	~CDedicatedServerAPI() override;
	bool Init(
		char* basedir, char* cmdline, CreateInterfaceFn launcherFactory, CreateInterfaceFn filesystemFactory ) override;
	int Shutdown() override;
	bool RunFrame() override;
	void AddConsoleText( char* text ) override;
	void UpdateStatus( float* fps, int* nActive, int* nMaxPlayers, char* pszMap ) override;

#if defined(__APPLE__)
    std::uint32_t Retire(std::uint32_t interfaceVersion);
#endif

private:
#if defined(__APPLE__)
    bool m_initializing = false;
    bool m_shuttingDown = false;
    bool m_shutdownCompleted = false;
    bool m_retired = false;
#endif
    char m_OrigCmd[1024]{};
#if defined(__APPLE__)
    bool m_registryInitialized = false;
    bool m_filesystemInitialized = false;
    bool m_gameInitialized = false;
    bool m_engineStarted = false;
    bool m_running = false;
#endif
};
#ifdef _MSC_VER
#pragma warning( pop )
#endif

#if !defined(CSE_OWNED_STEAM_PROVIDER)
extern "C" void SteamAPI_UseBreakpadCrashHandler( const char* version, const char* date, const char* time,
	int fullMemoryDumps, void* context, void* preMinidumpCallback );
extern "C" void SteamAPI_SetMiniDumpComment( const char* comment );
extern "C" void SteamAPI_WriteMiniDump( unsigned int exceptionCode, void* exceptionInfo, unsigned int build );
#endif

void	SeedRandomNumberGenerator();
void	SV_ResetModInfo();
void	Host_InitializeGameDLL();
void	Host_GetHostInfo( float* fps, int* nActive, int* unused, int* nMaxPlayers, char* pszMap );
void	Steam_InitClient();
void	Steam_ShutdownClient();
void	VideoMode_Create();
int		RandomLong( int low, int high );

extern qboolean		g_bIsDedicatedServer;
extern cvar_t		servercfgfile;
extern cvar_t		mapcyclefile;
extern char*		g_pPostRestartCmdLineArgs;
extern SDL_Window**	pmainwindow;
extern int			Win32AtLeastV4;

qboolean g_bIsWin95;
qboolean g_bIsWin98;
int gHasMMXTechnology;
void ( *VID_FlipScreen )();
int ( *D_SurfaceCacheForRes )( int width, int height );
void ( *Launcher_ConsolePrintf )( char* format, ... );
CseVideoDC maindc;
CseVideoContext baseRC;
CseGLModule g_hOpenGL;
char*		argv[MAX_NUM_ARGVS];
qboolean	g_bPrintingKeepAliveDots;
const char*	szReslistsExt = ".lst";
const char*	szReslistsBaseDir = "reslists";
const char*	szCommonPreloads = "multiplayer_preloads";

static IDedicatedExports* dedicated;
static double g_flLastSteamProgressUpdateTime;
#if defined(_WIN32)
static qboolean g_PerfCounterInitialized;
static CRITICAL_SECTION g_PerfCounterMutex;
#endif

CGameUIFuncs::~CGameUIFuncs()
{
}

CEngineAPI::~CEngineAPI()
{
}

CDedicatedServerAPI::~CDedicatedServerAPI()
{
}

void Sys_VID_FlipScreen()
{
	videomode->FlipScreen();
}

int Sys_GetSurfaceCacheSize(int width, int height) {
    constexpr int kMaximumSoftwareWidth = 5120;
    constexpr int kMaximumSoftwareHeight = 1600;
    constexpr std::int64_t kCacheBaseBytes = 2953728;
    constexpr std::int64_t kCacheBytesPerPixel = 6;
    constexpr std::int64_t kSmallBufferLimit = 64001;
    constexpr int kSmallSurfaceCacheBytes = 3145728;
    constexpr int kBytesPerKibibyte = 1024;
    if (width <= 0 || height <= 0 || width > kMaximumSoftwareWidth || height > kMaximumSoftwareHeight) {
        return 0;
    }
    const int parameter = COM_CheckParm("-surfcachesize");
    if (parameter) {
        if (parameter + 1 >= com_argc) {
            return 0;
        }
        char *end = nullptr;
        const long long kibibytes = std::strtoll(com_argv[parameter + 1], &end, 10);
        if (end == com_argv[parameter + 1] || *end != '\0' || kibibytes <= 0 ||
            kibibytes > INT_MAX / kBytesPerKibibyte) {
            return 0;
        }
        return static_cast<int>(kibibytes) * kBytesPerKibibyte;
    }
    const std::int64_t pixels = static_cast<std::int64_t>(height) * width;
    if (sizeof(short) * pixels < kSmallBufferLimit) {
        return kSmallSurfaceCacheBytes;
    }
    return static_cast<int>(kCacheBaseBytes + kCacheBytesPerPixel * pixels);
}

void Legacy_Sys_Printf( char* format, ... )
{
	char text[1024];
	va_list arguments;
	va_start( arguments, format );
	_vsnprintf( text, sizeof( text ), format, arguments );
	va_end( arguments );
	if ( dedicated )
		dedicated->Sys_Printf( text );
}

void Legacy_ErrorMessage( int, const char* )
{
}

void Legacy_MP3subsys_Suspend_Audio()
{
}

void Legacy_MP3subsys_Resume_Audio()
{
}

void Sys_SetupLegacyAPIs()
{
	VID_FlipScreen = Sys_VID_FlipScreen;
	D_SurfaceCacheForRes = Sys_GetSurfaceCacheSize;
	Launcher_ConsolePrintf = Legacy_Sys_Printf;
}

void Sys_InitLauncherInterface()
{
    gHasMMXTechnology = CsePackedSurfaceLightingAvailable();
#if defined(_WIN32)
	g_hOpenGL = GetModuleHandleA( "opengl32.dll" );
#endif
	VID_FlipScreen = Sys_VID_FlipScreen;
	D_SurfaceCacheForRes = Sys_GetSurfaceCacheSize;
	Launcher_ConsolePrintf = Legacy_Sys_Printf;
}

void Sys_ShutdownLauncherInterface()
{
}

void Sys_InitAuthentication()
{
	Sys_Printf( "STEAM Auth Server\r\n" );
}

void Sys_ShutdownAuthentication()
{
}

qboolean Sys_IsWin95()
{
	return g_bIsWin95;
}

qboolean Sys_IsWin98()
{
	return g_bIsWin98;
}

void Sys_CheckOSVersion()
{
#if defined(_WIN32)
	OSVERSIONINFOA version = {};
	version.dwOSVersionInfoSize = sizeof( version );
#pragma warning( push )
#pragma warning( disable : 4996 )
#pragma warning( suppress: 28159 )
	BOOL success = GetVersionExA( &version );
#pragma warning( pop )
	if ( !success )
		Sys_Error( "Couldn't get OS info" );
	Win32AtLeastV4 = version.dwMajorVersion >= 4;
	if ( version.dwPlatformId == VER_PLATFORM_WIN32_WINDOWS && version.dwMajorVersion == 4 )
	{
		if ( !version.dwMinorVersion )
			g_bIsWin95 = true;
		else if ( version.dwMinorVersion - 1 <= 88 )
			g_bIsWin98 = true;
	}
#else
    Win32AtLeastV4 = true;
#endif
}

void Sys_Shutdown()
{
	Sys_ShutdownFloatTime();
	Steam_ShutdownClient();
#if defined(_WIN32)
	if ( g_PerfCounterInitialized )
	{
		DeleteCriticalSection( &g_PerfCounterMutex );
		g_PerfCounterInitialized = false;
	}
#endif
}

void Sys_ShutdownArgv()
{
}

void Sys_InitArgv( char* commandLine )
{
#if defined(__APPLE__) && !defined(_WIN32)
    static_assert(MAX_NUM_ARGVS == cse::native_arguments::ArgumentCapacity,
        "Native encoded argument limit must match the real engine argv storage");
    for (bool& quoted : cse::native_arguments::QuotedArguments) {
        quoted = false;
    }
    argv[0] = const_cast<char*>(com_empty_string);
    int argc = 1;
    char* input = commandLine;
    char* output = commandLine;
    while (*input && argc < MAX_NUM_ARGVS) {
        while (*input == ' ') {
            ++input;
        }
        if (!*input) {
            break;
        }
        const bool quoted = *input == '"';
        cse::native_arguments::QuotedArguments[argc] = quoted;
        argv[argc++] = output;
        if (quoted) {
            ++input;
        }
        while (*input && (quoted ? *input != '"' : *input != ' ')) {
            *output++ = *input++;
        }
        if (quoted && *input == '"') {
            ++input;
        }
        // Advance past the separator before writing the terminator in place.
        if (*input == ' ') {
            ++input;
        }
        *output++ = '\0';
    }
#else
	argv[0] = (char*)com_empty_string;
	int argc = 1;
	host_parms.argc = 1;
	while ( *commandLine != '\0' && argc < MAX_NUM_ARGVS )
	{
		if ( (unsigned char)( *commandLine - 33 ) > 0x5D )
		{
			char character;
			do
			{
				character = *++commandLine;
			} while ( (unsigned char)( *commandLine - 33 ) > 0x5D && character != '\0' );
			if ( character == '\0' )
				break;
		}

		host_parms.argc = ++argc;
		argv[argc - 1] = commandLine;
		while ( (unsigned char)( *commandLine - 33 ) <= 0x5D )
			++commandLine;
		if ( *commandLine == '\0' )
			break;
		*commandLine++ = '\0';
	}

#endif
	host_parms.argv = argv;
	COM_InitArgv( argc, argv );
	host_parms.argc = com_argc;
	host_parms.argv = com_argv;
}

void Sys_InitMemory()
{
#if defined(__APPLE__) && !defined(_WIN32)
    namespace memory = cse_native_engine_memory;
    int bytes = host_parms.memsize;
    const int parameter = COM_CheckParm("-heapsize");
    if (parameter != 0 && parameter < com_argc - 1) {
        if (!memory::ParseHeapArgument(com_argv[parameter + 1], bytes)) {
            Sys_Error("Invalid -heapsize: expected nonnegative KiB within signed32 bytes\n");
            return;
        }
        host_parms.memsize = bytes;
    }
    memory::PhysicalMemory physical{memory::QueryStatus::InvalidPageCount, 0};
    if (bytes < memory::kMinimumHeapBytes) {
        physical = memory::QueryPhysicalMemory();
    }
    memory::HeapPlan plan{};
    if (memory::PlanHeap(bytes, physical, g_bIsDedicatedServer != 0, plan)
        != memory::PlanStatus::Ready) {
        Sys_Error("Available memory less than 15MB!!! %i\n", host_parms.memsize);
        return;
    }
    host_parms.memsize = plan.bytes;
    if (COM_CheckParm("-minmemory") != 0) {
        host_parms.memsize = memory::kMinimumHeapBytes;
    }
    host_parms.membase = memory::AllocateHeap(host_parms.memsize);
    if (host_parms.membase == nullptr) {
        constexpr float kBytesToMebibytes = 0.00000095367431640625f;
        const double mebibytes = static_cast<double>(static_cast<float>(host_parms.memsize) * kBytesToMebibytes);
        Sys_Error("Unable to allocate %.2f MB\n", mebibytes);
    }
#else
	int memsize = host_parms.memsize;
	int parameter = COM_CheckParm( "-heapsize" );
	if ( parameter && parameter < com_argc - 1 )
	{
		memsize = atoi( com_argv[parameter + 1] ) << 10;
		host_parms.memsize = memsize;
	}
	bool automaticSize = false;
	if ( memsize < MINIMUM_HEAP_SIZE )
	{
		MEMORYSTATUS memory;
		memory.dwLength = sizeof( memory );
#pragma warning( suppress: 28159 )
		GlobalMemoryStatus( &memory );
		automaticSize = true;
		if ( !memory.dwTotalPhys || memory.dwTotalPhys > 0x7FFFFFFF )
		{
			memsize = MAXIMUM_HEAP_SIZE;
		}
		else
		{
			if ( memory.dwTotalPhys < 0xF00000 )
				Sys_Error( "Available memory less than 15MB!!! %i\n", host_parms.memsize );
			memsize = static_cast<int>(memory.dwTotalPhys >> 1);
			if ( memsize < MINIMUM_HEAP_SIZE )
				memsize = MINIMUM_HEAP_SIZE;
		}
		host_parms.memsize = memsize;
	}
	if ( g_bIsDedicatedServer )
	{
		if ( automaticSize )
			memsize = 40 * 1024 * 1024;
		host_parms.memsize = memsize;
	}
	if ( memsize > MAXIMUM_HEAP_SIZE )
		host_parms.memsize = MAXIMUM_HEAP_SIZE;
	if ( COM_CheckParm( "-minmemory" ) )
		host_parms.memsize = MINIMUM_HEAP_SIZE;
	host_parms.membase = GlobalAlloc( GMEM_FIXED, host_parms.memsize );
	if ( !host_parms.membase )
		Sys_Error( "Unable to allocate %.2f MB\n", (double)( (float)host_parms.memsize * 0.00000095367431640625f ) );
#endif
}

void Sys_ShutdownMemory()
{
#if defined(__APPLE__) && !defined(_WIN32)
    if (host_parms.membase && cache_head.next) {
        // Cache users can live outside the hunk (notably mod_known). Retire
        // their data pointers while the linked cache headers still exist.
        Cache_Force_Flush();
    }
    VID_ReleaseBuffers();
    cse_native_engine_memory::ReleaseHeap(host_parms.membase, host_parms.memsize);
#else
	GlobalFree( host_parms.membase );
	host_parms.membase = NULL;
	host_parms.memsize = 0;
#endif
}

void Sys_ShowProgressTicks( char* )
{
	static bool recursionGuard;
	static int numTics;
	if ( recursionGuard )
		return;

	recursionGuard = true;
	if ( COM_CheckParm( "-steam" ) )
	{
		double now = Sys_FloatTime();
		if ( g_flLastSteamProgressUpdateTime + 2.0 <= now )
		{
			g_flLastSteamProgressUpdateTime = now;
			int tic = ++numTics;
			if ( g_bIsDedicatedServer )
			{
				if ( g_bMajorMapChange )
				{
					g_bPrintingKeepAliveDots = true;
					Sys_Printf( "." );
				}
			}
			else
			{
				char message[128];
				strcpy( message, "Updating game resources" );
				int dots = tic % 5;
				int count = 0;
				while ( dots >= count )
				{
					++count;
					strcat( message, "." );
				}
				SetLoadingProgressBarStatusText( message );
			}
		}
	}
	recursionGuard = false;
}

const char* GetCurrentSteamAppName()
{
	if ( Q_strcasecmp( com_gamedir, "cstrike" ) == 0 || Q_strcasecmp( com_gamedir, "cstrike_beta" ) == 0 )
		return "Counter-Strike";
	if ( Q_strcasecmp( com_gamedir, "valve" ) == 0 )
		return "Half-Life";
	if ( Q_strcasecmp( com_gamedir, "ricochet" ) == 0 )
		return "Ricochet";
	if ( Q_strcasecmp( com_gamedir, "dod" ) == 0 )
		return "Day of Defeat";
	if ( Q_strcasecmp( com_gamedir, "tfc" ) == 0 )
		return "Team Fortress Classic";
	if ( Q_strcasecmp( com_gamedir, "dmc" ) == 0 )
		return "Deathmatch Classic";
	if ( Q_strcasecmp( com_gamedir, "czero" ) == 0 )
		return "Condition Zero";
	return "Half-Life";
}

void SetRateRegistrySetting( const char* rateSetting )
{
	registry->WriteString( "rate", rateSetting );
}

const char* GetRateRegistrySetting( const char* defaultRate )
{
	return registry->ReadString( "rate", defaultRate );
}

void Sys_GetCDKey(char *key, int *length, int *dedicatedServer) {
    char hostname[MAX_PATH];
    char formatted[65];
    if (gethostname(hostname, MAX_PATH) != 0) {
        _snprintf(formatted, sizeof(formatted), "%u", static_cast<unsigned int>(RandomLong(0, 0x7FFF)));
    } else {
#if defined(__APPLE__)
        // This legacy LAN identifier is independent of Steam authentication.
        // Resolving a Mac's local hostname can block the game thread in mDNS.
        hostname[sizeof(hostname) - 1] = '\0';
        CRC32_t crc;
        CRC32_Init(&crc);
        CRC32_ProcessBuffer(&crc, hostname, static_cast<int>(std::strlen(hostname)));
        _snprintf(formatted, sizeof(formatted), "%u", CRC32_Final(crc));
#else
#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable : 4996)
#endif
        hostent *host = gethostbyname(hostname);
#if defined(_MSC_VER)
#pragma warning(pop)
#endif
        unsigned char *address = nullptr;
        if (host != nullptr && host->h_length == 4) {
            address = reinterpret_cast<unsigned char *>(*host->h_addr_list);
        }
        if (address == nullptr) {
            CRC32_t crc;
            CRC32_ProcessBuffer(&crc, hostname, static_cast<int>(std::strlen(hostname)));
            _snprintf(formatted, sizeof(formatted), "%u", crc);
        } else {
            _snprintf(formatted, sizeof(formatted), "%u.%u.%u.%u", address[0], address[1], address[2], address[3]);
        }
#endif
    }

    formatted[sizeof(formatted) - 1] = '\0';
    std::strcpy(key, formatted);
    if (length != nullptr) {
        *length = static_cast<int>(std::strlen(formatted));
    }
    if (dedicatedServer != nullptr) {
        *dedicatedServer = 0;
    }
}

int Sys_InitGame( char*, char*, void* windowAddress, int dedicatedServer )
{
	host_initialized = false;
	if ( dedicatedServer == 0 )
	{
		pmainwindow = (SDL_Window**)windowAddress;
		videomode->UpdateWindowPosition();
	}

	g_bIsDedicatedServer = dedicatedServer;
	if ( COM_CheckParm( "-dev" ) )
		developer.value = 1.0f;

	memset( &gmodinfo, 0, sizeof( gmodinfo ) );
	SV_ResetModInfo();
	TraceInit( "Sys_Init()", "Sys_Shutdown()", 0 );
	Sys_Init();
#if INTPTR_MAX > INT32_MAX
    if (!dedicatedServer) {
        cse::nativeGameInit.sysInitialized = true;
    }
#endif
	FS_LogLevelLoadStarted( "Launcher" );
	SeedRandomNumberGenerator();
	TraceInit( "Sys_InitMemory()", "Sys_ShutdownMemory()", 0 );
	Sys_InitMemory();
#if INTPTR_MAX > INT32_MAX
    if (!dedicatedServer) {
        cse::nativeGameInit.memoryAllocated = host_parms.membase != nullptr;
    }
#endif
	TraceInit( "Sys_InitLauncherInterface()", "Sys_ShutdownLauncherInterface()", 0 );
	Sys_InitLauncherInterface();
#if INTPTR_MAX > INT32_MAX
    if (!dedicatedServer) {
        cse::nativeGameInit.launcherInitialized = true;
    }
#endif

	TraceInit( "Host_Init( &host_parms )", "Host_Shutdown()", 0 );
#if INTPTR_MAX > INT32_MAX
    if (!dedicatedServer) {
        cse::nativeGameInit.hostAttempted = true;
    }
#endif
#if INTPTR_MAX > INT32_MAX
    const int hostResult = Host_Init(&host_parms);
    if (hostResult == 0 || !host_initialized) {
        return 0;
    }
#else
    Host_Init(&host_parms);
    if (!host_initialized) {
        return 0;
    }
#endif

	TraceInit( "Sys_InitAuthentication()", "Sys_ShutdownAuthentication()", 0 );
	Sys_InitAuthentication();
#if INTPTR_MAX > INT32_MAX
    if (!dedicatedServer) {
        cse::nativeGameInit.authenticationInitialized = true;
    }
#endif
	if ( g_bIsDedicatedServer )
	{
		Host_InitializeGameDLL();
		NET_Config( true );
	}
	else
	{
		ClientDLL_ActivateMouse();
#if INTPTR_MAX > INT32_MAX
    if (!dedicatedServer) {
        cse::nativeGameInit.clientMouseActivated = true;
    }
#endif
	}

	return 1;
}

void Sys_ShutdownGame()
{
#if INTPTR_MAX > INT32_MAX
    if (!g_bIsDedicatedServer) {
        cse::TryShutdownNativeGame();
        return;
    }
#endif
	if ( !g_bIsDedicatedServer )
		ClientDLL_DeactivateMouse();
	TraceShutdown( "Host_Shutdown()", 0 );
	Host_Shutdown();
	if ( g_bIsDedicatedServer )
		NET_Config( false );
	TraceShutdown( "Sys_ShutdownLauncherInterface()", 0 );
	TraceShutdown( "Sys_ShutdownAuthentication()", 0 );
	TraceShutdown( "Sys_ShutdownMemory()", 0 );
	Sys_ShutdownMemory();
	TraceShutdown( "Sys_Shutdown()", 0 );
	Sys_Shutdown();
}

int RunListenServer( void* instance, char* basedir, char* cmdline, char* postRestartCmdLineArgs,
	CreateInterfaceFn, CreateInterfaceFn filesystemFactory )
{
#if defined(__APPLE__)
    bool nativeReleaseVideo = false;
    bool nativeShutdownGame = false;
    bool nativeShutdownVideo = false;
#endif
#if INTPTR_MAX > INT32_MAX
    cse::nativeGameInit = {};
    cse::ResetNativeHostShutdown();
#endif
	static char OrigCmd[1024];
    g_bIsDedicatedServer = false;
	g_pPostRestartCmdLineArgs = postRestartCmdLineArgs;
	Q_strncpy_s( OrigCmd, cmdline, sizeof( OrigCmd ) );
	TraceInit( "Sys_InitArgv( OrigCmd )", "Sys_ShutdownArgv()", 0 );
	Sys_InitArgv( OrigCmd );
	eng->SetQuitting( 0 );
	registry->Init();
	Steam_InitClient();
#if INTPTR_MAX > INT32_MAX
    cse::nativeGameInit.steamClientInitialized = true;
#endif
	TraceInit( "FileSystem_Init(basedir, (void *)filesystemFactory)", "FileSystem_Shutdown()", 0 );
	int initialized = FileSystem_Init( basedir, filesystemFactory );
    if (!initialized) {
#if INTPTR_MAX > INT32_MAX
        // Filesystem setup can partially bind an interface/image before returning failure.
        FileSystem_Shutdown();
        Steam_ShutdownClient();
        cse::nativeGameInit.steamClientInitialized = false;
        cse::nativeGameInit.complete = true;
        registry->Shutdown();
#endif
        return initialized;
    }

	VideoMode_Create();
	int result = 2;
	registry->WriteInt( "CrashInitializingVideoMode", 1 );
	if ( videomode->Init( instance ) )
	{
#if defined(__APPLE__)
        nativeShutdownVideo = true;
#endif
		result = 0;
		registry->WriteInt( "CrashInitializingVideoMode", 0 );
		if ( game->Init( instance ) )
		{
#if defined(__APPLE__)
            nativeShutdownGame = true;
#endif
			result = 2;
			if ( eng->Load( false, basedir, cmdline ) )
			{
				videomode->PlayStartupSequence();
				for ( ;; )
				{
					game->SleepUntilInput( 0 );
					if (eng->GetQuitting()) {
#if defined(__APPLE__)
                        if (!cse::NativeMouseWindowState().Invalidate(nullptr)) {
                            // Retain this outer engine frame until native dispatch has unwound.
                            SDL_Delay(1);
                            continue;
                        }
#endif
                        break;
                    }
					eng->Frame();
				}
				result = eng->GetQuitting() != 1;
#if INTPTR_MAX <= INT32_MAX
                eng->Unload();
#endif
			}
#if INTPTR_MAX > INT32_MAX
            // Retain this run frame and AudioModuleShutdown until every inner owner has retired.
            for (;;) {
                bool retired = false;
                if (cse::EnterNativeGameUIRetirement()) {
                    retired = cse::TryUnloadNativeEngine();
                    cse::LeaveNativeGameUIRetirement();
                }
                if (retired) {
                    break;
                }
                constexpr unsigned int RetirementYieldMilliseconds = 1;
                SDL_Delay(RetirementYieldMilliseconds);
            }
#endif
#if defined(__APPLE__)
            nativeReleaseVideo = true;
#else
			videomode->ReleaseVideo();
            game->Shutdown();
#endif
		}
#if !defined(__APPLE__)
        videomode->Shutdown();
#endif
    }
#if !defined(_WIN32)
    else {
#if defined(__APPLE__)
        nativeReleaseVideo = true;
        nativeShutdownGame = true;
        nativeShutdownVideo = true;
#else
        videomode->ReleaseVideo();
        game->Shutdown();
        videomode->Shutdown();
#endif
    }
#endif

#if INTPTR_MAX > INT32_MAX
    for (;;) {
        bool retired = false;
        if (cse::EnterNativeGameUIRetirement()) {
            retired = cse::TryUnloadNativeEngine();
            cse::LeaveNativeGameUIRetirement();
        }
        if (retired) {
            break;
        }
        constexpr unsigned int RetirementYieldMilliseconds = 1;
        SDL_Delay(RetirementYieldMilliseconds);
    }
#endif
#if defined(__APPLE__)
    if (game->GetMainWindow()) {
        nativeShutdownGame = true;
        nativeShutdownVideo = true;
    }
    bool outerRetired = cse::BeginNativeListenShutdown(nativeReleaseVideo, nativeShutdownGame, nativeShutdownVideo);
    while (!outerRetired) {
        constexpr unsigned int RetirementYieldMilliseconds = 1;
        SDL_Delay(RetirementYieldMilliseconds);
        outerRetired = cse::FinishNativeListenShutdown();
    }
    return result;
#else
    TraceShutdown( "FileSystem_Shutdown()", 0 );
	FileSystem_Shutdown();
	registry->Shutdown();
	TraceShutdown( "Sys_ShutdownArgv()", 0 );
	return result;
#endif
}

#if defined(_WIN32)
static void ExceptionHandler( unsigned int code, EXCEPTION_POINTERS* exception )
{
	char message[256];
	_snprintf( message, sizeof( message ), "hl: %s\n", com_gamedir );
	SteamAPI_SetMiniDumpComment( message );
	SteamAPI_WriteMiniDump( code, exception, build_number() );
}

#endif

int CEngineAPI::Run( void* instance, char* basedir, char* cmdline, char* postRestartCmdLineArgs,
	CreateInterfaceFn launcherFactory, CreateInterfaceFn filesystemFactory )
{
#if INTPTR_MAX > INT32_MAX
    // Every ordinary return authorizes launcher image unload. Invalid ownership cannot return safely.
    static std::atomic_flag nativeRunActive = ATOMIC_FLAG_INIT;
    if (nativeRunActive.test_and_set(std::memory_order_acquire) || !cse::InitializeNativeGameUIHost()) {
        std::fputs("Fatal native engine Run ownership invariant; active dependencies cannot be unloaded\n", stderr);
        std::terminate();
    }
    struct NativeRunGuard {
        std::atomic_flag& active;
        ~NativeRunGuard() {
            active.clear(std::memory_order_release);
        }
    } nativeRunGuard{nativeRunActive};
#if defined(__APPLE__)
    while (!cse::FinishNativeListenShutdown()) {
        constexpr unsigned int RetirementYieldMilliseconds = 1;
        SDL_Delay(RetirementYieldMilliseconds);
    }
#endif
#endif
    cse::AudioModuleShutdown audioModuleShutdown;
	if ( !strstr( cmdline, "-nobreakpad" ) )
	{
		char* build = va( "%d", build_number() );
		SteamAPI_UseBreakpadCrashHandler( build, "Oct  7 2024", "19:00:27", 0, NULL, NULL );
	}
#if defined(_WIN32)
	_set_se_translator( ExceptionHandler );
#endif
	try
	{
		return RunListenServer( instance, basedir, cmdline, postRestartCmdLineArgs, launcherFactory, filesystemFactory );
	}
	catch ( ... )
	{
#if INTPTR_MAX > INT32_MAX
        std::fputs("Fatal native engine exception; unknown partial ownership cannot be safely retired\n", stderr);
        std::terminate();
#else
		return 0;
#endif
	}
}

#if defined(__APPLE__)
bool CDedicatedServerAPI::Init(char *basedir, char *cmdline, CreateInterfaceFn launcherFactory,
                               CreateInterfaceFn filesystemFactory) {
#if defined(__APPLE__)
    if (m_retired || m_initializing || m_shuttingDown) {
        return false;
    }
#endif
    static_assert(sizeof(m_OrigCmd) == cse::native_arguments::CommandCapacity);
    if (!launcherFactory || !basedir || !cse::native_arguments::Validate(cmdline)) {
        return false;
    }
    if (!cse::dedicated_abi::HostFactoryCompatible(launcherFactory)
        || !cse::filesystem_abi::CompatibleFactory(filesystemFactory)) {
        return false;
    }
    Shutdown();
    if (!m_shutdownCompleted) {
        return false;
    }
#if defined(__APPLE__)
    m_shutdownCompleted = false;
    m_initializing = true;
    struct InitializationScope {
        bool &active;
        ~InitializationScope() {
            active = false;
        }
    } initializationScope{m_initializing};
#endif
    dedicated = static_cast<IDedicatedExports *>(launcherFactory("VENGINE_DEDICATEDEXPORTS_API_VERSION001", nullptr));
    if (!dedicated || !cse::dedicated_abi::ViewCompatible<IDedicatedExports>(dedicated, launcherFactory, 1)) {
        dedicated = nullptr;
        return false;
    }
    std::strcpy(m_OrigCmd, cmdline);
    if (!std::strstr(cmdline, "-nobreakpad")) {
        char *build = va("%d", build_number());
        SteamAPI_UseBreakpadCrashHandler(build, "Oct  7 2024", "19:00:27", 0, nullptr, nullptr);
    }
    TraceInit("Sys_InitArgv( m_OrigCmd )", "Sys_ShutdownArgv()", 0);
    Sys_InitArgv(m_OrigCmd);
    eng->SetQuitting(0);
    registry->Init();
    m_registryInitialized = true;
    g_bIsDedicatedServer = true;
    TraceInit("FileSystem_Init(basedir, (void *)filesystemFactory)", "FileSystem_Shutdown()", 0);
    m_filesystemInitialized = true;
    if (!FileSystem_Init(basedir, filesystemFactory)) {
        Shutdown();
        return false;
    }
    if (!game->Init(nullptr)) {
        Shutdown();
        return false;
    }
    m_gameInitialized = true;
    m_engineStarted = true;
    if (!eng->Load(true, basedir, cmdline)) {
        Shutdown();
        return false;
    }
    m_running = true;
    char command[256];
    std::snprintf(command, sizeof(command), "exec %s\n", servercfgfile.string);
    Cbuf_InsertText(command);
    return true;
}

int CDedicatedServerAPI::Shutdown() {
    if (m_shuttingDown) {
        return giActive;
    }
    m_shuttingDown = true;
    m_shutdownCompleted = false;
    struct ShutdownScope {
        bool &active;
        ~ShutdownScope() {
            active = false;
        }
    } shutdownScope{m_shuttingDown};

    m_running = false;
    // Keep each owner live until its external cleanup returns. Nested API calls
    // cannot advance dependent teardown or publish completion during callbacks.
    if (m_engineStarted) {
        eng->Unload();
        m_engineStarted = false;
    }
    if (m_gameInitialized) {
        if (!game->Shutdown()) {
            return giActive;
        }
        m_gameInitialized = false;
    }
    if (m_filesystemInitialized) {
        TraceShutdown("FileSystem_Shutdown()", 0);
        FileSystem_Shutdown();
        m_filesystemInitialized = false;
    }
    if (m_registryInitialized) {
        registry->Shutdown();
        TraceShutdown("Sys_ShutdownArgv()", 0);
        Sys_ShutdownArgv();
        m_registryInitialized = false;
    }
    dedicated = nullptr;
    m_shutdownCompleted = true;
    return giActive;
}

#if defined(__APPLE__)
std::uint32_t CDedicatedServerAPI::Retire(std::uint32_t interfaceVersion) {
    if (interfaceVersion != CSE_NATIVE_DEDICATED_LIFETIME_VERSION) {
        return CSE_DEDICATED_RETIRE_UNSUPPORTED_VERSION;
    }
    if (m_retired) {
        return CSE_DEDICATED_RETIRE_ALREADY_RETIRED;
    }
    if (m_initializing || m_shuttingDown || m_registryInitialized || m_filesystemInitialized || m_gameInitialized ||
        m_engineStarted ||
        m_running || host_initialized || videomode) {
        return CSE_DEDICATED_RETIRE_SESSION_ACTIVE;
    }
    if (!m_shutdownCompleted) {
        return CSE_DEDICATED_RETIRE_SHUTDOWN_REQUIRED;
    }
    if (!CseSteamRetireServerTransport()) {
        return CSE_DEDICATED_RETIRE_TRANSPORT_FAILURE;
    }
    m_retired = true;
    return CSE_DEDICATED_RETIRE_READY;
}
#endif

bool CDedicatedServerAPI::RunFrame() {
    if (m_running && eng->GetQuitting() == 0) {
        eng->Frame();
        return true;
    }
    return false;
}

#else
bool CDedicatedServerAPI::Init(
	char* basedir, char* cmdline, CreateInterfaceFn launcherFactory, CreateInterfaceFn filesystemFactory )
{
	dedicated = (IDedicatedExports*)launcherFactory( "VENGINE_DEDICATEDEXPORTS_API_VERSION001", NULL );
	if ( !dedicated )
		return false;

	strcpy( m_OrigCmd, cmdline );
	if ( !strstr( cmdline, "-nobreakpad" ) )
	{
		char* build = va( "%d", build_number() );
		SteamAPI_UseBreakpadCrashHandler( build, "Oct  7 2024", "19:00:27", 0, NULL, NULL );
	}

	TraceInit( "Sys_InitArgv( m_OrigCmd )", "Sys_ShutdownArgv()", 0 );
	Sys_InitArgv( m_OrigCmd );
	eng->SetQuitting( 0 );
	registry->Init();
	g_bIsDedicatedServer = true;
	TraceInit( "FileSystem_Init(basedir, (void *)filesystemFactory)", "FileSystem_Shutdown()", 0 );
	if ( FileSystem_Init( basedir, filesystemFactory ) == 0 )
		return false;
	if ( !game->Init( NULL ) )
		return false;
	if ( !eng->Load( true, basedir, cmdline ) )
		return false;

	char command[256];
	_snprintf( command, sizeof( command ) - 1, "exec %s\n", servercfgfile.string );
	command[sizeof( command ) - 1] = '\0';
	Cbuf_InsertText( command );
	return true;
}

int CDedicatedServerAPI::Shutdown()
{
	// Unload GL, Sound, etc.
	eng->Unload();
	game->Shutdown();
	TraceShutdown( "FileSystem_Shutdown()", 0 );
	FileSystem_Shutdown();
	registry->Shutdown();
	TraceShutdown( "Sys_ShutdownArgv()", 0 );
	dedicated = NULL;
	return giActive;
// AR: does the dedicated server need registry access? Probably not, so leave this commented out for now...
//	registry->Shutdown();
}

bool CDedicatedServerAPI::RunFrame()
{
	if ( eng->GetQuitting() == 0 )
	{
		// Run engine frame
		eng->Frame();
		return true;
	}
	return false;
}

#endif

void CDedicatedServerAPI::AddConsoleText( char* text )
{
	Cbuf_AddText( text );
}

void CDedicatedServerAPI::UpdateStatus( float* fps, int* nActive, int* nMaxPlayers, char* pszMap )
{
	Host_GetHostInfo( fps, nActive, NULL, nMaxPlayers, pszMap );
}

int BuildMapCycleListHints( char** hints )
{
	char mod[MAX_PATH];
	char mapCyclePath[MAX_PATH];
	char line[MAX_PATH + 2];
	COM_FileBase( com_gamedir, mod );
	snprintf( mapCyclePath, sizeof( mapCyclePath ), "%s/%s", mod, mapcyclefile.string );
	FileHandle_t file = FS_Open( mapCyclePath, "rb" );
	if ( !file )
	{
		Con_Printf( "Unable to open %s\n", mapCyclePath );
		return 0;
	}

	snprintf( line, sizeof( line ), "%s\\%s\\%s%s\r\n", szReslistsBaseDir, GetCurrentSteamAppName(),
		szCommonPreloads, szReslistsExt );
	size_t lineLength = strlen( line );
	char* hintText = (char*)malloc( lineLength + 1 );
	*hints = hintText;
	if ( !hintText )
	{
		Con_Printf( "Unable to allocate memory for map cycle hints list" );
		FS_Close( file );
		return 0;
	}
	memcpy( hintText, line, lineLength + 1 );

	unsigned int fileSize = FS_Size( file );
	if ( fileSize > INT_MAX )
	{
		FS_Close( file );
		return 0;
	}
	if ( fileSize )
	{
		char* fileBytes = (char*)calloc( (size_t)fileSize + 1, 1 );
		if ( fileBytes )
		{
			if ( FS_Read( fileBytes, (int)fileSize, 1, file ) == 1 )
			{
				char* cursor = fileBytes;
				while ( true )
				{
					cursor = (char*)COM_Parse( cursor );
					if ( com_token[0] == '\0' )
						break;

					char mapName[MAX_PATH + 2];
					strncpy( mapName, com_token, sizeof( mapName ) - 1 );
					mapName[sizeof( mapName ) - 1] = '\0';
					if ( COM_TokenWaiting( cursor ) )
						cursor = (char*)COM_Parse( cursor );

					char mapLine[MAX_PATH + 2];
					snprintf( mapLine, sizeof( mapLine ), "%s\\%s\\%s%s\r\n", szReslistsBaseDir,
						GetCurrentSteamAppName(), mapName, szReslistsExt );
					size_t hintLength = strlen( *hints );
					size_t mapLineLength = strlen( mapLine );
					char* resized = (char*)realloc( *hints, hintLength + mapLineLength + 2 );
					if ( !resized )
					{
						Con_Printf( "Unable to reallocate memory for map cycle hints list" );
						free( fileBytes );
						FS_Close( file );
						return 0;
					}
					*hints = resized;
					strcat( resized, mapLine );
				}
			}
			free( fileBytes );
		}
	}

	FS_Close( file );
	snprintf( line, sizeof( line ), "%s\\%s\\mp_maps.txt\r\n", szReslistsBaseDir, GetCurrentSteamAppName() );
	size_t hintLength = strlen( *hints );
	char* resized = (char*)realloc( *hints, strlen( line ) + hintLength + 2 );
	if ( !resized )
		return 0;
	*hints = resized;
	strcat( resized, line );
	return 1;
}

EXPOSE_SINGLE_INTERFACE( CEngineAPI, IEngineAPI, VENGINE_LAUNCHER_API_VERSION );
#if defined(__APPLE__)
static CDedicatedServerAPI nativeDedicatedServer;
EXPOSE_SINGLE_INTERFACE_GLOBALVAR(CDedicatedServerAPI, IDedicatedServerAPI,
    "VENGINE_HLDS_API_VERSION002", nativeDedicatedServer);
extern "C" __attribute__((visibility("default"))) const cse::abi::Descriptor* CseGetDedicatedAbiV1() {
    static constexpr auto descriptor = cse::engine_bootstrap_abi::DescribeEngine<IEngineAPI,
        IDedicatedServerAPI, IDedicatedExports, CreateInterfaceFn>();
    return &descriptor;
}
extern "C" __attribute__((visibility("default"))) std::uint32_t CseNativeDedicatedRetireV1(
    std::uint32_t interfaceVersion) {
    return nativeDedicatedServer.Retire(interfaceVersion);
}
#else
EXPOSE_SINGLE_INTERFACE( CDedicatedServerAPI, IDedicatedServerAPI, "VENGINE_HLDS_API_VERSION002" );
#endif
EXPOSE_SINGLE_INTERFACE( CGameUIFuncs, IGameUIFuncs, "VENGINE_GAMEUIFUNCS_VERSION001" );

#else

#if defined(_WIN32)
#include <winsock2.h>
#include <Windows.h>
#endif
#include "video_platform.h"
#if !defined(_WIN32)
#include <netdb.h>
#endif

#include "interface.h"
#include "keys.h"
#include "engine_apiproxy.h"
#include "igameuifuncs.h"
#include "ivideomode.h"
#include "client.h"
#include "kbutton.h"
#include "buildnum.h"
#include "cmd.h"
#include "common.h"
#include "conprint.h"
#include "crc.h"
#include "cvar.h"
#include "filesystem.h"
#include "host.h"
#include "cd.h"
#include "iengine.h"
#include "igame.h"
#include "modinfo.h"
#include "registry.h"
#include "sys.h"
#include "traceinit.h"

#include <SDL3/SDL.h>

#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <climits>
#include <cstring>
#if defined(_WIN32)
#include <eh.h>
#endif
#include "surface_backend_config.h"
#include <clocale>
#include <cstdint>

#if !defined( _WIN32 )
#define _snprintf snprintf
#define _vsnprintf vsnprintf
#endif

vgui2::KeyCode GetVGUI2KeyCodeForBind( const char* binding );
void SetLoadingProgressBarStatusText( const char* statusText );

class CGameUIFuncs : public IGameUIFuncs
{
public:
	~CGameUIFuncs() override;
	bool IsKeyDown( const char* keyName, bool& isDown ) override;
	const char* Key_NameForKey( int keynum ) override;
	const char* Key_BindingForKey( int keynum ) override;
	vgui2::KeyCode GetVGUI2KeyCodeForBind( const char* binding ) override;
	void GetVideoModes( vmode_s** modes, int* count ) override;
	void GetCurrentVideoMode( int* width, int* height, int* bitsPerPixel ) override;
	void GetCurrentRenderer( char* name, int nameLength, int* windowed, int* hdModels, int* addons ) override;
	bool IsConnectedToVACSecureServer() override;
	int Key_KeyStringToKeyNum( const char* keyName ) override;
};

static_assert(sizeof(CGameUIFuncs) == sizeof(void*));

extern "C" void* F( void** engineInterface )
{
	CreateInterfaceFn factory = Sys_GetFactoryThis();
	void* instance = factory( VENGINE_LAUNCHER_API_VERSION, nullptr );
	*engineInterface = instance;
	return instance;
}

void ClearIOStates()
{
	for ( int key = 0; key < KEY_BINDING_COUNT; ++key )
		Key_Event( key, 0 );
	Key_ClearStates();
	ClientDLL_ClearStates();
}

bool CGameUIFuncs::IsKeyDown( const char* keyName, bool& isDown )
{
	kbutton_t* key = ClientDLL_FindKey( keyName );
	if ( !key )
		return false;
	isDown = ( key->state & BUTTON_DOWN ) != 0;
	return true;
}

const char* CGameUIFuncs::Key_NameForKey( int keynum )
{
	return Key_KeynumToString( keynum );
}

const char* CGameUIFuncs::Key_BindingForKey( int keynum )
{
	return ::Key_BindingForKey( keynum );
}

vgui2::KeyCode CGameUIFuncs::GetVGUI2KeyCodeForBind( const char* binding )
{
	return ::GetVGUI2KeyCodeForBind( binding );
}

int CGameUIFuncs::Key_KeyStringToKeyNum( const char* keyName )
{
	return Key_StringToKeynum( const_cast<char*>( keyName ) );
}

void CGameUIFuncs::GetVideoModes( vmode_s** modes, int* count )
{
	VideoMode_GetVideoModes( modes, count );
}

void CGameUIFuncs::GetCurrentVideoMode( int* width, int* height, int* bitsPerPixel )
{
	VideoMode_GetCurrentVideoMode( width, height, bitsPerPixel );
}

void CGameUIFuncs::GetCurrentRenderer( char* name, int nameLength, int* windowed, int* hdModels, int* addons )
{
	VideoMode_GetCurrentRenderer( name, nameLength, windowed, hdModels, addons );
}

bool CGameUIFuncs::IsConnectedToVACSecureServer()
{
	return ( cls.state == ca_connected || cls.state == ca_active ) && cls.isVAC2Secure != 0;
}

namespace
{
constexpr unsigned char kCommandTokenSpan = 0x5D;
constexpr int kArgumentLimit = 49;
#if defined(_WIN32)
constexpr int kFourteenMegabytes = 0xE00000;
constexpr int kFourteenMegabytesMinusOne = 0xDFFFFF;
constexpr int kOneHundredTwentyEightMegabytes = 0x8000000;
#endif
constexpr int kSurfaceCacheFloor = 3145728;
constexpr int kSurfaceCacheBias = 2953728;
constexpr int kSurfaceCachePixelLimit = 64001;
constexpr int kProgressMessageBytes = 128;
constexpr int kMapCyclePathBytes = 260;
constexpr int kMapHintLineBytes = 262;
constexpr int kCommandBytes = 256;
constexpr int kLocaleMessageBytes = 512;
constexpr int kCdKeyBytes = 65;
#if defined( _WIN32 )
constexpr int kHostNameBytes = MAX_PATH;
constexpr int kCdKeyRandomMaximum = 0x7FFF;
#else
constexpr int kHostNameBytes = 4096;
constexpr int kCdKeyRandomMaximum = 0x7FFFFFFF;
#endif
#if defined(_WIN32)
constexpr float kBytesToMegabytes = 0.00000095367431640625f;
#endif

bool IsCommandTokenCharacter( char character )
{
	unsigned char span = static_cast<unsigned char>( static_cast<unsigned char>( character ) - 33 );
	return span <= kCommandTokenSpan;
}
}

class IDedicatedExports : public IBaseInterface
{
public:
	virtual void Sys_Printf( char* text ) = 0;
};

class IEngineAPI : public IBaseInterface
{
public:
	virtual int Run( void* instance, char* basedir, char* cmdline, char* postRestartCmdLineArgs,
		CreateInterfaceFn launcherFactory, CreateInterfaceFn filesystemFactory ) = 0;
};

#include "../public/dedicated_api.h"

class CEngineAPI : public IEngineAPI
{
public:
	~CEngineAPI() override;
	int Run( void* instance, char* basedir, char* cmdline, char* postRestartCmdLineArgs,
		CreateInterfaceFn launcherFactory, CreateInterfaceFn filesystemFactory ) override;
};

#ifdef _MSC_VER
#pragma warning( push )
#pragma warning( disable: 26495 )
#endif
class CDedicatedServerAPI : public IDedicatedServerAPI
{
public:
	~CDedicatedServerAPI() override;
	bool Init(
		char* basedir, char* cmdline, CreateInterfaceFn launcherFactory, CreateInterfaceFn filesystemFactory ) override;
	int Shutdown() override;
	bool RunFrame() override;
	void AddConsoleText( char* text ) override;
	void UpdateStatus( float* fps, int* nActive, int* nMaxPlayers, char* pszMap ) override;

private:
	char m_OrigCmd[1024];
};
#ifdef _MSC_VER
#pragma warning( pop )
#endif

static_assert(sizeof(CEngineAPI) == sizeof(void*));
static_assert(sizeof(CDedicatedServerAPI) == sizeof(void*) + 1024);

#if !defined( _WIN32 )
extern "C" long sysconf( int name );
#endif

#if !defined(CSE_OWNED_STEAM_PROVIDER)
extern "C" void SteamAPI_UseBreakpadCrashHandler( const char* version, const char* date, const char* time,
	int fullMemoryDumps, void* context, void* preMinidumpCallback );
extern "C" void SteamAPI_SetMiniDumpComment( const char* comment );
extern "C" void SteamAPI_WriteMiniDump( unsigned int exceptionCode, void* exceptionInfo, unsigned int build );
#endif

void SeedRandomNumberGenerator();
void SV_ResetModInfo();
void Host_InitializeGameDLL();
void Host_GetHostInfo( float* fps, int* nActive, int* unused, int* nMaxPlayers, char* pszMap );
void Steam_InitClient();
void Steam_ShutdownClient();
void VideoMode_Create();
int GL_SetMode(
	CseGameWindowHandle mainwindow, CseVideoDC* pmaindc, CseVideoContext* pbaseRC, int fD3D, const char* pszDriver, const char* pszCmdLine );
void GL_Shutdown( CseGameWindowHandle hwnd, CseVideoDC hdc, CseVideoContext hglrc );
int RandomLong( int low, int high );

extern int Win32AtLeastV4;
extern qboolean g_bIsDedicatedServer;
extern cvar_t servercfgfile;
extern cvar_t mapcyclefile;
extern char* g_pPostRestartCmdLineArgs;
extern SDL_Window** pmainwindow;

qboolean g_bIsWin95;
qboolean g_bIsWin98;
int gHasMMXTechnology;
void ( *VID_FlipScreen )();
int ( *D_SurfaceCacheForRes )( int width, int height );
void ( *Launcher_ConsolePrintf )( char* format, ... );
CseGLModule g_hOpenGL;
CseVideoDC maindc;
CseVideoContext baseRC;
char* argv[MAX_NUM_ARGVS];
qboolean g_bPrintingKeepAliveDots;
const char* szReslistsExt = ".lst";
const char* szReslistsBaseDir = "reslists";
const char* szCommonPreloads = "multiplayer_preloads";

static IDedicatedExports* dedicated;
static double g_flLastSteamProgressUpdateTime;
static qboolean g_PerfCounterInitialized;
#if defined(_WIN32)
static CRITICAL_SECTION g_PerfCounterMutex;
#endif
extern int gD3DMode;

CGameUIFuncs::~CGameUIFuncs()
{
}

CEngineAPI::~CEngineAPI()
{
}

CDedicatedServerAPI::~CDedicatedServerAPI()
{
}

void Sys_VID_FlipScreen()
{
	if ( pmainwindow != nullptr )
		SDL_GL_SwapWindow( *pmainwindow );
}

int Sys_GetSurfaceCacheSize( int width, int height )
{
	int parameter = COM_CheckParm( "-surfcachesize" );
	if ( parameter != 0 )
		return std::atoi( com_argv[parameter + 1] ) << 10;

	int cached = kSurfaceCacheBias + 6 * height * width;
	if ( 2 * height * width < kSurfaceCachePixelLimit )
		return kSurfaceCacheFloor;
	return cached;
}

void Legacy_Sys_Printf( char* format, ... )
{
	char text[1024];
	va_list arguments;
	va_start( arguments, format );
	_vsnprintf( text, sizeof( text ), format, arguments );
	va_end( arguments );
	if ( dedicated != nullptr )
		dedicated->Sys_Printf( text );
}

void Legacy_ErrorMessage( int, const char* )
{
}

void Legacy_MP3subsys_Suspend_Audio()
{
}

void Legacy_MP3subsys_Resume_Audio()
{
}

void Sys_SetupLegacyAPIs()
{
	VID_FlipScreen = Sys_VID_FlipScreen;
	D_SurfaceCacheForRes = Sys_GetSurfaceCacheSize;
	Launcher_ConsolePrintf = Legacy_Sys_Printf;
}

void Sys_InitLauncherInterface()
{
    gHasMMXTechnology = CsePackedSurfaceLightingAvailable();
#if defined(_WIN32)
	g_hOpenGL = GetModuleHandleA( "opengl32.dll" );
#else
    g_hOpenGL = nullptr;
#endif
	VID_FlipScreen = Sys_VID_FlipScreen;
	D_SurfaceCacheForRes = Sys_GetSurfaceCacheSize;
	Launcher_ConsolePrintf = Legacy_Sys_Printf;
}

void Sys_ShutdownLauncherInterface()
{
}

void Sys_InitAuthentication()
{
	Sys_Printf( const_cast<char*>( "STEAM Auth Server\r\n" ) );
}

void Sys_ShutdownAuthentication()
{
}

qboolean Sys_IsWin95()
{
	return g_bIsWin95;
}

qboolean Sys_IsWin98()
{
	return g_bIsWin98;
}

void Sys_CheckOSVersion()
{
#if defined(_WIN32)
	OSVERSIONINFOA version = {};
	version.dwOSVersionInfoSize = sizeof( version );
#pragma warning( push )
#pragma warning( disable: 4996 )
#pragma warning( suppress: 28159 )
	BOOL success = GetVersionExA( &version );
#pragma warning( pop )
	if ( !success )
		Sys_Error( "Couldn't get OS info" );
	Win32AtLeastV4 = version.dwMajorVersion >= 4;
	if ( version.dwPlatformId == VER_PLATFORM_WIN32_WINDOWS && version.dwMajorVersion == 4 )
	{
		if ( version.dwMinorVersion == 0 )
			g_bIsWin95 = true;
		else if ( version.dwMinorVersion - 1 <= 88 )
			g_bIsWin98 = true;
	}
#else
    g_bIsWin95 = false;
    g_bIsWin98 = false;
#endif
}

void Sys_Shutdown()
{
	Sys_ShutdownFloatTime();
	Steam_ShutdownClient();
	if ( g_PerfCounterInitialized )
	{
#if defined(_WIN32)
		DeleteCriticalSection( &g_PerfCounterMutex );
#endif
		g_PerfCounterInitialized = false;
	}
	GL_Shutdown( reinterpret_cast<CseGameWindowHandle>( *pmainwindow ), maindc, baseRC );
}

void Sys_ShutdownArgv()
{
}

void Sys_InitArgv( char* commandLine )
{
	argv[0] = const_cast<char*>( com_empty_string );
	int argc = 1;
	host_parms.argc = 1;
	while ( *commandLine != '\0' && argc <= kArgumentLimit )
	{
		if ( !IsCommandTokenCharacter( *commandLine ) )
		{
			char character;
			do
			{
				character = *++commandLine;
			} while ( !IsCommandTokenCharacter( *commandLine ) && character != '\0' );
			if ( character == '\0' )
				break;
		}

		host_parms.argc = ++argc;
		argv[argc - 1] = commandLine;
		while ( IsCommandTokenCharacter( *commandLine ) )
			++commandLine;
		if ( *commandLine == '\0' )
			break;
		*commandLine++ = '\0';
	}

	host_parms.argv = argv;
	COM_InitArgv( argc, argv );
	host_parms.argc = com_argc;
	host_parms.argv = com_argv;
}

void Sys_InitMemory()
{
#if defined( _WIN32 )
	constexpr int dedicatedHeapSize = 40 * 1024 * 1024;
	int memsize = host_parms.memsize;
	int parameter = COM_CheckParm( "-heapsize" );
	if ( parameter != 0 && parameter < com_argc - 1 )
	{
		memsize = std::atoi( com_argv[parameter + 1] ) << 10;
		host_parms.memsize = memsize;
	}
	bool automaticSize = memsize < kFourteenMegabytes;
	if ( automaticSize )
	{
		MEMORYSTATUS memory = {};
		memory.dwLength = sizeof( memory );
#pragma warning( suppress: 28159 )
		GlobalMemoryStatus( &memory );
		if ( memory.dwTotalPhys == 0 || memory.dwTotalPhys > 0x7FFFFFFF )
			memsize = kOneHundredTwentyEightMegabytes;
		else
		{
			if ( memory.dwTotalPhys < 0xF00000 )
				Sys_Error( "Available memory less than 15MB!!! %i\n", host_parms.memsize );
			memsize = static_cast<int>( memory.dwTotalPhys >> 1 );
			if ( memsize < kFourteenMegabytes )
				memsize = kFourteenMegabytes;
		}
		host_parms.memsize = memsize;
	}
	if ( g_bIsDedicatedServer )
	{
		if ( automaticSize )
			memsize = dedicatedHeapSize;
		host_parms.memsize = memsize;
	}
	if ( memsize > kOneHundredTwentyEightMegabytes )
		host_parms.memsize = kOneHundredTwentyEightMegabytes;
	if ( COM_CheckParm( "-minmemory" ) != 0 )
		host_parms.memsize = kFourteenMegabytes;
	host_parms.membase = GlobalAlloc( GMEM_FIXED, host_parms.memsize );
	if ( host_parms.membase == nullptr )
	{
		double megabytes = static_cast<double>( static_cast<float>( host_parms.memsize ) * kBytesToMegabytes );
		Sys_Error( "Unable to allocate %.2f MB\n", megabytes );
	}
#elif defined(__APPLE__)
    namespace memory = cse_native_engine_memory;
    int bytes = host_parms.memsize;
    const int parameter = COM_CheckParm("-heapsize");
    if (parameter != 0 && parameter < com_argc - 1) {
        if (!memory::ParseHeapArgument(com_argv[parameter + 1], bytes)) {
            Sys_Error("Invalid -heapsize: expected nonnegative KiB within signed32 bytes\n");
            return;
        }
        host_parms.memsize = bytes;
    }
    memory::PhysicalMemory physical{memory::QueryStatus::InvalidPageCount, 0};
    if (bytes < memory::kMinimumHeapBytes) {
        physical = memory::QueryPhysicalMemory();
    }
    memory::HeapPlan plan{};
    if (memory::PlanHeap(bytes, physical, g_bIsDedicatedServer != 0, plan)
        != memory::PlanStatus::Ready) {
        Sys_Error("Available memory less than 15MB!!! %i\n", host_parms.memsize);
        return;
    }
    host_parms.memsize = plan.bytes;
    if (COM_CheckParm("-minmemory") != 0) {
        host_parms.memsize = memory::kMinimumHeapBytes;
    }
    host_parms.membase = memory::AllocateHeap(host_parms.memsize);
    if (host_parms.membase == nullptr) {
        constexpr float kBytesToMebibytes = 0.00000095367431640625f;
        const double mebibytes = static_cast<double>(static_cast<float>(host_parms.memsize) * kBytesToMebibytes);
        Sys_Error("Unable to allocate %.2f MB\n", mebibytes);
    }
#else
	int memsize;
	int parameter = COM_CheckParm( "-heapsize" );
	if ( parameter != 0 && parameter < com_argc - 1 )
	{
		memsize = std::strtol( com_argv[parameter + 1], nullptr, 10 ) << 10;
		host_parms.memsize = memsize;
	}
	else
	{
		memsize = host_parms.memsize;
	}

	bool capMemory = true;
	if ( memsize <= kFourteenMegabytesMinusOne )
	{
		constexpr int kScPhysPages = 85;
		constexpr int kScPageSize = 30;
		const std::int32_t pageCount = static_cast<std::int32_t>( sysconf( kScPhysPages ) );
		const std::int64_t pageSize = sysconf( kScPageSize );
		const std::int64_t totalBytes = pageCount * pageSize;
		std::uint64_t totalMinusOne = static_cast<std::uint64_t>( totalBytes - 1 );
		std::uint32_t high = static_cast<std::uint32_t>( totalMinusOne >> 32 );
		std::uint32_t low = static_cast<std::uint32_t>( totalMinusOne );
		if ( high > 0 || low > 0x7FFFFFFEu )
		{
			memsize = kOneHundredTwentyEightMegabytes;
			host_parms.memsize = memsize;
		}
		else if ( low <= 0xEFFFFEu )
		{
			Sys_Error( "Available memory less than 15MB!!! %i\n", host_parms.memsize );
		}
		else
		{
			std::uint64_t half = static_cast<std::uint64_t>( totalBytes ) >> 1;
			std::uint32_t halfLow = static_cast<std::uint32_t>( half );
			std::uint32_t halfHigh = static_cast<std::uint32_t>( half >> 32 );
			if ( halfHigh == 0 && halfLow <= static_cast<std::uint32_t>( kFourteenMegabytesMinusOne ) )
			{
				host_parms.memsize = kFourteenMegabytes;
				capMemory = false;
			}
			else
			{
				memsize = static_cast<int>( halfLow );
				host_parms.memsize = memsize;
			}
		}
	}

	if ( capMemory && memsize > kOneHundredTwentyEightMegabytes )
		host_parms.memsize = kOneHundredTwentyEightMegabytes;

	int bytes;
	if ( COM_CheckParm( "-minmemory" ) != 0 )
	{
		bytes = kFourteenMegabytes;
		host_parms.memsize = bytes;
	}
	else
	{
		bytes = host_parms.memsize;
	}

	host_parms.membase = std::malloc( static_cast<std::size_t>( bytes ) );
	if ( host_parms.membase == nullptr )
	{
		double megabytes = static_cast<double>( static_cast<long double>( bytes ) * kBytesToMegabytes );
		Sys_Error( "Unable to allocate %.2f MB\n", megabytes );
	}
#endif
}

void Sys_ShutdownMemory()
{
#if defined(__APPLE__) && !defined(_WIN32)
    cse_native_engine_memory::ReleaseHeap(host_parms.membase, host_parms.memsize);
#else
#if defined( _WIN32 )
	GlobalFree( host_parms.membase );
#else
	std::free( host_parms.membase );
#endif
	host_parms.membase = nullptr;
	host_parms.memsize = 0;
#endif
}

void Sys_ShowProgressTicks( [[maybe_unused]] char* specialProgressMsg )
{
	static bool recursionGuard;
	static int numTics;
	if ( recursionGuard )
		return;

	recursionGuard = true;
	if ( COM_CheckParm( "-steam" ) != 0 )
	{
		double now = Sys_FloatTime();
		if ( g_flLastSteamProgressUpdateTime + 2.0 <= now )
		{
			g_flLastSteamProgressUpdateTime = now;
			int tic = ++numTics;
			if ( g_bIsDedicatedServer )
			{
				if ( g_bMajorMapChange )
				{
					g_bPrintingKeepAliveDots = true;
					Sys_Printf( const_cast<char*>( "." ) );
				}
			}
			else
			{
				char message[kProgressMessageBytes];
				std::strcpy( message, "Updating game resources" );
				int dots = tic % 5;
				int count = 0;
				while ( dots >= count )
				{
					++count;
					std::strcat( message, "." );
				}
				SetLoadingProgressBarStatusText( message );
			}
		}
	}
	recursionGuard = false;
}

const char* GetCurrentSteamAppName()
{
	if ( Q_strcasecmp( com_gamedir, "cstrike" ) == 0 || Q_strcasecmp( com_gamedir, "cstrike_beta" ) == 0 )
		return "Counter-Strike";
	if ( Q_strcasecmp( com_gamedir, "valve" ) == 0 )
		return "Half-Life";
	if ( Q_strcasecmp( com_gamedir, "ricochet" ) == 0 )
		return "Ricochet";
	if ( Q_strcasecmp( com_gamedir, "dod" ) == 0 )
		return "Day of Defeat";
	if ( Q_strcasecmp( com_gamedir, "tfc" ) == 0 )
		return "Team Fortress Classic";
	if ( Q_strcasecmp( com_gamedir, "dmc" ) == 0 )
		return "Deathmatch Classic";
	if ( Q_strcasecmp( com_gamedir, "czero" ) == 0 )
		return "Condition Zero";
	return "Half-Life";
}

void SetRateRegistrySetting( const char* rateSetting )
{
#if defined( _WIN32 )
	Sys_SetRegKeyValue( "Software\\Valve\\Steam", "Rate", rateSetting );
#else
	registry->WriteString( "rate", rateSetting );
#endif
}

const char* GetRateRegistrySetting( const char* defaultRate )
{
#if defined( _WIN32 )
	static char rateSetting[32];
	Sys_GetRegKeyValue( "Software\\Valve\\Steam", "Rate", rateSetting, sizeof( rateSetting ), defaultRate );
	return rateSetting;
#else
	return registry->ReadString( "rate", defaultRate );
#endif
}

void Sys_GetCDKey(char *key, int *length, int *dedicatedServer) {
    char hostname[kHostNameBytes];
    char formatted[kCdKeyBytes];
    if (gethostname(hostname, kHostNameBytes) != 0) {
        _snprintf(formatted, sizeof(formatted), "%u", static_cast<unsigned int>(RandomLong(0, kCdKeyRandomMaximum)));
    } else {
#if defined(__APPLE__)
        // This legacy LAN identifier is independent of Steam authentication.
        // Resolving a Mac's local hostname can block the game thread in mDNS.
        hostname[sizeof(hostname) - 1] = '\0';
        CRC32_t crc;
        CRC32_Init(&crc);
        CRC32_ProcessBuffer(&crc, hostname, static_cast<int>(std::strlen(hostname)));
        _snprintf(formatted, sizeof(formatted), "%u", CRC32_Final(crc));
#else
#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable : 4996)
#endif
        hostent *host = gethostbyname(hostname);
#if defined(_MSC_VER)
#pragma warning(pop)
#endif
        unsigned char *address = nullptr;
        if (host != nullptr && host->h_length == 4) {
            address = reinterpret_cast<unsigned char *>(*host->h_addr_list);
        }
        if (address == nullptr) {
            CRC32_t crc;
            CRC32_ProcessBuffer(&crc, hostname, static_cast<int>(std::strlen(hostname)));
            _snprintf(formatted, sizeof(formatted), "%u", crc);
        } else {
            _snprintf(formatted, sizeof(formatted), "%u.%u.%u.%u", address[0], address[1], address[2], address[3]);
        }
#endif
    }

    formatted[kCdKeyBytes - 1] = '\0';
    std::strcpy(key, formatted);
    if (length != nullptr) {
        *length = static_cast<int>(std::strlen(formatted));
    }
    if (dedicatedServer != nullptr) {
        *dedicatedServer = 0;
    }
}

int Sys_InitGame( char* cmdline, [[maybe_unused]] char* basedir, void* windowAddress, int dedicatedServer )
{
	host_initialized = false;
	if ( dedicatedServer == 0 )
	{
		pmainwindow = static_cast<SDL_Window**>( windowAddress );
		videomode->UpdateWindowPosition();
	}

	g_bIsDedicatedServer = dedicatedServer;
	if ( COM_CheckParm( "-dev" ) != 0 )
		developer.value = 1.0f;

	std::memset( &gmodinfo, 0, sizeof( gmodinfo ) );
	SV_ResetModInfo();
	TraceInit( "Sys_Init()", "Sys_Shutdown()", 0 );
	Sys_Init();
#if INTPTR_MAX > INT32_MAX
    if (!dedicatedServer) {
        cse::nativeGameInit.sysInitialized = true;
    }
#endif
	FS_LogLevelLoadStarted( "Launcher" );
	SeedRandomNumberGenerator();
	TraceInit( "Sys_InitMemory()", "Sys_ShutdownMemory()", 0 );
	Sys_InitMemory();
#if INTPTR_MAX > INT32_MAX
    if (!dedicatedServer) {
        cse::nativeGameInit.memoryAllocated = host_parms.membase != nullptr;
    }
#endif
	TraceInit( "Sys_InitLauncherInterface()", "Sys_ShutdownLauncherInterface()", 0 );
	Sys_InitLauncherInterface();
#if INTPTR_MAX > INT32_MAX
    if (!dedicatedServer) {
        cse::nativeGameInit.launcherInitialized = true;
    }
#endif
#if INTPTR_MAX > INT32_MAX
    if (!dedicatedServer) {
        cse::nativeGameInit.modeAttempted = true;
    }
#endif
	if ( GL_SetMode( reinterpret_cast<CseGameWindowHandle>( *pmainwindow ), &maindc, &baseRC, gD3DMode, "opengl32.dll", cmdline ) == 0 )
		return 0;

	TraceInit( "Host_Init( &host_parms )", "Host_Shutdown()", 0 );
#if INTPTR_MAX > INT32_MAX
    if (!dedicatedServer) {
        cse::nativeGameInit.hostAttempted = true;
    }
#endif
#if INTPTR_MAX > INT32_MAX
    const int hostResult = Host_Init(&host_parms);
    if (hostResult == 0 || !host_initialized) {
        return 0;
    }
#else
    Host_Init(&host_parms);
    if (!host_initialized) {
        return 0;
    }
#endif

	TraceInit( "Sys_InitAuthentication()", "Sys_ShutdownAuthentication()", 0 );
	Sys_InitAuthentication();
#if INTPTR_MAX > INT32_MAX
    if (!dedicatedServer) {
        cse::nativeGameInit.authenticationInitialized = true;
    }
#endif
	if ( g_bIsDedicatedServer )
	{
		Host_InitializeGameDLL();
		NET_Config( true );
	}
	else
	{
		ClientDLL_ActivateMouse();
#if INTPTR_MAX > INT32_MAX
    if (!dedicatedServer) {
        cse::nativeGameInit.clientMouseActivated = true;
    }
#endif
	}

#if !defined( _WIN32 )
	const char* localeName = "en_US.UTF-8";
	char* currentLocale = std::setlocale( LC_ALL, nullptr );
	if ( currentLocale == nullptr )
		currentLocale = const_cast<char*>( "c" );
	if ( Q_strcasecmp( currentLocale, localeName ) != 0 )
	{
		char message[kLocaleMessageBytes];
		std::snprintf( message, sizeof( message ),
			"SetLocale('%s') failed. Using '%s'.\nYou may have limited glyph support.\nPlease install '%s' locale.",
			localeName, currentLocale, localeName );
		SDL_ShowSimpleMessageBox( 0, "Warning", message, *pmainwindow );
	}
#endif
	return 1;
}

void Sys_ShutdownGame()
{
#if INTPTR_MAX > INT32_MAX
    if (!g_bIsDedicatedServer) {
        cse::TryShutdownNativeGame();
        return;
    }
#endif
	if ( !g_bIsDedicatedServer )
		ClientDLL_DeactivateMouse();
	TraceShutdown( "Host_Shutdown()", 0 );
	Host_Shutdown();
	if ( g_bIsDedicatedServer )
		NET_Config( false );
	TraceShutdown( "Sys_ShutdownLauncherInterface()", 0 );
	TraceShutdown( "Sys_ShutdownAuthentication()", 0 );
	TraceShutdown( "Sys_ShutdownMemory()", 0 );
	Sys_ShutdownMemory();
	TraceShutdown( "Sys_Shutdown()", 0 );
	Sys_Shutdown();
}

int RunListenServer( void* instance, char* basedir, char* cmdline, char* postRestartCmdLineArgs,
	[[maybe_unused]] CreateInterfaceFn launcherFactory, CreateInterfaceFn filesystemFactory )
{
#if defined(__APPLE__)
    bool nativeReleaseVideo = false;
    bool nativeShutdownGame = false;
    bool nativeShutdownVideo = false;
#endif
#if INTPTR_MAX > INT32_MAX
    cse::nativeGameInit = {};
    cse::ResetNativeHostShutdown();
#endif
	static char OrigCmd[1024];
	g_pPostRestartCmdLineArgs = postRestartCmdLineArgs;
	Q_strncpy_s( OrigCmd, cmdline, static_cast<int>( sizeof( OrigCmd ) ) );
	TraceInit( "Sys_InitArgv( OrigCmd )", "Sys_ShutdownArgv()", 0 );
	Sys_InitArgv( OrigCmd );
	eng->SetQuitting( 0 );
	registry->Init();
	Steam_InitClient();
#if INTPTR_MAX > INT32_MAX
    cse::nativeGameInit.steamClientInitialized = true;
#endif
	TraceInit( "FileSystem_Init(basedir, (void *)filesystemFactory)", "FileSystem_Shutdown()", 0 );
	int initialized = FileSystem_Init( basedir, filesystemFactory );
    if (!initialized) {
#if INTPTR_MAX > INT32_MAX
        // Filesystem setup can partially bind an interface/image before returning failure.
        FileSystem_Shutdown();
        Steam_ShutdownClient();
        cse::nativeGameInit.steamClientInitialized = false;
        cse::nativeGameInit.complete = true;
        registry->Shutdown();
#endif
        return initialized;
    }

	VideoMode_Create();
	int result = 2;
	registry->WriteInt( "CrashInitializingVideoMode", 1 );
	if ( videomode->Init( instance ) )
	{
#if defined(__APPLE__)
        nativeShutdownVideo = true;
#endif
		result = 0;
		registry->WriteInt( "CrashInitializingVideoMode", 0 );
		if ( game->Init( instance ) )
		{
#if defined(__APPLE__)
            nativeShutdownGame = true;
#endif
			result = 2;
			// Start up the game engine
			if ( eng->Load( false, basedir, cmdline ) )
			{
				videomode->PlayStartupSequence();
				for ( ;; )
				{
					game->SleepUntilInput( 0 );
					if (eng->GetQuitting() != 0) {
#if defined(__APPLE__)
                        if (!cse::NativeMouseWindowState().Invalidate(nullptr)) {
                            // Retain this outer engine frame until native dispatch has unwound.
                            SDL_Delay(1);
                            continue;
                        }
#endif
                        break;
                    }
					eng->Frame();
				}
				result = eng->GetQuitting() != 1;
				// Unload GL, Sound, etc.
#if INTPTR_MAX <= INT32_MAX
                eng->Unload();
#endif
			}
#if INTPTR_MAX > INT32_MAX
            // Retain this run frame and AudioModuleShutdown until every inner owner has retired.
            for (;;) {
                bool retired = false;
                if (cse::EnterNativeGameUIRetirement()) {
                    retired = cse::TryUnloadNativeEngine();
                    cse::LeaveNativeGameUIRetirement();
                }
                if (retired) {
                    break;
                }
                constexpr unsigned int RetirementYieldMilliseconds = 1;
                SDL_Delay(RetirementYieldMilliseconds);
            }
#endif
			// Shut down memory, etc.
#if !defined(__APPLE__)
			game->Shutdown();
#endif
		}
#if !defined(__APPLE__)
		videomode->Shutdown();
#endif
	}

#if INTPTR_MAX > INT32_MAX
    for (;;) {
        bool retired = false;
        if (cse::EnterNativeGameUIRetirement()) {
            retired = cse::TryUnloadNativeEngine();
            cse::LeaveNativeGameUIRetirement();
        }
        if (retired) {
            break;
        }
        constexpr unsigned int RetirementYieldMilliseconds = 1;
        SDL_Delay(RetirementYieldMilliseconds);
    }
#endif
#if defined(__APPLE__)
    if (game->GetMainWindow()) {
        nativeShutdownGame = true;
        nativeShutdownVideo = true;
    }
    bool outerRetired = cse::BeginNativeListenShutdown(nativeReleaseVideo, nativeShutdownGame, nativeShutdownVideo);
    while (!outerRetired) {
        constexpr unsigned int RetirementYieldMilliseconds = 1;
        SDL_Delay(RetirementYieldMilliseconds);
        outerRetired = cse::FinishNativeListenShutdown();
    }
    return result;
#else
    TraceShutdown( "FileSystem_Shutdown()", 0 );
	FileSystem_Shutdown();
	registry->Shutdown();
	TraceShutdown( "Sys_ShutdownArgv()", 0 );
	return result;
#endif
}

#if defined(_WIN32)
static void ExceptionHandler( unsigned int code, EXCEPTION_POINTERS* exception )
{
	char message[256];
	_snprintf( message, sizeof( message ), "hl: %s\n", com_gamedir );
	SteamAPI_SetMiniDumpComment( message );
	SteamAPI_WriteMiniDump( code, exception, build_number() );
}

#endif

int CEngineAPI::Run( void* instance, char* basedir, char* cmdline, char* postRestartCmdLineArgs,
	CreateInterfaceFn launcherFactory, CreateInterfaceFn filesystemFactory )
{
#if INTPTR_MAX > INT32_MAX
    // Every ordinary return authorizes launcher image unload. Invalid ownership cannot return safely.
    static std::atomic_flag nativeRunActive = ATOMIC_FLAG_INIT;
    if (nativeRunActive.test_and_set(std::memory_order_acquire) || !cse::InitializeNativeGameUIHost()) {
        std::fputs("Fatal native engine Run ownership invariant; active dependencies cannot be unloaded\n", stderr);
        std::terminate();
    }
    struct NativeRunGuard {
        std::atomic_flag& active;
        ~NativeRunGuard() {
            active.clear(std::memory_order_release);
        }
    } nativeRunGuard{nativeRunActive};
#if defined(__APPLE__)
    while (!cse::FinishNativeListenShutdown()) {
        constexpr unsigned int RetirementYieldMilliseconds = 1;
        SDL_Delay(RetirementYieldMilliseconds);
    }
#endif
#endif
    cse::AudioModuleShutdown audioModuleShutdown;
	if ( std::strstr( cmdline, "-nobreakpad" ) == nullptr )
	{
		char* build = va( const_cast<char*>( "%d" ), build_number() );
		SteamAPI_UseBreakpadCrashHandler( build, "Oct  7 2024", "19:06:44", 0, nullptr, nullptr );
	}
#if defined(_WIN32)
	_set_se_translator( ExceptionHandler );
#endif
	try
	{
		return RunListenServer( instance, basedir, cmdline, postRestartCmdLineArgs, launcherFactory, filesystemFactory );
	}
	catch ( ... )
	{
#if INTPTR_MAX > INT32_MAX
        std::fputs("Fatal native engine exception; unknown partial ownership cannot be safely retired\n", stderr);
        std::terminate();
#else
		return 0;
#endif
	}
}

bool CDedicatedServerAPI::Init(
	char* basedir, char* cmdline, CreateInterfaceFn launcherFactory, CreateInterfaceFn filesystemFactory )
{
	dedicated =
		reinterpret_cast<IDedicatedExports*>( launcherFactory( "VENGINE_DEDICATEDEXPORTS_API_VERSION001", nullptr ) );
	if ( dedicated == nullptr )
		return false;

	std::strcpy( m_OrigCmd, cmdline );
	if ( std::strstr( cmdline, "-nobreakpad" ) == nullptr )
	{
		char* build = va( const_cast<char*>( "%d" ), build_number() );
		SteamAPI_UseBreakpadCrashHandler( build, "Oct  7 2024", "19:06:44", 0, nullptr, nullptr );
	}

	TraceInit( "Sys_InitArgv( m_OrigCmd )", "Sys_ShutdownArgv()", 0 );
	Sys_InitArgv( m_OrigCmd );
	eng->SetQuitting( 0 );
	registry->Init();
	g_bIsDedicatedServer = true;
	TraceInit( "FileSystem_Init(basedir, (void *)filesystemFactory)", "FileSystem_Shutdown()", 0 );
	if ( FileSystem_Init( basedir, filesystemFactory ) == 0 )
		return false;
	if ( !game->Init( nullptr ) )
		return false;
	if ( !eng->Load( true, basedir, cmdline ) )
		return false;

	char command[kCommandBytes];
	_snprintf( command, kCommandBytes - 1, "exec %s\n", servercfgfile.string );
	command[kCommandBytes - 1] = '\0';
	Cbuf_InsertText( command );
	return true;
}

int CDedicatedServerAPI::Shutdown()
{
	eng->Unload();
	game->Shutdown();
	TraceShutdown( "FileSystem_Shutdown()", 0 );
	FileSystem_Shutdown();
	registry->Shutdown();
	TraceShutdown( "Sys_ShutdownArgv()", 0 );
	dedicated = nullptr;
	return giActive;
}

bool CDedicatedServerAPI::RunFrame()
{
	if ( eng->GetQuitting() == 0 )
	{
		eng->Frame();
		return true;
	}
	return false;
}

void CDedicatedServerAPI::AddConsoleText( char* text )
{
	Cbuf_AddText( text );
}

void CDedicatedServerAPI::UpdateStatus( float* fps, int* nActive, int* nMaxPlayers, char* pszMap )
{
	Host_GetHostInfo( fps, nActive, nullptr, nMaxPlayers, pszMap );
}

int BuildMapCycleListHints( char** hints )
{
	char mod[kMapCyclePathBytes];
	char mapCyclePath[kMapCyclePathBytes];
	char line[kMapHintLineBytes];
	COM_FileBase( com_gamedir, mod );
	std::snprintf( mapCyclePath, sizeof( mapCyclePath ), "%s/%s", mod, mapcyclefile.string );
	FileHandle_t file = FS_Open( mapCyclePath, "rb" );
	if ( file == nullptr )
	{
		Con_Printf( "Unable to open %s\n", mapCyclePath );
		return 0;
	}

	std::snprintf( line, sizeof( line ), "%s\\%s\\%s%s\r\n", szReslistsBaseDir, GetCurrentSteamAppName(),
		szCommonPreloads, szReslistsExt );
	std::size_t lineLength = std::strlen( line );
	char* hintText = static_cast<char*>( std::malloc( lineLength + 1 ) );
	*hints = hintText;
	if ( hintText == nullptr )
	{
		Con_Printf( "Unable to allocate memory for map cycle hints list" );
		FS_Close( file );
		return 0;
	}
	std::memcpy( hintText, line, lineLength + 1 );

	unsigned int fileSize = FS_Size( file );
	if ( fileSize > INT_MAX )
	{
		FS_Close( file );
		return 0;
	}
	if ( fileSize != 0 )
	{
		char* fileBytes = static_cast<char*>( std::calloc( (std::size_t)fileSize + 1, 1 ) );
		if ( fileBytes != nullptr )
		{
			if ( FS_Read( fileBytes, static_cast<int>( fileSize ), 1, file ) == 1 )
			{
				char* cursor = fileBytes;
				while ( true )
				{
					cursor = const_cast<char*>( COM_Parse( cursor ) );
					if ( com_token[0] == '\0' )
						break;

					char mapName[kMapHintLineBytes];
					std::strncpy( mapName, com_token, 0x105 );
					mapName[kMapHintLineBytes - 1] = '\0';
					if ( COM_TokenWaiting( cursor ) != 0 )
						cursor = const_cast<char*>( COM_Parse( cursor ) );

					char mapLine[kMapHintLineBytes];
					std::snprintf( mapLine, sizeof( mapLine ), "%s\\%s\\%s%s\r\n", szReslistsBaseDir,
						GetCurrentSteamAppName(), mapName, szReslistsExt );
					std::size_t hintLength = std::strlen( *hints );
					std::size_t mapLineLength = std::strlen( mapLine );
					char* resized = static_cast<char*>( std::realloc( *hints, hintLength + mapLineLength + 2 ) );
					if ( resized == nullptr )
					{
						Con_Printf( "Unable to reallocate memory for map cycle hints list" );
						std::free( fileBytes );
						FS_Close( file );
						return 0;
					}
					*hints = resized;
					std::strcat( resized, mapLine );
				}
			}
			std::free( fileBytes );
		}
	}

	FS_Close( file );
	std::snprintf( line, sizeof( line ), "%s\\%s\\mp_maps.txt\r\n", szReslistsBaseDir, GetCurrentSteamAppName() );
	std::size_t hintLength = std::strlen( *hints );
	char* resized = static_cast<char*>( std::realloc( *hints, std::strlen( line ) + hintLength + 2 ) );
	if ( !resized )
		return 0;
	*hints = resized;
	std::strcat( resized, line );
	return 1;
}

EXPOSE_SINGLE_INTERFACE( CEngineAPI, IEngineAPI, VENGINE_LAUNCHER_API_VERSION );
EXPOSE_SINGLE_INTERFACE( CDedicatedServerAPI, IDedicatedServerAPI, "VENGINE_HLDS_API_VERSION002" );
EXPOSE_SINGLE_INTERFACE( CGameUIFuncs, IGameUIFuncs, "VENGINE_GAMEUIFUNCS_VERSION001" );

#endif

#if INTPTR_MAX > INT32_MAX
bool cse::TryShutdownNativeGame() {
    auto& state = nativeGameInit;
    if (state.shuttingDown || !NativeGameUIIsRetirementSafePoint()) {
        return false;
    }
    if (state.complete) {
        return true;
    }
    state.shuttingDown = true;
    struct Guard {
        NativeGameInitState& state;
        ~Guard() {
            state.shuttingDown = false;
        }
    } guard{state};
    if (state.clientMouseActivated) {
        ClientDLL_DeactivateMouse();
        state.clientMouseActivated = false;
    }
    if (state.hostAttempted) {
        if (!TryShutdownNativeHost()) {
            return false;
        }
        state.hostAttempted = false;
    }
    if (state.memoryAllocated) {
        Sys_ShutdownMemory();
        state.memoryAllocated = false;
    }
    if (state.sysInitialized) {
        Sys_Shutdown();
        state.sysInitialized = false;
        state.steamClientInitialized = false;
    } else if (state.steamClientInitialized) {
        Steam_ShutdownClient();
        state.steamClientInitialized = false;
    }
    state.complete = true;
    return true;
}
#endif

#if defined(__APPLE__)
namespace {
enum class NativeListenShutdownPhase {
    Idle,
    ReleaseVideo,
    Game,
    Video,
    FileSystem,
    Registry,
    Complete
};
NativeListenShutdownPhase nativeListenShutdownPhase = NativeListenShutdownPhase::Idle;
bool nativeListenReleaseVideo = false;
bool nativeListenShutdownGame = false;
bool nativeListenShutdownVideo = false;
bool nativeListenShutdownBusy = false;
} // namespace

bool cse::BeginNativeListenShutdown(bool releaseVideo, bool shutdownGame, bool shutdownVideo) {
    if (!pthread_main_np()) {
        return false;
    }
    if (nativeListenShutdownBusy || (nativeListenShutdownPhase != NativeListenShutdownPhase::Idle &&
        nativeListenShutdownPhase != NativeListenShutdownPhase::Complete)) {
        return false;
    }
    nativeListenReleaseVideo = releaseVideo;
    nativeListenShutdownGame = shutdownGame;
    nativeListenShutdownVideo = shutdownVideo;
    nativeListenShutdownPhase = NativeListenShutdownPhase::ReleaseVideo;
    return FinishNativeListenShutdown();
}

bool cse::FinishNativeListenShutdown() {
    if (!pthread_main_np()) {
        return false;
    }
    if (!EnterNativeGameUIRetirement()) {
        return false;
    }
    const bool engineRetired = TryShutdownNativeGame();
    LeaveNativeGameUIRetirement();
    if (!engineRetired) {
        return false;
    }
    if (nativeListenShutdownBusy) {
        return false;
    }
    nativeListenShutdownBusy = true;
    struct ShutdownScope {
        ~ShutdownScope() {
            nativeListenShutdownBusy = false;
        }
    } shutdownScope;
    for (;;) {
        switch (nativeListenShutdownPhase) {
        case NativeListenShutdownPhase::Idle:
        case NativeListenShutdownPhase::Complete:
            return true;
        case NativeListenShutdownPhase::ReleaseVideo:
            if (nativeListenReleaseVideo) {
                videomode->ReleaseVideo();
            }
            nativeListenShutdownPhase = NativeListenShutdownPhase::Game;
            break;
        case NativeListenShutdownPhase::Game:
            if (nativeListenShutdownGame && !game->Shutdown()) {
                return false;
            }
            nativeListenShutdownPhase = NativeListenShutdownPhase::Video;
            break;
        case NativeListenShutdownPhase::Video:
            if (nativeListenShutdownVideo) {
                videomode->Shutdown();
            }
            nativeListenShutdownPhase = NativeListenShutdownPhase::FileSystem;
            break;
        case NativeListenShutdownPhase::FileSystem:
            TraceShutdown("FileSystem_Shutdown()", 0);
            FileSystem_Shutdown();
            nativeListenShutdownPhase = NativeListenShutdownPhase::Registry;
            break;
        case NativeListenShutdownPhase::Registry:
            registry->Shutdown();
            TraceShutdown("Sys_ShutdownArgv()", 0);
            nativeListenShutdownPhase = NativeListenShutdownPhase::Complete;
            break;
        }
    }
}
#endif
