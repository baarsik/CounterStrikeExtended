#pragma once

#include "common/BaseSystemModule.h"
#include "common/IBaseSystem.h"
#include "common/TokenLine.h"
#if defined(_WIN32)
#include "TextConsoleWin32.h"
#else
#include "TextConsolePosix.h"
#include "PosixSystem.h"
#include <cstdint>
namespace vgui2 {
class Panel;
}
#endif
#if defined(_WIN32)
#include "FileSystem.h"
#else
#include "filesystem.h"
#endif

#pragma warning( push )
#pragma warning( disable : 4250 )

class System : public BaseSystemModule, public IBaseSystem
{
public:
	System();
	~System() override;

	bool Init( IBaseSystem* system, int serial, char* name ) override;
	void RunFrame( double time ) override;
	void ExecuteCommand( int commandID, char* commandLine ) override;
	char* GetStatusLine() override;
	char* GetType() override;
	void ShutDown() override;

	double GetTime() override;
	unsigned int GetTick() override;
	void SetFPS( float fps ) override;
	void Printf( char* format, ... ) override;
	void DPrintf( char* format, ... ) override;
	void RedirectOutput( char* buffer, int maxSize ) override;
	IFileSystem* GetFileSystem() override;
	unsigned char* LoadFile( const char* name, int* length ) override;
	void FreeFile( unsigned char* buffer ) override;
	void SetTitle( char* title ) override;
	void SetStatusLine( char* line ) override;
	void ShowConsole( bool visible ) override;
	void LogConsole( char* filename ) override;
	bool InitVGUI( IVGuiModule* module ) override;
#if defined(_WIN32)
	vgui2::Panel* GetPanel() override;
#else
    vgui2::Panel* GetPanel();
#endif
	bool RegisterCommand( char* name, ISystemModule* module, int commandID ) override;
	void GetCommandMatches( char* string, ObjectList* matches ) override;
	void ExecuteString( char* commands ) override;
	void ExecuteFile( char* filename ) override;
	void Errorf( char* format, ... ) override;
	char* CheckParam( char* param ) override;
	bool AddModule( ISystemModule* module, char* name ) override;
	ISystemModule* GetModule( char* interfaceName, char* libraryName, char* instanceName ) override;
	bool RemoveModule( ISystemModule* module ) override;
	void Stop() override;
	char* COM_GetBaseDir() override;

private:
#if defined(_WIN32)
	friend int WINAPI WinMain( _In_ HINSTANCE instance, _In_opt_ HINSTANCE previous,
			_In_ LPSTR commandLine, _In_ int show );
#else
    friend int main(int argc, char** argv);
#endif

	enum SystemCommand
	{
		CMD_DEVELOPER = 1,
		CMD_EXEC,
		CMD_MODULES,
		CMD_LIST,
		CMD_LOGFILE,
		CMD_QUIT,
		CMD_LOADMODULE,
		CMD_UNLOADMODULE,
		CMD_TITLE,
		CMD_SHOWCON,
		CMD_ECHO
	};

	enum
	{
		MODULE_PAUSED = 2
	};

	typedef struct command_s
	{
		char			name[32];
		int				id;
		ISystemModule*	module;
	} command_t;

	typedef struct library_s
	{
#if defined(_WIN32)
		char				name[MAX_PATH];
#else
        char name[hltv_posix::PathCapacity];
#endif
		CSysModule*			module;
		CreateInterfaceFn	factory;
	} library_t;

	ISystemModule* FindModule( char* interfaceName, char* instanceName );
	void OpenLogFile( char* filename );
	void CloseLogFile();
	bool StartVGUI();

	double				m_LastTime;
#if defined(_WIN32)
	CTextConsoleWin32	m_Console;
#else
    CTextConsolePosix m_Console;
#endif
	unsigned char		m_ReservedConsole[4];
#if defined(_WIN32)
	CRITICAL_SECTION	m_ClockLock;
	int					m_ClockShift;
	double				m_ClockFrequency;
	unsigned char		m_ReservedClock[8];
#else
    hltv_posix::MonotonicClock m_Clock;
#endif
	CSysModule*			m_FileSystemLibrary;
	IFileSystem*		m_FileSystem;
	CSysModule*			m_VGuiLibrary;
	TokenLine			m_CommandLine;
#if defined(_WIN32)
	char				m_BaseDir[MAX_PATH];
#else
    char m_BaseDir[hltv_posix::PathCapacity];
#endif
	char				m_StatusLine[80];
	FileHandle_t		m_LogFile;
#if defined(_WIN32)
	char				m_LogFileName[MAX_PATH];
#else
    char m_LogFileName[hltv_posix::PathCapacity];
#endif
	ObjectList			m_Modules;
	ObjectList			m_Libraries;
	ObjectList			m_Commands;
	float				m_MinFrameTime;
	char*				m_RedirectBuffer;
	int					m_RedirectSize;
	bool				m_Developer;
	int					m_NextSerial;
	unsigned int		m_Tick;
#if defined(_WIN32)
	DWORD				m_SleepTime;
#else
    std::uint32_t m_SleepTime;
#endif
	CreateInterfaceFn	m_Factories[3];
	bool				m_VGUI;
	vgui2::Panel*		m_MainPanel;
};

#pragma warning( pop )

void System_Printf( const char* format, ... );
