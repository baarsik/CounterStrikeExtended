#include "System.h"
#include "public/native_filesystem_module.h"

#include "HLTV/common/common.h"
#include "HLTV/common/random.h"
#include "GameUI/common/IVGuiModule.h"
#include "vgui/IScheme.h"
#include "vgui2/controls/Panel.h"
#include "vgui2/controls/controls.h"
#include "engine/controls.h"
#include "vgui/IVGui.h"
#include "vgui/ISurface.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <cstring>
#if !defined(_WIN32)
#include <sys/resource.h>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <exception>
#endif

static System g_System;

#ifdef _MSC_VER
#pragma warning( suppress : 26495 )
#endif
System::System()
{
	m_State = MODULE_UNDEFINED;
#if CSE_NATIVE_SYSTEM_MODULE
    m_LastTime = 0;
    m_FileSystemLibrary = nullptr;
    m_FileSystem = nullptr;
    m_VGuiLibrary = nullptr;
    std::memset(m_BaseDir, 0, sizeof(m_BaseDir));
    std::memset(m_StatusLine, 0, sizeof(m_StatusLine));
    m_LogFile = nullptr;
    std::memset(m_LogFileName, 0, sizeof(m_LogFileName));
    m_MinFrameTime = 0;
    m_RedirectBuffer = nullptr;
    m_RedirectSize = 0;
    m_Developer = false;
    m_NextSerial = 0;
    m_Tick = 0;
    m_SleepTime = 0;
    std::memset(m_Factories, 0, sizeof(m_Factories));
    m_VGUI = false;
    m_MainPanel = nullptr;
#endif
#if defined(_WIN32)
	InitializeCriticalSection( &m_ClockLock );
#endif
}

System::~System()
{
#if defined(_WIN32)
	DeleteCriticalSection( &m_ClockLock );
#endif
}

bool System::Init( IBaseSystem* system, int serial, char* name )
{
#if CSE_NATIVE_SYSTEM_MODULE
    // A stopped run may still own providers until callback frames have unwound.
    if (!m_NativeResourcesRetired || m_NativeShutdownActive) {
        return false;
    }
    m_NativeResourcesRetired = false;
#endif
	if ( !BaseSystemModule::Init( system, serial, name ) )
		return false;

	m_Serial = serial;
	m_NextSerial = serial + 1;
#if defined(_WIN32)
	_snprintf( m_Name, sizeof( m_Name ), name ? name : "system" );
#else
    const char* moduleName = name != nullptr ? name : "system";
    const std::size_t moduleNameLength = strnlen(moduleName, sizeof(m_Name));
    if (moduleNameLength >= sizeof(m_Name)) {
        std::fprintf(stderr, "HLTV: system name exceeds storage capacity\n");
        return false;
    }
    std::memcpy(m_Name, moduleName, moduleNameLength + 1);
#endif
#if defined(_WIN32)
	strncpy( m_BaseDir, COM_GetBaseDir(), sizeof( m_BaseDir ) );
#else
    const char* baseDirectory = COM_GetBaseDir();
    if (!*baseDirectory) {
        std::fprintf(stderr, "HLTV: cannot determine executable directory: %s\n", std::strerror(errno));
        return false;
    }
    std::memcpy(m_BaseDir, baseDirectory, std::strlen(baseDirectory) + 1);
#endif
	m_Developer = CheckParam( "-dev" ) != NULL;

	if ( !m_Console.Init( this ) )
	{
#if defined(_WIN32)
		char* message;
		FormatMessageA( FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
				NULL, GetLastError(), MAKELANGID( LANG_NEUTRAL, SUBLANG_DEFAULT ), (char*)&message, 0, NULL );
		Errorf( "%s", message );
		LocalFree( message );
#else
        char format[] = "Couldn't initialize console: %s\n";
        Errorf(format, std::strerror(errno));
#endif
		return false;
	}

#if defined(_WIN32)
	LARGE_INTEGER frequency;
	if ( !QueryPerformanceFrequency( &frequency ) )
	{
		Errorf( "Couldn't get system timer.\n" );
		return false;
	}

	m_ClockShift = 0;
	while ( frequency.HighPart || (double)frequency.LowPart > 2000000.0 )
	{
		m_ClockShift++;
		frequency.QuadPart = (unsigned __int64)frequency.QuadPart >> 1;
	}
	m_ClockFrequency = 1.0 / (double)frequency.LowPart;
#else
    if (!m_Clock.Reset()) {
        char format[] = "Couldn't initialize monotonic timer: %s\n";
        Errorf(format, std::strerror(errno));
        return false;
    }
#endif
	m_SystemTime = 0;
	m_LastTime = 0;

#if defined(_WIN32)
	m_FileSystemLibrary = Sys_LoadModule( "filesystem_stdio.dll" );
#else
    m_FileSystemLibrary = hltv_posix::LoadRuntimeModule("filesystem_stdio.dll", m_BaseDir, Sys_LoadModule);
#endif
	if ( !m_FileSystemLibrary )
	{
		Errorf( "Couldn't load %s.\n", "filesystem_stdio.dll" );
		return false;
	}

    CreateInterfaceFn factory = cse::filesystem_abi::CheckedFactory(m_FileSystemLibrary);
    if (!factory) {
        Errorf("Incompatible owned filesystem module.\n");
        Sys_UnloadModule(m_FileSystemLibrary);
        m_FileSystemLibrary = nullptr;
        return false;
    }
	m_FileSystem = (IFileSystem*)factory( "VFileSystem009", NULL );
	if ( !m_FileSystem )
	{
		Errorf( "Couldn't get IFileSystem from %s.\n", "filesystem_stdio.dll" );
		return false;
	}

	m_FileSystem->Mount();
	m_FileSystem->AddSearchPath( m_BaseDir, "ROOT" );
	Printf( "FileSystem initialized" );
	DPrintf( " (%s, %s)", "filesystem_stdio.dll", m_BaseDir );
	Printf( ".\n" );

	m_Commands.Init();
	m_Modules.Init();
#if CSE_NATIVE_SYSTEM_MODULE
    m_NativeModules.Restart();
#endif
	m_Libraries.Init();
	m_LogFile = NULL;
	memset( m_LogFileName, 0, sizeof( m_LogFileName ) );
	m_RedirectBuffer = NULL;
	m_RedirectSize = 0;
	m_Tick = 0;
	m_SleepTime = 1;
	m_VGUI = false;
	m_VGuiLibrary = NULL;
	memset( m_Factories, 0, sizeof( m_Factories ) );
	SeedRandomNumberGenerator();

	if ( CheckParam( "-highpriority" ) )
	{
		m_SleepTime = 0;
#if defined(_WIN32)
		SetPriorityClass( GetCurrentProcess(), HIGH_PRIORITY_CLASS );
#else
        constexpr int HighPriorityNiceness = -10;
        if (setpriority(PRIO_PROCESS, 0, HighPriorityNiceness) != 0) {
            char format[] = "WARNING! Could not set high process priority: %s\n";
            DPrintf(format, std::strerror(errno));
        }
#endif
	}

	if ( CheckParam( "-maxfps" ) )
		SetFPS( (float)atof( CheckParam( "-maxfps" ) ) );
	else
		SetFPS( 100 );

	RegisterCommand( "cmdlist", this, CMD_LIST );
	RegisterCommand( "help", this, CMD_LIST );
	RegisterCommand( "logfile", this, CMD_LOGFILE );
	RegisterCommand( "modules", this, CMD_MODULES );
	RegisterCommand( "exec", this, CMD_EXEC );
	RegisterCommand( "developer", this, CMD_DEVELOPER );
	RegisterCommand( "showcon", this, CMD_SHOWCON );
	RegisterCommand( "quit", this, CMD_QUIT );
	RegisterCommand( "exit", this, CMD_QUIT );
	RegisterCommand( "echo", this, CMD_ECHO );
	RegisterCommand( "loadmodule", this, CMD_LOADMODULE );
	RegisterCommand( "unloadmodule", this, CMD_UNLOADMODULE );

	SetTitle( "Console" );
#if CSE_NATIVE_SYSTEM_MODULE
    if (!GetModule("proxy001", "proxy", nullptr) || m_State == MODULE_DISCONNECTED) {
        Errorf("Couldn't initialize required Proxy module.\n");
        return false;
    }
#else
	GetModule( "proxy001", "proxy", NULL );
#endif
	SetTitle( "HLTV" );
	m_State = MODULE_RUNNING;
	DPrintf( "System initialized.\n" );

	int token = 0;
	char* argument = m_CommandLine.GetToken( token );
	while ( argument )
	{
		token++;
		if ( *argument == '+' )
		{
			char command[256];
			memset( command, 0, sizeof( command ) );
			strncpy( command, argument + 1, sizeof( command ) - 1 );
			command[sizeof( command ) - 1] = 0;
			argument = m_CommandLine.GetToken( token );
			while ( argument && *argument != '+' && *argument != '-' )
			{
				strncat( command, " ", sizeof( command ) - 1 - strlen( command ) );
				strncat( command, argument, sizeof( command ) - 1 - strlen( command ) );
				argument = m_CommandLine.GetToken( ++token );
			}
			ExecuteString( command );
		}
		else
		{
			argument = m_CommandLine.GetToken( token );
		}
	}

	Printf( "Type 'help' for a list of commands.\n" );
	return true;
}

void System::RunFrame( double time )
{
#if defined(_WIN32)
	static int firstCount = 1;
	static DWORD previousCount;
	float nextConsoleUpdate = 0;
#else
    double nextConsoleUpdate = 0;
#endif
	BaseSystemModule::RunFrame( time );

	while ( m_State == MODULE_RUNNING || m_State == MODULE_PAUSED )
	{
#if defined(_WIN32)
		EnterCriticalSection( &m_ClockLock );
		LARGE_INTEGER counter;
		QueryPerformanceCounter( &counter );
		DWORD count = counter.LowPart;
		if ( m_ClockShift )
			count = ( counter.LowPart >> m_ClockShift ) | ( (DWORD)counter.HighPart << ( 32 - m_ClockShift ) );

		if ( firstCount )
			firstCount = 0;
		else if ( count > previousCount || previousCount - count >= 0x10000000 )
			m_SystemTime = (double)( count - previousCount ) * m_ClockFrequency + m_SystemTime;

		previousCount = count;
		LeaveCriticalSection( &m_ClockLock );
#else
        if (hltv_posix::ShutdownRequested) {
            Stop();
            break;
        }
        if (!m_Clock.Read(m_SystemTime)) {
            char format[] = "Couldn't read monotonic timer: %s\n";
            Errorf(format, std::strerror(errno));
            break;
        }
#endif

		double difference = m_SystemTime - m_LastTime;
		if ( difference == 0 )
		{
#if defined(_WIN32)
			Sleep( m_SleepTime );
#else
            if (!hltv_posix::SleepMilliseconds(m_SleepTime)) {
                char format[] = "Console sleep failed: %s\n";
                Errorf(format, std::strerror(errno));
            }
#endif
			continue;
		}
		if ( m_LastTime > 0 && difference <= 0 )
		{
			Printf( "WARNING! System::RunFrame: system time difference <= 0.\n" );
			difference = 0.001f;
		}

		if ( m_MinFrameTime <= difference )
		{
			m_Tick++;
			m_LastTime = m_SystemTime;
			if ( m_SystemTime > nextConsoleUpdate )
			{
				ExecuteString( m_Console.GetLine() );
				m_Console.SetStatusLine( m_StatusLine );
#if defined(_WIN32)
				nextConsoleUpdate = (float)( m_SystemTime + 0.1f );
#else
                constexpr double ConsoleUpdateSeconds = 0.1;
                nextConsoleUpdate = m_SystemTime + ConsoleUpdateSeconds;
#endif
			}

			if ( m_VGUI )
				vgui2::ivgui()->RunFrame();

			if ( m_State == MODULE_PAUSED )
			{
#if defined(_WIN32)
				Sleep( 1 );
#else
                if (!hltv_posix::SleepMilliseconds(1)) {
                    char format[] = "Console sleep failed: %s\n";
                    Errorf(format, std::strerror(errno));
                }
#endif
			}
			else
			{
#if CSE_NATIVE_SYSTEM_MODULE
                {
                    cse::modules::CallbackScope callback(&m_NativeModules.lifetime);
                    for (ISystemModule* module : m_NativeModules.Running()) {
                        if (m_NativeModules.Running(module)) {
                            module->RunFrame(m_SystemTime);
                        }
                    }
                }
                if (m_NativeModules.stopping) {
                    break;
                }
#else
				ISystemModule* module = (ISystemModule*)m_Modules.GetFirst();
				while ( module )
				{
					if ( m_State == MODULE_DISCONNECTED )
						break;
					module->RunFrame( m_SystemTime );
					module = (ISystemModule*)m_Modules.GetNext();
				}
#endif
			}
		}
		else
		{
#if defined(_WIN32)
			Sleep( m_SleepTime );
#else
            if (!hltv_posix::SleepMilliseconds(m_SleepTime)) {
                char format[] = "Console sleep failed: %s\n";
                Errorf(format, std::strerror(errno));
            }
#endif
		}
	}
}

void System::ShutDown()
{
#if CSE_NATIVE_SYSTEM_MODULE
    if (m_NativeResourcesRetired || m_NativeShutdownActive) {
        return;
    }
    m_NativeShutdownActive = true;
    struct ShutdownScope {
        bool& active;
        ~ShutdownScope() {
            active = false;
        }
    } shutdown{m_NativeShutdownActive};
    Stop();
    m_NativeModules.Stop([this](ISystemModule* removed) {
        RemoveNativeCommands(removed);
        m_Modules.Remove(removed);
    });
    // Keep the filesystem, console and libraries alive while provider frames remain.
    // A later owner call retries retirement after the callback scope has returned.
    if (!m_NativeModules.ReleaseAll()) {
        return;
    }
    m_Commands.Clear(true);
    while (auto* library = static_cast<library_t*>(m_Libraries.RemoveTail())) {
        if (library->module) {
            Sys_UnloadModule(library->module);
        }
        free(library);
    }
    CloseLogFile();
    RetireNativeVGUI();
    if (m_FileSystem) {
        m_FileSystem->Unmount();
        m_FileSystem = nullptr;
    }
    if (m_FileSystemLibrary) {
        Sys_UnloadModule(m_FileSystemLibrary);
        m_FileSystemLibrary = nullptr;
    }
    if (m_Developer) {
        constexpr unsigned int DeveloperShutdownDelay = 3000;
#if defined(_WIN32)
        Sleep(DeveloperShutdownDelay);
#else
        hltv_posix::SleepMilliseconds(DeveloperShutdownDelay);
#endif
    }
    m_RedirectBuffer = nullptr;
    m_RedirectSize = 0;
    m_Console.ShutDown();
    m_NativeResourcesRetired = true;
#else

	m_Commands.Clear( true );
	ISystemModule* module = (ISystemModule*)m_Modules.GetFirst();
	while ( module )
	{
		module->ShutDown();
		module = (ISystemModule*)m_Modules.GetFirst();
	}

	library_t* library = (library_t*)m_Libraries.RemoveTail();
	while ( library )
	{
		if ( library->module )
			Sys_UnloadModule( library->module );
		free( library );
		library = (library_t*)m_Libraries.RemoveTail();
	}

	CloseLogFile();
	if ( m_FileSystem )
		m_FileSystem->Unmount();
	Sys_UnloadModule( m_FileSystemLibrary );

	if ( m_VGUI )
	{
		vgui2::surface()->Shutdown();
		delete m_MainPanel;
		m_MainPanel = NULL;
		Sys_UnloadModule( m_VGuiLibrary );
		Printf( "VGUI shutdown.\n" );
	}

	if ( m_Developer )
#if defined(_WIN32)
		Sleep( 3000 );
#else
    {
        constexpr unsigned int DeveloperShutdownDelay = 3000;
        hltv_posix::SleepMilliseconds(DeveloperShutdownDelay);
    }
#endif
	m_Console.ShutDown();
	m_State = MODULE_DISCONNECTED;
#endif
}

double System::GetTime()
{
	return m_SystemTime;
}

unsigned int System::GetTick()
{
	return m_Tick;
}

void System::SetFPS( float fps )
{
	m_MinFrameTime = fps > 0 ? 1.0f / fps : 0;
}

char* System::GetStatusLine()
{
	static char string[80];
	sprintf( string, " Libraries %i, Modules %i, Commands %i.\n", m_Libraries.CountElements(),
			m_Modules.CountElements(), m_Commands.CountElements() );
	return string;
}

char* System::GetType()
{
	return "basesystem002";
}

void System::Stop()
{
	m_State = MODULE_DISCONNECTED;
}

char* System::COM_GetBaseDir()
{
#if defined(_WIN32)
	static char baseDir[MAX_PATH];
	baseDir[0] = 0;
	if ( GetModuleFileNameA( NULL, baseDir, sizeof( baseDir ) ) )
	{
		char* separator = strrchr( baseDir, '\\' );
		if ( separator && *separator )
			separator[1] = 0;

		int length = (int)strlen( baseDir );
		if ( length > 0 && ( baseDir[length - 1] == '\\' || baseDir[length - 1] == '/' ) )
			baseDir[length - 1] = 0;
	}
	return baseDir;
#else
    static char baseDirectory[hltv_posix::PathCapacity];
    if (!hltv_posix::ExecutableDirectory(baseDirectory, sizeof(baseDirectory))) {
        baseDirectory[0] = 0;
    }
    return baseDirectory;
#endif
}

char* System::CheckParam( char* param )
{
	return m_CommandLine.CheckToken( param );
}

IFileSystem* System::GetFileSystem()
{
	return m_FileSystem;
}

void System::SetTitle( char* title )
{
	m_Console.SetTitle( title );
}

void System::SetStatusLine( char* line )
{
	strncpy( m_StatusLine, line, sizeof( m_StatusLine ) - 1 );
	m_StatusLine[sizeof( m_StatusLine ) - 1] = 0;
}

void System::ShowConsole( bool visible )
{
	m_Console.SetVisible( visible );
}

void System::Printf( char* format, ... )
{
	static char text[8192];
	va_list args;
	va_start( args, format );
	_vsnprintf( text, sizeof( text ), format, args );
	text[sizeof( text ) - 1] = 0;
	va_end( args );

	if ( m_LogFile )
		m_FileSystem->Write( text, (int)strlen( text ), m_LogFile );
	if ( m_RedirectBuffer && strlen( text ) + strlen( m_RedirectBuffer ) + 1 < (size_t)m_RedirectSize )
		strcat( m_RedirectBuffer, text );
	m_Console.Print( text );
}

void System::DPrintf( char* format, ... )
{
	if ( !m_Developer )
		return;

	static char text[8192];
	va_list args;
	va_start( args, format );
	_vsnprintf( text, sizeof( text ), format, args );
	text[sizeof( text ) - 1] = 0;
	va_end( args );

	if ( m_LogFile )
		m_FileSystem->Write( text, (int)strlen( text ), m_LogFile );
	if ( m_RedirectBuffer && strlen( text ) + strlen( m_RedirectBuffer ) + 1 < (size_t)m_RedirectSize )
		strcat( m_RedirectBuffer, text );
	m_Console.Print( text );
}

void System::Errorf( char* format, ... )
{
	static char text[1024];
	va_list args;
	va_start( args, format );
	_vsnprintf( text, sizeof( text ), format, args );
	text[sizeof( text ) - 1] = 0;
	va_end( args );
	Printf( "***** FATAL ERROR *****\n" );
	Printf( "%s", text );
	Printf( "*** STOPPING SYSTEM ***\n" );
	Stop();
}

void System::RedirectOutput( char* buffer, int maxSize )
{
	m_RedirectBuffer = buffer;
	m_RedirectSize = maxSize;
	if ( buffer )
		memset( buffer, 0, maxSize );
}

unsigned char* System::LoadFile( const char* name, int* length )
{
	if ( !m_FileSystem )
		return NULL;
	if ( length )
		*length = 0;
	FileHandle_t file = m_FileSystem->Open( name, "rb" );
	if ( !file )
		return NULL;

	m_FileSystem->Seek( file, 0, FILESYSTEM_SEEK_TAIL );
	int size = m_FileSystem->Tell( file );
	unsigned char* buffer = (unsigned char*)malloc( size + 1 );
	if ( !buffer )
	{
		Printf( "WARNING! System::LoadFile: not enough space for %s.\n", name );
		m_FileSystem->Close( file );
		return NULL;
	}

	buffer[size] = 0;
	m_FileSystem->Seek( file, 0, FILESYSTEM_SEEK_HEAD );
	m_FileSystem->Read( buffer, size, file );
	m_FileSystem->Close( file );
	if ( length )
		*length = size;
	return buffer;
}

void System::FreeFile( unsigned char* buffer )
{
	if ( buffer )
		free( buffer );
}

void System::LogConsole( char* filename )
{
	if ( filename )
		OpenLogFile( filename );
	else
		CloseLogFile();
}

void System::OpenLogFile( char* filename )
{
	CloseLogFile();
	if ( *filename == '/' || strstr( filename, ":" ) || strstr( filename, ".." ) || strstr( filename, "\\" ) )
	{
		Printf( "WARNING! System::OpenLogFile: unable to open %s (contains illegal characters).\n", filename );
		return;
	}

#if defined(_WIN32)
	strncpy( m_LogFileName, filename, 255 );
	m_LogFileName[255] = 0;
#else
    constexpr std::size_t ExtensionBytes = sizeof(".txt");
    const std::size_t filenameLength = std::strlen(filename);
    if (filenameLength > sizeof(m_LogFileName) - ExtensionBytes) {
        char format[] = "System::OpenLogFile: filename is too long: %s\n";
        Printf(format, filename);
        return;
    }
    std::memcpy(m_LogFileName, filename, filenameLength + 1);
#endif
	COM_DefaultExtension( m_LogFileName, ".txt" );
	if ( strcmp( COM_FileExtension( m_LogFileName ), "txt" ) )
	{
		memset( m_LogFileName, 0, sizeof( m_LogFileName ) );
		Printf( "WARNING! System::OpenLogFile: unable to open %s (wrong file extension, must be .txt).\n", filename );
		return;
	}

	m_LogFile = m_FileSystem->Open( m_LogFileName, "wt" );
	if ( !m_LogFile )
	{
		memset( m_LogFileName, 0, sizeof( m_LogFileName ) );
		Printf( "System::OpenLogFile: error while opening logfile.\n" );
	}
}

void System::CloseLogFile()
{
	if ( m_LogFile && m_FileSystem )
	{
		m_FileSystem->Flush( m_LogFile );
		m_FileSystem->Close( m_LogFile );
		m_LogFile = NULL;
		Printf( "Console log file closed.\n" );
		memset( m_LogFileName, 0, sizeof( m_LogFileName ) );
	}
}

bool System::AddModule( ISystemModule* module, char* name )
{
#if CSE_NATIVE_SYSTEM_MODULE
    if (!m_NativeModules.Add(this, module, name, nullptr, [this](ISystemModule* removed) {
        RemoveNativeCommands(removed);
        m_Modules.Remove(removed);
    })) {
        return false;
    }
    if (!m_Modules.AddHead(module)) {
        RemoveModule(module);
        return false;
    }
    return true;
#else

	if ( !module )
		return false;
	if ( !module->Init( this, m_NextSerial, name ) )
	{
		Printf( "ERROR! System::AddModule: couldn't initialize module %s.\n", name );
		return false;
	}

	m_Modules.AddHead( module );
	m_NextSerial++;
	return true;

#endif
}

bool System::RemoveModule( ISystemModule* module )
{
#if CSE_NATIVE_SYSTEM_MODULE
    return m_NativeModules.Remove(module, [this](ISystemModule* removed) {
        RemoveNativeCommands(removed);
        m_Modules.Remove(removed);
    });
#else

	if ( !module )
		return true;
	module->ShutDown();

	command_t* command = (command_t*)m_Commands.GetFirst();
	while ( command )
	{
		if ( command->module->GetSerial() == module->GetSerial() )
		{
			m_Commands.Remove( command );
			free( command );
		}
		command = (command_t*)m_Commands.GetNext();
	}

	ISystemModule* entry = (ISystemModule*)m_Modules.GetFirst();
	while ( entry )
	{
		if ( entry->GetSerial() == module->GetSerial() )
		{
			m_Modules.Remove( entry );
			return true;
		}
		entry = (ISystemModule*)m_Modules.GetNext();
	}
	return false;

#endif
}

ISystemModule* System::FindModule( char* interfaceName, char* instanceName )
{
#if CSE_NATIVE_SYSTEM_MODULE
    if (!interfaceName || !*interfaceName) {
        return nullptr;
    }
    for (ISystemModule* module : m_NativeModules.Running()) {
        if ((module->QueryInterface(interfaceName) || !_stricmp(interfaceName, module->GetType())) && (!instanceName || !strcmp(instanceName, module->GetName()))) {
            return module;
        }
    }
    return nullptr;
#else

	if ( !interfaceName || !*interfaceName )
		return NULL;

	ISystemModule* module = (ISystemModule*)m_Modules.GetFirst();
	while ( module )
	{
		if ( !_stricmp( interfaceName, module->GetType() ) )
		{
			if ( !instanceName || !_stricmp( instanceName, module->GetName() ) )
				return module;
		}
		module = (ISystemModule*)m_Modules.GetNext();
	}
	return NULL;

#endif
}

ISystemModule* System::GetModule( char* interfaceName, char* libraryName, char* instanceName )
{
	ISystemModule* module = FindModule( interfaceName, instanceName );
	if ( module )
		return module;

#if CSE_NATIVE_SYSTEM_MODULE
    if (!m_NativeModules.CanCreate() || !libraryName || !*libraryName) {
        return nullptr;
    }
#endif

#if defined(_WIN32)
	char filename[MAX_PATH];
	strncpy( filename, libraryName, sizeof( filename ) - 1 );
	filename[sizeof( filename ) - 1] = 0;
	for ( char* character = filename; *character; character++ )
	{
		if ( *character == '/' )
			*character = '\\';
	}
#else
    char filename[hltv_posix::PathCapacity];
    constexpr std::size_t LibrarySuffixBytes = sizeof(".dll");
    const std::size_t filenameLength = strnlen(libraryName, sizeof(filename));
    if (filenameLength > sizeof(filename) - LibrarySuffixBytes) {
        char format[] = "ERROR! System::GetModule: library path is too long (%s).\n";
        DPrintf(format, libraryName);
        return nullptr;
    }
    std::memcpy(filename, libraryName, filenameLength + 1);
#endif

	library_t* library = (library_t*)m_Libraries.GetFirst();
	while ( library && _stricmp( libraryName, library->name ) )
		library = (library_t*)m_Libraries.GetNext();

	if ( !library )
	{
		library = (library_t*)malloc( sizeof( library_t ) );
		if ( !library )
		{
			DPrintf( "ERROR! System::GetLibrary: out of memory (%s).\n", libraryName );
			return NULL;
		}

		_snprintf( library->name, sizeof( library->name ), "%s.dll", filename );
#if defined(_WIN32)
		library->module = Sys_LoadModule( library->name );
#else
        library->module = hltv_posix::LoadRuntimeModule(library->name, m_BaseDir, Sys_LoadModule);
#endif
		if ( !library->module )
		{
			DPrintf( "WARNING! System::GetLibrary: coulnd't load library (%s).\n", library->name );
			free( library );
			return NULL;
		}

		library->factory = Sys_GetFactory( library->module );
		if ( !library->factory )
		{
			DPrintf( "ERROR! System::GetLibrary: coulnd't get object factory(%s).\n", library->name );
			free( library );
			return NULL;
		}

		m_Libraries.Add( library );
		DPrintf( "Loaded library %s.\n", library->name );
	}

#if CSE_NATIVE_SYSTEM_MODULE
    const auto* provider = cse::modules::Admit(library->factory);
    if (!provider) {
        Printf(const_cast<char*>("Rejected incompatible native system-module provider %s.\n"), library->name);
        return nullptr;
    }
    module = provider->Create(interfaceName);
    if (!module) {
        return nullptr;
    }
    if (!m_NativeModules.Add(this, module, instanceName, provider, [this](ISystemModule* removed) {
        RemoveNativeCommands(removed);
        m_Modules.Remove(removed);
    })) {
        return nullptr;
    }
    if (!m_Modules.AddHead(module)) {
        RemoveModule(module);
        return nullptr;
    }
    return module;
#else
	module = (ISystemModule*)library->factory( interfaceName, NULL );
	if ( !module )
	{
		DPrintf( "ERROR! System::GetModule: interface \"%s\" not found in library %s.\n", interfaceName, library->name );
		return NULL;
	}

	AddModule( module, instanceName );
	return module;
#endif
}

bool System::RegisterCommand( char* name, ISystemModule* module, int commandID )
{
	command_t* command = (command_t*)m_Commands.GetFirst();
	while ( command )
	{
		if ( !_stricmp( command->name, name ) )
		{
			Printf( "WARNING! System::RegisterCommand: command \"%s\" already exists.\n", name );
			return false;
		}
		command = (command_t*)m_Commands.GetNext();
	}

	command = (command_t*)malloc( sizeof( command_t ) );
#if CSE_NATIVE_SYSTEM_MODULE
    if (!command) {
        return false;
    }
#endif
	strncpy( command->name, name, sizeof( command->name ) );
	command->module = module;
	command->name[sizeof( command->name ) - 1] = 0;
	command->id = commandID;
#if CSE_NATIVE_SYSTEM_MODULE
    if (!m_Commands.Add(command)) {
        free(command);
        return false;
    }
#else
	m_Commands.Add( command );
#endif
	return true;
}

void System::GetCommandMatches( char* string, ObjectList* matches )
{
	size_t length = strlen( string );
	command_t* command = (command_t*)m_Commands.GetFirst();
	while ( command )
	{
		if ( !strncmp( command->name, string, length ) )
			matches->AddTail( command->name );
		command = (command_t*)m_Commands.GetNext();
	}
}

void System::ExecuteFile( char* filename )
{
	if ( *filename == '/' || strstr( filename, ":" ) || strstr( filename, ".." ) || strstr( filename, "\\" ) )
	{
		Printf( "WARNING! System::ExecuteFile: unable to open %s (contains illegal characters).\n", filename );
		return;
	}

	FileHandle_t file = m_FileSystem->Open( filename, "rt" );
	if ( !file )
	{
		Printf( "Couldn't open config file %s.\n", filename );
		return;
	}

	Printf( "Executing file %s.\n", filename );
	char line[256];
	while ( m_FileSystem->ReadLine( line, sizeof( line ) - 1, file ) )
	{
		char* comment = strstr( line, "//" );
		if ( comment )
			*comment = 0;
		ExecuteString( line );
	}
	m_FileSystem->Close( file );
}

void System::ExecuteString( char* commands )
{
	if ( !commands || !*commands )
		return;

	for ( char* character = commands; *character; character++ )
	{
		if ( (unsigned char)( *character - ' ' ) > '~' - ' ' || *character == '%' )
			*character = ' ';
	}

	char text[256];
	char* output = text;
	bool quoted = false;
	unsigned int length = 1;
	for ( char* character = commands; ; character++, length++ )
	{
		*output = *character;
		if ( *character == '"' )
		{
			quoted = !quoted;
			output++;
		}
		else if ( ( *character == ';' && !quoted ) || !*character )
		{
			*output = 0;
			char* commandText = text;
			while ( *commandText == ' ' )
				commandText++;

			if ( *commandText )
			{
				TokenLine tokens( commandText );
				command_t* command = (command_t*)m_Commands.GetFirst();
				while ( command && _stricmp( command->name, tokens.GetToken( 0 ) ) )
					command = (command_t*)m_Commands.GetNext();

                if (command) {
#if CSE_NATIVE_SYSTEM_MODULE
                    cse::modules::CallbackScope callback(&m_NativeModules.lifetime);
#endif
                    command->module->ExecuteCommand(command->id, commandText);
                } else {
                    Printf("WARNING! System::DispatchCommand: command \"%s\" not registered.\n", commandText);
                }
			}

			output = text;
			length = 0;
			if ( !*character )
				return;
		}
		else
		{
			output++;
		}

		if ( length + 1 >= sizeof( text ) )
		{
			Printf( "WARNING! System::ExecuteString: Command token too long.\n" );
			return;
		}
	}
}

void System::ExecuteCommand( int commandID, char* commandLine )
{
	switch ( commandID )
	{
	case CMD_DEVELOPER:
	{
		TokenLine params( commandLine );
		if ( params.CountToken() == 2 )
		{
			m_Developer = atoi( params.GetToken( 1 ) ) != 0;
		}
		else
		{
			g_System.Printf( "Syntax: developer <0|1>\n" );
			g_System.Printf( "Developer mode is %s.\n", m_Developer ? "on" : "off" );
		}
		break;
	}
	case CMD_EXEC:
	{
		TokenLine params( commandLine );
		if ( params.CountToken() == 2 )
			ExecuteFile( params.GetToken( 1 ) );
		else
			g_System.Printf( "Syntax: exec <file.cfg>\n" );
		break;
	}
	case CMD_MODULES:
	{
		ISystemModule* module = (ISystemModule*)m_Modules.GetFirst();
		while ( module )
		{
			Printf( "Module %s, Interface %s, Version %i\n", module->GetName(), module->GetType(), module->GetVersion() );
			module = (ISystemModule*)m_Modules.GetNext();
		}
		Printf( "--- %i modules in total ---\n", m_Modules.CountElements() );
		break;
	}
	case CMD_LIST:
	{
		Printf( "Registerd console commands:" );
		command_t* command = (command_t*)m_Commands.GetFirst();
		while ( command )
		{
			Printf( "\n %s", command->name );
			DPrintf( " (%s)", command->module->GetName() );
			command = (command_t*)m_Commands.GetNext();
		}
		Printf( "\n" );
		break;
	}
	case CMD_LOGFILE:
	{
		TokenLine params( commandLine );
		if ( params.CountToken() == 2 )
		{
			char* filename = params.GetToken( 1 );
			if ( !_stricmp( filename, "1" ) )
			{
				if ( m_LogFile )
				{
					Printf( "Console is logged to \"%s\".\n", m_LogFileName );
				}
				else
				{
					char name[MAX_PATH];
					sprintf( name, "logfile%s.txt", COM_TimeString() );
					OpenLogFile( name );
				}
			}
			else if ( !_stricmp( filename, "0" ) )
			{
				CloseLogFile();
			}
			else
			{
				OpenLogFile( filename );
			}
		}
		else
		{
			Printf( "Syntax: logfile <0|1|filename>\n" );
			if ( m_LogFile )
				Printf( "Console is logged to \"%s\".\n", m_LogFileName );
			else
				Printf( "Logging is off.\n" );
		}
		break;
	}
	case CMD_QUIT:
		Stop();
		break;
	case CMD_LOADMODULE:
	{
		TokenLine params( commandLine );
		if ( params.CountToken() < 2 )
			g_System.Printf( "Syntax: loadmodule <module> [<library>] [<name>]\n" );
		else if ( params.CountToken() == 2 )
			GetModule( params.GetToken( 1 ), params.GetToken( 1 ), NULL );
		else if ( params.CountToken() == 3 )
			GetModule( params.GetToken( 1 ), params.GetToken( 2 ), NULL );
		else
			GetModule( params.GetToken( 1 ), params.GetToken( 2 ), params.GetToken( 3 ) );
		break;
	}
	case CMD_UNLOADMODULE:
	{
		TokenLine params( commandLine );
		if ( params.CountToken() < 2 )
		{
			Printf( "Syntax: unloadmodule <module> [<name>]\n" );
			break;
		}

		ISystemModule* module = NULL;
		if ( params.CountToken() == 2 )
			module = FindModule( params.GetToken( 1 ), NULL );
		else if ( params.CountToken() == 3 )
			module = FindModule( params.GetToken( 1 ), params.GetToken( 2 ) );

		if ( module )
			RemoveModule( module );
		else
			Printf( "Module not found.\n" );
		break;
	}
	case CMD_TITLE:
	{
		TokenLine params( commandLine );
		if ( params.CountToken() == 2 )
			m_Console.SetTitle( params.GetToken( 1 ) );
		else
			Printf( "Syntax: title <text>\n" );
		break;
	}
	case CMD_SHOWCON:
	{
		TokenLine params( commandLine );
		if ( params.CountToken() == 2 )
			ShowConsole( *params.GetToken( 1 ) != '0' );
		else
			Printf( "Syntax: showcon <0|1>\n" );
		break;
	}
	case CMD_ECHO:
	{
		TokenLine params( commandLine );
		if ( params.CountToken() >= 2 )
			Printf( "%s\n", params.GetRestOfLine( 1 ) );
		break;
	}
	default:
		Printf( "ERROR! System::ExecuteCommand: unknown command ID %i.\n", commandID );
		break;
	}
}

#if CSE_NATIVE_SYSTEM_MODULE
void System::RetireNativeVGUI() {
    // Cleanup may call back into the owner. Preserve an outer shutdown guard and
    // prevent recursive retirement while panel/surface/provider frames are live.
    struct RetirementScope {
        bool& active;
        bool previous;
        ~RetirementScope() {
            active = previous;
        }
    } retirement{m_NativeShutdownActive, m_NativeShutdownActive};
    m_NativeShutdownActive = true;
    if (m_VGUI) {
        m_VGUI = false;
        vgui2::ivgui()->Stop();
    }
    if (m_MainPanel) {
        vgui2::Panel* panel = m_MainPanel;
        m_MainPanel = nullptr;
        vgui2::surface()->SetEmbeddedPanel(0);
        delete panel;
    }
    if (m_NativeVGuiControlsReady) {
        m_NativeVGuiControlsReady = false;
        // Panel destruction still needs the active control services, so it must
        // precede surface shutdown (which also shuts down IVGui in this provider).
        vgui2::surface()->Shutdown();
    }
    CSysModule* library = m_VGuiLibrary;
    m_VGuiLibrary = nullptr;
    std::memset(m_Factories, 0, sizeof(m_Factories));
    if (library) {
        Sys_UnloadModule(library);
    }
}
#endif

bool System::StartVGUI()
{
#if CSE_NATIVE_SYSTEM_MODULE
    if (m_NativeResourcesRetired || m_NativeShutdownActive) {
        return false;
    }
    // A prior unsuccessful acquisition must not be overwritten by a retry.
    RetireNativeVGUI();
#endif
	if ( !m_FileSystemLibrary )
	{
		Printf( "System::StartVGUI: filesystem not loaded." );
		return false;
	}
	if ( CheckParam( "-novgui" ) )
		return false;

#if defined(_WIN32)
	m_VGuiLibrary = Sys_LoadModule( "vgui2.dll" );
#else
    m_VGuiLibrary = hltv_posix::LoadRuntimeModule("vgui2.dll", m_BaseDir, Sys_LoadModule);
#endif
	m_Factories[0] = Sys_GetFactoryThis();
	m_Factories[1] = Sys_GetFactory( m_VGuiLibrary );
	m_Factories[2] = Sys_GetFactory( m_FileSystemLibrary );
	if ( !vgui2::VGuiControls_Init( "HLTV", m_Factories, 3 ) )
	{
		Printf( "System::StartVGUI: Could not initialize vgui." );
#if CSE_NATIVE_SYSTEM_MODULE
        RetireNativeVGUI();
#endif
		return false;
	}

#if CSE_NATIVE_SYSTEM_MODULE
    m_NativeVGuiControlsReady = true;
#endif
	vgui2::filesystem()->AddSearchPath( "platform", "PLATFORM" );
	vgui2::localize()->AddFile( vgui2::filesystem(), "Resource/platform_%language%.txt" );
	vgui2::localize()->AddFile( vgui2::filesystem(), "Resource/vgui_%language%.txt" );
	m_MainPanel = new vgui2::Panel( NULL, "MainPanel" );
	vgui2::surface()->SetEmbeddedPanel( m_MainPanel->GetVPanel() );
	if ( !vgui2::scheme()->LoadSchemeFromFile( "platform/resource/TrackerScheme.res", "DEFAULT" ) )
	{
		Printf( "System::StartVGUI: Could not load TrackerScheme.res\n" );
#if CSE_NATIVE_SYSTEM_MODULE
        RetireNativeVGUI();
#endif
		return false;
	}

	vgui2::ivgui()->Start();
	m_VGUI = true;
	Printf( "VGUI initialized.\n" );
	return true;
}

bool System::InitVGUI( IVGuiModule* module )
{
	if ( !m_VGUI && !StartVGUI() )
		return false;
	module->Initialize( m_Factories, 3 );
	module->PostInitialize( NULL, 0 );
	module->SetParent( m_MainPanel->GetVPanel() );
	return true;
}

#if CSE_NATIVE_SYSTEM_MODULE && defined(_WIN32)
void* System::GetPanel()
#else
vgui2::Panel* System::GetPanel()
#endif
{
	if ( m_VGUI || StartVGUI() )
		return m_MainPanel;
	return NULL;
}

void System_Printf( const char* format, ... )
{
	static char text[8192];
	va_list args;
	va_start( args, format );
	_vsnprintf( text, sizeof( text ), format, args );
	text[sizeof( text ) - 1] = 0;
	va_end( args );
	g_System.Printf( "%s", text );
}

#if defined(_WIN32)
int WINAPI WinMain( _In_ HINSTANCE, _In_opt_ HINSTANCE, _In_ LPSTR commandLine, _In_ int )
{
	g_System.m_CommandLine.SetLine( commandLine );
#if CSE_NATIVE_SYSTEM_MODULE
    const bool initialized = g_System.Init(&g_System, 0, "Console");
    if (initialized) {
        g_System.RunFrame(0);
    }
    g_System.ShutDown();
    return initialized ? EXIT_SUCCESS : EXIT_FAILURE;
#else
	if ( g_System.Init( &g_System, 0, "Console" ) )
	{
		g_System.RunFrame( 0 );
		g_System.ShutDown();
	}
	else
	{
		Sleep( 2000 );
	}
	return 0;
#endif
}
#else
int main(int argc, char** argv) {
    const char* argumentError = nullptr;
    if (!hltv_posix::SetArguments(g_System.m_CommandLine, argc, argv, &argumentError)) {
        std::fprintf(stderr, "HLTV: %s\n", argumentError);
        return EXIT_FAILURE;
    }
    hltv_posix::SignalScope signals;
    if (!signals.Install()) {
        std::fprintf(stderr, "HLTV: cannot install signal handlers: %s\n", std::strerror(errno));
        return EXIT_FAILURE;
    }
    int result = EXIT_FAILURE;
    struct ConsoleCleanup {
        CTextConsole& console;
        ~ConsoleCleanup() {
            console.ShutDown();
        }
    } cleanup{g_System.m_Console};
    try {
        char name[] = "Console";
        if (g_System.Init(&g_System, 0, name)) {
            g_System.RunFrame(0);
            result = EXIT_SUCCESS;
        }
    } catch (const std::exception& error) {
        std::fprintf(stderr, "HLTV: %s\n", error.what());
    } catch (...) {
        std::fprintf(stderr, "HLTV: native system exception\n");
    }
    try {
        g_System.ShutDown();
    } catch (const std::exception& error) {
        std::fprintf(stderr, "HLTV: shutdown failed: %s\n", error.what());
        result = EXIT_FAILURE;
    } catch (...) {
        std::fprintf(stderr, "HLTV: native shutdown exception\n");
        result = EXIT_FAILURE;
    }
    return result;
}
#endif

#if CSE_NATIVE_SYSTEM_MODULE
void System::RemoveNativeCommands(ISystemModule* module) {
    std::vector<command_t*> commands;
    for (auto* command = static_cast<command_t*>(m_Commands.GetFirst()); command;
         command = static_cast<command_t*>(m_Commands.GetNext())) {
        if (command->module == module) {
            commands.push_back(command);
        }
    }
    for (auto* command : commands) {
        m_Commands.Remove(command);
        free(command);
    }
}
#endif
