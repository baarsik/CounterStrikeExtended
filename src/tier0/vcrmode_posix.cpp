#if !defined( _WIN32 )

#include "tier0/dbg.h"
#include "tier0/threadtools.h"

#include <sys/socket.h>
#include <sys/types.h>

#include <errno.h>
#include <float.h>
#include <limits.h>
#include <new>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

typedef char tchar;
typedef unsigned char uint8;
typedef unsigned int uint32;

static_assert(CHAR_BIT == 8 && sizeof(int) == 4 && sizeof(uint32) == 4, "VCR v2 uses 32-bit scalar fields");
static_assert(sizeof(double) == 8 && DBL_MANT_DIG == 53 && DBL_MAX_EXP == 1024, "VCR v2 uses IEEE binary64");
static_assert(sizeof(off_t) >= 8, "VCR native file positions require large-file support");

constexpr int VcrNoSavedMode = -1;
constexpr int VcrNoReadEvent = -1;
constexpr int VcrSocketFailure = -1;

enum VCRMode
{
	VCR_Disabled	= 0,
	VCR_Record		= 1,
	VCR_Playback	= 2
};

enum VCREvent
{
	VCREvent_Sys_FloatTime					= 0,
	VCREvent_recvfrom						= 1,
	VCREvent_SyncToken						= 2,
	VCREvent_GetCursorPos					= 3,
	VCREvent_SetCursorPos					= 4,
	VCREvent_ScreenToClient					= 5,
	VCREvent_Cmd_Exec						= 6,
	VCREvent_CmdLine						= 7,
	VCREvent_RegOpenKeyEx					= 8,
	VCREvent_RegSetValueEx					= 9,
	VCREvent_RegQueryValueEx				= 10,
	VCREvent_RegCreateKeyEx					= 11,
	VCREvent_RegCloseKey					= 12,
	VCREvent_PeekMessage					= 13,
	VCREvent_GameMsg						= 14,
	VCREvent_GetNumberOfConsoleInputEvents	= 15,
	VCREvent_ReadConsoleInput				= 16,
	VCREvent_GetKeyState					= 17,
	VCREvent_recv							= 18,
	VCREvent_send							= 19,
	VCREvent_Generic						= 20,
	VCREvent_CreateThread					= 21,
	VCREvent_WaitForSingleObject			= 22,
	VCREvent_EnterCriticalSection			= 23,
	VCREvent_Time							= 24,
	VCREvent_LocalTime						= 25,
	VCREvent_GenericString					= 26,

	VCREvent_NUMEVENTS						= 27
};

struct tagMSG;
struct tagPOINT;
struct sockaddr;

class IVCRHelpers
{
public:
	virtual void ErrorMessage( char* pMsg ) = 0;
};

class IVCRTrace
{
public:
	virtual VCREvent ReadEvent() = 0;
	virtual void Read( void* pDest, int size ) = 0;
};

class CVCRTrace : public IVCRTrace
{
public:
	virtual VCREvent ReadEvent() override;
	virtual void Read( void* pDest, int size ) override;
};

struct VCR_t
{
	int				( *Start )( const tchar* pFilename, bool bRecord, IVCRHelpers* pHelpers );
	void			( *End )();
	IVCRTrace*		( *GetVCRTraceInterface )();
	VCRMode			( *GetMode )();
	void			( *SetEnabled )( int bEnabled );
	void			( *SyncToken )( const tchar* pToken );

	double			( *Hook_Sys_FloatTime )( double flTime );
	int				( *Hook_PeekMessage )( tagMSG* msg, void* hWnd, unsigned int wMsgFilterMin,
						unsigned int wMsgFilterMax, unsigned int wRemoveMsg );
	void			( *Hook_RecordGameMsg )( unsigned int uMsg, unsigned int wParam, int lParam );
	void			( *Hook_RecordEndGameMsg )();
	bool			( *Hook_PlaybackGameMsg )( unsigned int* uMsg, unsigned int* wParam, int* lParam );
	int				( *Hook_recvfrom )( int s, char* buf, int len, int flags, sockaddr* from, int* fromlen );
	void			( *Hook_GetCursorPos )( tagPOINT* pt );
	void			( *Hook_ScreenToClient )( void* hWnd, tagPOINT* pt );
	void			( *Hook_Cmd_Exec )( tchar** f );
	tchar*			( *Hook_GetCommandLine )();
	int				( *Hook_RegOpenKeyEx )( void* hKey, const tchar* lpSubKey, unsigned int ulOptions,
						unsigned int samDesired, void* phkResult );
	int				( *Hook_RegSetValueEx )( void* hKey, const tchar* lpValueName, unsigned int Reserved,
						unsigned int dwType, const uint8* lpData, unsigned int cbData );
	int				( *Hook_RegQueryValueEx )( void* hKey, const tchar* lpValueName, unsigned int* lpReserved,
						unsigned int* lpType, uint8* lpData, unsigned int* lpcbData );
	int				( *Hook_RegCreateKeyEx )( void* hKey, const tchar* lpSubKey, unsigned int Reserved,
						tchar* lpClass, unsigned int dwOptions, unsigned int samDesired, void* lpSecurityAttributes,
						void* phkResult, unsigned int* lpdwDisposition );
	void			( *Hook_RegCloseKey )( void* hKey );
	int				( *Hook_GetNumberOfConsoleInputEvents )( void* hInput, unsigned int* pNumEvents );
	int				( *Hook_ReadConsoleInput )( void* hInput, void* pRecs, int nMaxRecs, unsigned int* pNumRead );
	void			( *Hook_LocalTime )( tm* today );
	short			( *Hook_GetKeyState )( int nVirtKey );
	int				( *Hook_recv )( int s, char* buf, int len, int flags );
	int				( *Hook_send )( int s, const char* buf, int len, int flags );

	void			( *GenericRecord )( const tchar* pEventName, const void* pData, int len );
	int				( *GenericPlayback )( const tchar* pEventName, void* pOutData, int maxLen, bool bForceSameLen );
	void			( *GenericValue )( const tchar* pEventName, void* pData, int maxLen );
	double			( *GetPercentCompleted )();

	void*			( *Hook_CreateThread )( void* pSecAttr, unsigned int cbStack, void* pStart, void* pParam,
						unsigned int dwFlags, unsigned int* pThreadId );
	unsigned int	( *Hook_WaitForSingleObject )( void* handle, unsigned int dwMilliseconds );
	void			( *Hook_EnterCriticalSection )( void* pCS );
	void			( *Hook_Time )( time_t* today );
	void			( *GenericString )( const char* pEventName, const char* pString );
	void			( *GenericValueVerify )( const tchar* pEventName, const void* pData, int maxLen );
	unsigned int	( *Hook_WaitForMultipleObjects )( uint32 nCount, const void** pHandles, int bWaitAll,
						uint32 dwMilliseconds );
};

static int			VCR_Start( const char* pFilename, bool bRecord, IVCRHelpers* pHelpers );
static void			VCR_End();
static IVCRTrace*	VCR_GetVCRTraceInterface();
static VCRMode		VCR_GetMode();
static void			VCR_SetEnabled( int bEnabled );
static void			VCR_SyncToken( const char* pToken );
static double		VCR_Hook_Sys_FloatTime( double flTime );
static int			VCR_Hook_PeekMessage( tagMSG* msg, void* hWnd, unsigned int wMsgFilterMin,
						unsigned int wMsgFilterMax, unsigned int wRemoveMsg );
void				VCR_Hook_RecordGameMsg( unsigned int uMsg, unsigned int wParam, int lParam );
void				VCR_Hook_RecordEndGameMsg();
bool				VCR_Hook_PlaybackGameMsg( unsigned int* uMsg, unsigned int* wParam, int* lParam );
static int			VCR_Hook_recvfrom( int s, char* buf, int len, int flags, sockaddr* from, int* fromlen );
static void			VCR_Hook_GetCursorPos( tagPOINT* pt );
static void			VCR_Hook_ScreenToClient( void* hWnd, tagPOINT* pt );
static void			VCR_Hook_Cmd_Exec( char** f );
static char*		VCR_Hook_GetCommandLine();
static int			VCR_Hook_RegOpenKeyEx( void* hKey, const char* lpSubKey, unsigned int ulOptions,
						unsigned int samDesired, void* phkResult );
static int			VCR_Hook_RegSetValueEx( void* hKey, const char* lpValueName, unsigned int Reserved,
						unsigned int dwType, const uint8* lpData, unsigned int cbData );
static int			VCR_Hook_RegQueryValueEx( void* hKey, const char* lpValueName, unsigned int* lpReserved,
						unsigned int* lpType, uint8* lpData, unsigned int* lpcbData );
static int			VCR_Hook_RegCreateKeyEx( void* hKey, const char* lpSubKey, unsigned int Reserved,
						char* lpClass, unsigned int dwOptions, unsigned int samDesired, void* lpSecurityAttributes,
						void* phkResult, unsigned int* lpdwDisposition );
static void			VCR_Hook_RegCloseKey( void* hKey );
int					VCR_Hook_GetNumberOfConsoleInputEvents( void* hInput, unsigned int* pNumEvents );
int					VCR_Hook_ReadConsoleInput( void* hInput, void* pRecs, int nMaxRecs, unsigned int* pNumRead );
void				VCR_Hook_LocalTime( tm* today );
short				VCR_Hook_GetKeyState( int nVirtKey );
static int			VCR_Hook_recv( int s, char* buf, int len, int flags );
static int			VCR_Hook_send( int s, const char* buf, int len, int flags );

void				VCR_GenericRecord( const char* pEventName, const void* pData, int len );
int					VCR_GenericPlayback( const char* pEventName, void* pOutData, int maxLen, bool bForceSameLen );
void				VCR_GenericValue( const char* pEventName, void* pData, int maxLen );

static void			VCR_Error( const char* pFormat, ... );
static bool VCR_Read(void* destination, int size);
static bool VCR_Write(const void* source, int size);

extern "C"
{

VCR_t g_VCR =
{
	VCR_Start,
	VCR_End,
	VCR_GetVCRTraceInterface,
	VCR_GetMode,
	VCR_SetEnabled,
	VCR_SyncToken,
	VCR_Hook_Sys_FloatTime,
	VCR_Hook_PeekMessage,
	VCR_Hook_RecordGameMsg,
	VCR_Hook_RecordEndGameMsg,
	VCR_Hook_PlaybackGameMsg,
	VCR_Hook_recvfrom,
	VCR_Hook_GetCursorPos,
	VCR_Hook_ScreenToClient,
	VCR_Hook_Cmd_Exec,
	VCR_Hook_GetCommandLine,
	VCR_Hook_RegOpenKeyEx,
	VCR_Hook_RegSetValueEx,
	VCR_Hook_RegQueryValueEx,
	VCR_Hook_RegCreateKeyEx,
	VCR_Hook_RegCloseKey,
	VCR_Hook_GetNumberOfConsoleInputEvents,
	VCR_Hook_ReadConsoleInput,
	VCR_Hook_LocalTime,
	VCR_Hook_GetKeyState,
	VCR_Hook_recv,
	VCR_Hook_send,
	VCR_GenericRecord,
	VCR_GenericPlayback,
	VCR_GenericValue,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL
};

VCR_t*			g_pVCR = &g_VCR;
int             g_LastReadEvent = VcrNoReadEvent;
int             g_OldVCRMode = VcrNoSavedMode;
bool			g_bTraceRead;
off_t           g_FileLen;
off_t           g_CurFilePos;
int				g_iCurEvent;
VCRMode			g_VCRMode;
FILE*			g_pVCRFile;
IVCRHelpers*	g_pHelpers;

}

static CVCRTrace g_VCRTrace;

static IVCRTrace* VCR_GetVCRTraceInterface()
{
	return &g_VCRTrace;
}

static VCRMode VCR_GetMode()
{
	return g_VCRMode;
}

static int VCR_Hook_PeekMessage( tagMSG*, void*, unsigned int, unsigned int, unsigned int )
{
	return -1;
}

static void VCR_Hook_ScreenToClient( void*, tagPOINT* )
{
}

static int VCR_Hook_RegOpenKeyEx( void*, const char*, unsigned int, unsigned int, void* )
{
	return 0;
}

static int VCR_Hook_RegSetValueEx( void*, const char*, unsigned int, unsigned int, const uint8*, unsigned int )
{
	return 0;
}

static int VCR_Hook_RegQueryValueEx( void*, const char*, unsigned int*, unsigned int*, uint8*, unsigned int* )
{
	return 0;
}

static int VCR_Hook_RegCreateKeyEx( void*, const char*, unsigned int, char*, unsigned int, unsigned int,
		void*, void*, unsigned int* )
{
	return 0;
}

static void VCR_Hook_RegCloseKey( void* )
{
}

int VCR_Hook_ReadConsoleInput( void*, void*, int, unsigned int* )
{
	return 0;
}

short VCR_Hook_GetKeyState( int )
{
	return 0;
}

static void VCR_Error(const char* format, ...) {
    const int savedError = errno;
    constexpr size_t ErrorMessageCapacity = 256;
    char message[ErrorMessageCapacity];
    va_list arguments;
    va_start(arguments, format);
    vsnprintf(message, sizeof(message), format, arguments);
    va_end(arguments);

    if (g_pHelpers != NULL) {
        g_pHelpers->ErrorMessage(message);
    } else {
        Error("%s", message);
    }
    g_pVCR->End();
    errno = savedError;
}

static bool VCR_Fail(const char* message, int error) {
    errno = error;
    VCR_Error("%s", message);
    return false;
}

static bool VCR_ValidateReadSize(int size) {
    if (size < 0 || g_CurFilePos < 0 || g_CurFilePos > g_FileLen
        || static_cast<off_t>(size) > g_FileLen - g_CurFilePos) {
        return VCR_Fail("VCR playback: invalid or truncated payload", EIO);
    }
    return true;
}

static bool VCR_Read(void* destination, int size) {
    if (size < 0 || (size > 0 && destination == NULL)) {
        return VCR_Fail("VCR playback: invalid read destination or size", EIO);
    }
    if (size == 0) {
        return true;
    }
    if (g_pVCRFile == NULL) {
        memset(destination, 0, static_cast<size_t>(size));
        return VCR_Fail("VCR playback: unexpected end of recording", EIO);
    }
    if (!VCR_ValidateReadSize(size)) {
        memset(destination, 0, static_cast<size_t>(size));
        return false;
    }

    const size_t requested = static_cast<size_t>(size);
    const size_t count = fread(destination, 1, requested, g_pVCRFile);
    const int readError = errno;
    g_CurFilePos += static_cast<off_t>(count);
    if (count != requested) {
        const int failure = ferror(g_pVCRFile) && readError != 0 ? readError : EIO;
        memset(destination, 0, requested);
        return VCR_Fail("VCR playback: short read", failure);
    }
    if (g_CurFilePos == g_FileLen) {
        g_pVCR->End();
    }
    return true;
}

static bool VCR_Write(const void* source, int size) {
    if (size < 0 || (size > 0 && source == NULL)) {
        return VCR_Fail("VCR recording: invalid source or size", EINVAL);
    }
    if (size == 0) {
        return true;
    }
    if (g_pVCRFile == NULL) {
        return false;
    }
    const int savedError = errno;
    const size_t requested = static_cast<size_t>(size);
    if (fwrite(source, 1, requested, g_pVCRFile) != requested || fflush(g_pVCRFile) != 0) {
        const int writeError = errno != 0 ? errno : EIO;
        return VCR_Fail("VCR recording: write failed", writeError);
    }
    errno = savedError;
    return true;
}

void VCR_Hook_LocalTime( tm* today )
{
	time_t ltime;

	time( &ltime );
	*today = *localtime( &ltime );
}

static void VCR_End()
{
	if ( g_pVCRFile != NULL )
	{
		fclose( g_pVCRFile );
		g_pVCRFile = NULL;
	}
	g_VCRMode = VCR_Disabled;
}

static void VCR_SetEnabled(int enabled) {
    if (enabled) {
        if (g_OldVCRMode == VcrNoSavedMode) {
            VCR_Error("VCR: no disabled mode to restore");
            return;
        }
        g_VCRMode = static_cast<VCRMode>(g_OldVCRMode);
        g_OldVCRMode = VcrNoSavedMode;
    } else {
        if (g_OldVCRMode != VcrNoSavedMode) {
            VCR_Error("VCR: mode is already disabled");
            return;
        }
        g_OldVCRMode = static_cast<int>(g_VCRMode);
        g_VCRMode = VCR_Disabled;
    }
}

static void VCR_Hook_GetCursorPos( tagPOINT* )
{
}

static int VCR_Start(const char* filename, bool record, IVCRHelpers* helpers) {
    constexpr uint32 FileVersion = 2;
    uint32 version = FileVersion;
    g_pHelpers = helpers;
    g_pVCR->End();
    g_OldVCRMode = VcrNoSavedMode;
    g_pVCRFile = fopen(filename, record ? "wb" : "rb");
    if (g_pVCRFile == NULL) {
        return 0;
    }
    if (record) {
        if (!VCR_Write(&version, sizeof(version))) {
            return 0;
        }
        g_VCRMode = VCR_Record;
        return 1;
    }

    if (fseeko(g_pVCRFile, 0, SEEK_END) != 0) {
        VCR_Fail("VCR playback: cannot seek recording", errno);
        return 0;
    }
    g_FileLen = ftello(g_pVCRFile);
    if (g_FileLen < 0 || fseeko(g_pVCRFile, 0, SEEK_SET) != 0) {
        VCR_Fail("VCR playback: cannot determine recording position", errno);
        return 0;
    }
    g_CurFilePos = 0;
    if (!VCR_Read(&version, sizeof(version))) {
        return 0;
    }
    if (version != FileVersion) {
        VCR_Fail("VCR playback: unsupported recording version", EIO);
        return 0;
    }
    // Reading the complete header also ends a valid empty recording.
    if (g_pVCRFile != NULL) {
        g_VCRMode = VCR_Playback;
    }
    return 1;
}

char* GetCommandLine();

static bool VCR_SyncEvent(VCREvent type) {
    signed char eventByte = static_cast<signed char>(type);
    if (g_VCRMode == VCR_Record) {
        g_bTraceRead = false;
        return VCR_Write(&eventByte, sizeof(eventByte));
    }
    g_bTraceRead = true;
    if (!VCR_Read(&eventByte, sizeof(eventByte))) {
        return false;
    }
    if (eventByte < 0 || eventByte >= static_cast<int>(VCREvent_NUMEVENTS)) {
        return VCR_Fail("VCR playback: invalid event type", EIO);
    }
    g_LastReadEvent = eventByte;
    if (g_LastReadEvent != static_cast<int>(type)) {
        return VCR_Fail("VCR playback: event type does not match", EIO);
    }
    if (g_pVCRFile == NULL) {
        return VCR_Fail("VCR playback: event payload is missing", EIO);
    }
    return true;
}

void VCR_GenericRecord(const char* eventName, const void* data, int length) {
    constexpr size_t MaximumEncodedNameLength = 254;
    constexpr unsigned char UnnamedEvent = 0xff;
    if (g_VCRMode != VCR_Record) {
        Error("VCR_GenericRecord: not recording a VCR file");
        return;
    }
    if (length < 0 || (length > 0 && data == NULL)) {
        VCR_Fail("VCR recording: invalid generic payload", EINVAL);
        return;
    }
    const size_t nameLength = eventName != NULL ? strlen(eventName) + 1 : 0;
    if (nameLength > MaximumEncodedNameLength) {
        VCR_Fail("VCR recording: generic event name is too long", EINVAL);
        return;
    }
    const unsigned char encodedNameLength = eventName != NULL
        ? static_cast<unsigned char>(nameLength) : UnnamedEvent;
    ++g_iCurEvent;
    if (!VCR_SyncEvent(VCREvent_Generic) || !VCR_Write(&encodedNameLength, sizeof(encodedNameLength))) {
        return;
    }
    if (eventName != NULL && !VCR_Write(eventName, static_cast<int>(nameLength))) {
        return;
    }
    if (!VCR_Write(&length, sizeof(length))) {
        return;
    }
    VCR_Write(data, length);
}

int VCR_GenericPlayback(const char* eventName, void* output, int maximumLength, bool forceSameLength) {
    constexpr unsigned char UnnamedEvent = 0xff;
    if (g_VCRMode != VCR_Playback) {
        Error("VCR_GenericPlayback: not playing back a VCR file");
        return 0;
    }
    ++g_iCurEvent;
    if (!VCR_SyncEvent(VCREvent_Generic)) {
        return 0;
    }
    unsigned char nameLength = 0;
    if (!VCR_Read(&nameLength, sizeof(nameLength))) {
        return 0;
    }
    if (nameLength != UnnamedEvent) {
        char recordedName[UnnamedEvent] = {};
        if (nameLength == 0) {
            VCR_Fail("VCR playback: invalid generic event name length", EIO);
            return 0;
        }
        if (!VCR_Read(recordedName, nameLength)) {
            return 0;
        }
        if (recordedName[nameLength - 1] != 0 || eventName == NULL || strcmp(eventName, recordedName) != 0) {
            VCR_Fail("VCR playback: generic event name does not match", EIO);
            return 0;
        }
    }
    int dataLength = 0;
    if (!VCR_Read(&dataLength, sizeof(dataLength))) {
        return 0;
    }
    if (dataLength < 0 || maximumLength < 0 || dataLength > maximumLength
        || (forceSameLength && dataLength != maximumLength)) {
        VCR_Fail("VCR playback: invalid generic data length", EIO);
        return 0;
    }
    if (!VCR_Read(output, dataLength)) {
        return 0;
    }
    return dataLength;
}

void VCR_GenericValue( const char* pEventName, void* pData, int maxLen )
{
	if ( g_VCRMode == VCR_Record )
	{
		VCR_GenericRecord( pEventName, pData, maxLen );
	}
	else if ( g_VCRMode == VCR_Playback )
	{
		VCR_GenericPlayback( pEventName, pData, maxLen, true );
	}
}

void VCR_Hook_RecordEndGameMsg() {
    if (g_VCRMode == VCR_Record) {
        const unsigned char event = static_cast<unsigned char>(VCREvent_GameMsg);
        constexpr unsigned char EndGameMessages = 0;
        ++g_iCurEvent;
        g_bTraceRead = false;
        if (VCR_Write(&event, sizeof(event))) {
            VCR_Write(&EndGameMessages, sizeof(EndGameMessages));
        }
    }
}

static double VCR_Hook_Sys_FloatTime(double time) {
    if (g_VCRMode != VCR_Disabled) {
        ++g_iCurEvent;
        if (!VCR_SyncEvent(VCREvent_Sys_FloatTime)) {
            return time;
        }
        if (g_VCRMode == VCR_Record) {
            VCR_Write(&time, sizeof(time));
        } else if (g_VCRMode == VCR_Playback && !VCR_Read(&time, sizeof(time))) {
            return 0.0;
        }
    }
    return time;
}

static char* VCR_Hook_GetCommandLine() {
    char* line = GetCommandLine();
    if (g_VCRMode == VCR_Disabled) {
        return line;
    }
    ++g_iCurEvent;
    if (!VCR_SyncEvent(VCREvent_CmdLine)) {
        return NULL;
    }
    if (g_VCRMode == VCR_Playback) {
        int length = 0;
        if (!VCR_Read(&length, sizeof(length))) {
            return NULL;
        }
        if (length <= 0) {
            VCR_Fail("VCR playback: invalid command line length", EIO);
            return NULL;
        }
        if (!VCR_ValidateReadSize(length)) {
            return NULL;
        }
        char* recorded = new (std::nothrow) char[length];
        if (recorded == NULL) {
            VCR_Fail("VCR playback: cannot allocate command line", ENOMEM);
            return NULL;
        }
        if (!VCR_Read(recorded, length)) {
            delete[] recorded;
            return NULL;
        }
        if (recorded[length - 1] != 0) {
            delete[] recorded;
            VCR_Fail("VCR playback: unterminated command line", EIO);
            return NULL;
        }
        return recorded;
    }
    const size_t textLength = strlen(line);
    if (textLength >= INT_MAX) {
        VCR_Fail("VCR recording: command line is too long", EOVERFLOW);
        return NULL;
    }
    const int length = static_cast<int>(textLength) + 1;
    if (!VCR_Write(&length, sizeof(length)) || !VCR_Write(line, length)) {
        return NULL;
    }
    return line;
}

void VCR_Hook_RecordGameMsg(unsigned int message, unsigned int wParam, int lParam) {
    if (g_VCRMode == VCR_Record) {
        const unsigned char event = static_cast<unsigned char>(VCREvent_GameMsg);
        constexpr unsigned char HasGameMessage = 1;
        ++g_iCurEvent;
        g_bTraceRead = false;
        if (!VCR_Write(&event, sizeof(event)) || !VCR_Write(&HasGameMessage, sizeof(HasGameMessage))
            || !VCR_Write(&message, sizeof(message)) || !VCR_Write(&wParam, sizeof(wParam))) {
            return;
        }
        VCR_Write(&lParam, sizeof(lParam));
    }
}

int VCR_Hook_GetNumberOfConsoleInputEvents(void*, unsigned int* numberOfEvents) {
    if (g_VCRMode != VCR_Disabled) {
        ++g_iCurEvent;
        if (!VCR_SyncEvent(VCREvent_GetNumberOfConsoleInputEvents)) {
            return 0;
        }
        if (g_VCRMode == VCR_Playback) {
            signed char result = 0;
            if (!VCR_Read(&result, sizeof(result)) || !VCR_Read(numberOfEvents, sizeof(*numberOfEvents))) {
                return 0;
            }
            return result;
        }
        constexpr signed char StubSuccess = 1;
        if (g_VCRMode == VCR_Record && (!VCR_Write(&StubSuccess, sizeof(StubSuccess))
            || !VCR_Write(numberOfEvents, sizeof(*numberOfEvents)))) {
            return 0;
        }
    }
    return 1;
}

static void VCR_Hook_Cmd_Exec(char** command) {
    if (g_VCRMode == VCR_Disabled) {
        return;
    }
    constexpr int NullCommandLength = -1;
    ++g_iCurEvent;
    if (!VCR_SyncEvent(VCREvent_Cmd_Exec)) {
        *command = NULL;
        return;
    }
    if (g_VCRMode == VCR_Playback) {
        int length = 0;
        if (!VCR_Read(&length, sizeof(length))) {
            *command = NULL;
            return;
        }
        if (length == NullCommandLength) {
            *command = NULL;
            return;
        }
        if (length <= 0) {
            *command = NULL;
            VCR_Fail("VCR playback: invalid command text length", EIO);
            return;
        }
        if (!VCR_ValidateReadSize(length)) {
            *command = NULL;
            return;
        }
        char* recorded = static_cast<char*>(malloc(static_cast<size_t>(length)));
        if (recorded == NULL) {
            *command = NULL;
            VCR_Fail("VCR playback: cannot allocate command text", ENOMEM);
            return;
        }
        if (!VCR_Read(recorded, length)) {
            free(recorded);
            *command = NULL;
            return;
        }
        if (recorded[length - 1] != 0) {
            free(recorded);
            *command = NULL;
            VCR_Fail("VCR playback: unterminated command text", EIO);
            return;
        }
        *command = recorded;
        return;
    }
    int length = NullCommandLength;
    if (*command != NULL) {
        const size_t textLength = strlen(*command);
        if (textLength >= INT_MAX) {
            VCR_Fail("VCR recording: command text is too long", EOVERFLOW);
            return;
        }
        length = static_cast<int>(textLength) + 1;
    }
    if (!VCR_Write(&length, sizeof(length))) {
        return;
    }
    if (length > 0) {
        VCR_Write(*command, length);
    }
}

static void VCR_SyncToken(const char* token) {
    if (g_VCRMode == VCR_Disabled) {
        return;
    }
    ++g_iCurEvent;
    if (!VCR_SyncEvent(VCREvent_SyncToken)) {
        return;
    }
    const unsigned char expectedLength = static_cast<unsigned char>(strlen(token));
    if (g_VCRMode == VCR_Record) {
        if (!VCR_Write(&expectedLength, sizeof(expectedLength))) {
            return;
        }
        VCR_Write(token, expectedLength);
    } else if (g_VCRMode == VCR_Playback) {
        unsigned char length = 0;
        constexpr size_t TokenBufferCapacity = UCHAR_MAX + 1;
        char recorded[TokenBufferCapacity];
        if (!VCR_Read(&length, sizeof(length)) || !VCR_Read(recorded, length)) {
            return;
        }
        if (length != expectedLength || memcmp(token, recorded, length) != 0) {
            VCR_Fail("VCR playback: sync token does not match", EIO);
            return;
        }
    }
}

static int VCR_NarrowSocketResult(ssize_t result, int& error) {
    if (result < VcrSocketFailure || result > INT_MAX) {
        error = EOVERFLOW;
        return VcrSocketFailure;
    }
    return static_cast<int>(result);
}

static int VCR_Hook_send(int socket, const char* buffer, int length, int flags) {
    if (g_VCRMode != VCR_Disabled) {
        ++g_iCurEvent;
        if (!VCR_SyncEvent(VCREvent_send)) {
            return VcrSocketFailure;
        }
    }
    if (g_VCRMode == VCR_Playback) {
        int result = 0;
        if (!VCR_Read(&result, sizeof(result))) {
            return VcrSocketFailure;
        }
        if (result < VcrSocketFailure || result > length) {
            VCR_Fail("VCR playback: invalid send result", EIO);
            return VcrSocketFailure;
        }
        if (result == VcrSocketFailure) {
            int error = 0;
            if (!VCR_Read(&error, sizeof(error))) {
                return VcrSocketFailure;
            }
            errno = error;
        }
        return result;
    }
    const ssize_t nativeResult = send(socket, buffer, static_cast<size_t>(length), flags);
    int socketError = errno;
    const int result = VCR_NarrowSocketResult(nativeResult, socketError);
    errno = socketError;
    if (g_VCRMode == VCR_Record) {
        if (!VCR_Write(&result, sizeof(result))
            || (result == VcrSocketFailure && !VCR_Write(&socketError, sizeof(socketError)))) {
            return VcrSocketFailure;
        }
    }
    errno = socketError;
    return result;
}

static int VCR_Hook_recv(int socket, char* buffer, int length, int flags) {
    if (g_VCRMode != VCR_Disabled) {
        ++g_iCurEvent;
        if (!VCR_SyncEvent(VCREvent_recv)) {
            return VcrSocketFailure;
        }
    }
    if (g_VCRMode == VCR_Playback) {
        int result = 0;
        if (!VCR_Read(&result, sizeof(result))) {
            return VcrSocketFailure;
        }
        if (result < VcrSocketFailure || result > length) {
            VCR_Fail("VCR playback: invalid receive result", EIO);
            return VcrSocketFailure;
        }
        if (result == VcrSocketFailure) {
            int error = 0;
            if (!VCR_Read(&error, sizeof(error))) {
                return VcrSocketFailure;
            }
            errno = error;
        } else if (!VCR_Read(buffer, result)) {
            return VcrSocketFailure;
        }
        return result;
    }
    const ssize_t nativeResult = recv(socket, buffer, static_cast<size_t>(length), flags);
    int socketError = errno;
    const int result = VCR_NarrowSocketResult(nativeResult, socketError);
    errno = socketError;
    if (g_VCRMode == VCR_Record) {
        if (result > length) {
            VCR_Fail("VCR recording: receive result exceeds supplied buffer", EOVERFLOW);
            return VcrSocketFailure;
        }
        if (!VCR_Write(&result, sizeof(result))) {
            return VcrSocketFailure;
        }
        if (result == VcrSocketFailure) {
            if (!VCR_Write(&socketError, sizeof(socketError))) {
                return VcrSocketFailure;
            }
        } else if (!VCR_Write(buffer, result)) {
            return VcrSocketFailure;
        }
    }
    errno = socketError;
    return result;
}

static int VCR_Hook_recvfrom(int socket, char* buffer, int length, int flags, sockaddr* from, int* fromLength) {
    if (g_VCRMode != VCR_Disabled) {
        ++g_iCurEvent;
        if (!VCR_SyncEvent(VCREvent_recvfrom)) {
            return VcrSocketFailure;
        }
    }
    if (g_VCRMode == VCR_Playback) {
        int result = 0;
        if (!VCR_Read(&result, sizeof(result))) {
            return VcrSocketFailure;
        }
        if (result < VcrSocketFailure || result > length) {
            VCR_Fail("VCR playback: invalid receive-from result", EIO);
            return VcrSocketFailure;
        }
        if (result == VcrSocketFailure) {
            int error = 0;
            if (!VCR_Read(&error, sizeof(error))) {
                return VcrSocketFailure;
            }
            errno = error;
            return result;
        }
        unsigned char hasAddress = 0;
        if (!VCR_Read(buffer, result) || !VCR_Read(&hasAddress, sizeof(hasAddress))) {
            return VcrSocketFailure;
        }
        if (hasAddress > 1 || (hasAddress != 0 && (from == NULL || fromLength == NULL || *fromLength < 0))) {
            VCR_Fail("VCR playback: invalid receive-from address", EIO);
            return VcrSocketFailure;
        }
        if (hasAddress != 0 && !VCR_Read(from, *fromLength)) {
            return VcrSocketFailure;
        }
        return result;
    }
    const int addressCapacity = fromLength != NULL ? *fromLength : 0;
    socklen_t nativeLength = static_cast<socklen_t>(addressCapacity);
    const ssize_t nativeResult = recvfrom(socket, buffer, static_cast<size_t>(length), flags,
        from, fromLength != NULL ? &nativeLength : NULL);
    int socketError = errno;
    const int result = VCR_NarrowSocketResult(nativeResult, socketError);
    if (result >= 0 && from != NULL && fromLength != NULL) {
        if (static_cast<unsigned long long>(nativeLength) > INT_MAX) {
            if (g_VCRMode == VCR_Record) {
                VCR_Fail("VCR recording: native address length is unrepresentable", EOVERFLOW);
            } else {
                errno = EOVERFLOW;
            }
            return VcrSocketFailure;
        }
        *fromLength = static_cast<int>(nativeLength);
    }
    errno = socketError;
    if (g_VCRMode != VCR_Record) {
        return result;
    }
    if (result > length || (result >= 0 && from != NULL
        && (fromLength == NULL || addressCapacity < 0 || *fromLength > addressCapacity))) {
        VCR_Fail("VCR recording: receive-from result exceeds supplied buffers", EOVERFLOW);
        return VcrSocketFailure;
    }
    if (!VCR_Write(&result, sizeof(result))) {
        return VcrSocketFailure;
    }
    if (result == VcrSocketFailure) {
        if (!VCR_Write(&socketError, sizeof(socketError))) {
            return VcrSocketFailure;
        }
    } else {
        const unsigned char hasAddress = from != NULL ? 1 : 0;
        if (!VCR_Write(buffer, result) || !VCR_Write(&hasAddress, sizeof(hasAddress))
            || (hasAddress != 0 && !VCR_Write(from, *fromLength))) {
            return VcrSocketFailure;
        }
    }
    errno = socketError;
    return result;
}

static char linuxCmdline[519];

bool VCR_Hook_PlaybackGameMsg(unsigned int* message, unsigned int* wParam, int* lParam) {
    if (g_VCRMode != VCR_Playback) {
        return false;
    }
    ++g_iCurEvent;
    const VCREvent currentEvent = g_VCRTrace.ReadEvent();
    if (currentEvent == VCREvent_NUMEVENTS) {
        return false;
    }
    if (currentEvent != VCREvent_GameMsg) {
        VCR_Fail("VCR playback: expected game message event", EIO);
        return false;
    }
    unsigned char hasMessage = 0;
    if (!VCR_Read(&hasMessage, sizeof(hasMessage))) {
        return false;
    }
    if (hasMessage > 1) {
        VCR_Fail("VCR playback: invalid game message marker", EIO);
        return false;
    }
    if (hasMessage == 0) {
        return false;
    }
    unsigned int recordedMessage = 0;
    unsigned int recordedWParam = 0;
    int recordedLParam = 0;
    if (!VCR_Read(&recordedMessage, sizeof(recordedMessage))
        || !VCR_Read(&recordedWParam, sizeof(recordedWParam))
        || !VCR_Read(&recordedLParam, sizeof(recordedLParam))) {
        return false;
    }
    if (message == NULL || wParam == NULL || lParam == NULL) {
        VCR_Fail("VCR playback: invalid game message destination", EIO);
        return false;
    }
    *message = recordedMessage;
    *wParam = recordedWParam;
    *lParam = recordedLParam;
    return true;
}

void OutputDebugStringFormat(const char* format, ...) {
    constexpr size_t TraceBufferCapacity = 4096;
    char message[TraceBufferCapacity] = {};
    va_list arguments;
    va_start(arguments, format);
    vsnprintf(message, sizeof(message) - 1, format, arguments);
    va_end(arguments);
    const int length = static_cast<int>(strlen(message));
    if (g_bTraceRead) {
        unsigned int recordedLength = 0;
        char recorded[TraceBufferCapacity];
        if (!VCR_Read(&recordedLength, sizeof(recordedLength))) {
            return;
        }
        if (recordedLength >= sizeof(recorded)) {
            VCR_Fail("VCR playback: debug trace text is too long", EIO);
            return;
        }
        if (!VCR_Read(recorded, static_cast<int>(recordedLength))) {
            return;
        }
        recorded[recordedLength] = 0;
        if (recordedLength != static_cast<unsigned int>(length) || memcmp(message, recorded, length) != 0) {
            VCR_Fail("VCR playback: debug trace text does not match", EIO);
            return;
        }
        fwrite("FILE: ", 1, sizeof("FILE: ") - 1, stderr);
        fputs(recorded, stderr);
    } else if (VCR_Write(&length, sizeof(length))) {
        VCR_Write(message, length);
    }
}

void BuildCmdLine( int argc, char** argv )
{
	int i;
	int len;

	if ( argc <= 0 )
	{
		linuxCmdline[0] = 0;
	}
	else
	{
		len = 0;
		for ( i = 0; i < argc; i++ )
		{
			len += (int)strlen( argv[i] );
		}

		if ( len > 512 )
		{
			printf( "command line too long, %i max\n", 512 );
			exit( -1 );
		}

		linuxCmdline[0] = 0;
		for ( i = 0; i < argc; i++ )
		{
			strcat( linuxCmdline, argv[i] );
			if ( i + 1 < argc )
			{
				strcat( linuxCmdline, " " );
			}
		}
	}

	strcat( linuxCmdline, " -steam" );
}

char* GetCommandLine()
{
	return linuxCmdline;
}

VCREvent CVCRTrace::ReadEvent() {
    g_bTraceRead = true;
    if (g_pVCRFile == NULL) {
        g_LastReadEvent = VCREvent_Sys_FloatTime;
        return static_cast<VCREvent>(g_LastReadEvent);
    }
    unsigned char event = 0;
    if (!VCR_Read(&event, sizeof(event))) {
        g_LastReadEvent = VCREvent_NUMEVENTS;
        return static_cast<VCREvent>(g_LastReadEvent);
    }
    if (event >= VCREvent_NUMEVENTS || g_pVCRFile == NULL) {
        VCR_Fail("VCR playback: invalid or truncated trace event", EIO);
        g_LastReadEvent = VCREvent_NUMEVENTS;
        return static_cast<VCREvent>(g_LastReadEvent);
    }
    g_LastReadEvent = event;
    return static_cast<VCREvent>(g_LastReadEvent);
}

void CVCRTrace::Read(void* destination, int size) {
    if (g_pVCRFile != NULL) {
        VCR_Read(destination, size);
    } else if (size > 0 && destination != NULL) {
        // Preserve the trace interface's disabled-mode zero-fill behavior.
        memset(destination, 0, static_cast<size_t>(size));
    }
}

#endif
