#include "conproc.h"

#if defined( _WIN32 )

#include "inherited_handle.h"

void Sys_Printf(char* format, ...);
#include "../../common/icommandline.h"

#include <ctype.h>
#include <process.h>
#include <cstdlib>
#include <windows.h>

static HANDLE	heventDone;
static HANDLE	hfileBuffer;
static HANDLE	heventChildSend;
static HANDLE	heventParentSend;
static HANDLE	hStdout;
static HANDLE	hStdin;
static HANDLE workerThread;

/*
==============
SetConsoleCXCY

==============
*/
BOOL SetConsoleCXCY( HANDLE output, int cx, int cy )
{
	CONSOLE_SCREEN_BUFFER_INFO info;
	COORD coordMax = GetLargestConsoleWindowSize( output );

	if ( cy > coordMax.Y )
		cy = coordMax.Y;
	if ( cx > coordMax.X )
		cx = coordMax.X;

	if ( !GetConsoleScreenBufferInfo( output, &info ) )
		return FALSE;

// height
	info.srWindow.Left = 0;
	info.srWindow.Right = (SHORT)( info.dwSize.X - 1 );
	info.srWindow.Top = 0;
	info.srWindow.Bottom = (SHORT)( cy - 1 );

	if ( cy < info.dwSize.Y )
	{
		if ( !SetConsoleWindowInfo( output, TRUE, &info.srWindow ) )
			return FALSE;

		info.dwSize.Y = (SHORT)cy;
		if ( !SetConsoleScreenBufferSize( output, info.dwSize ) )
			return FALSE;
	}
	else if ( cy > info.dwSize.Y )
	{
		info.dwSize.Y = (SHORT)cy;
		if ( !SetConsoleScreenBufferSize( output, info.dwSize ) )
			return FALSE;
		if ( !SetConsoleWindowInfo( output, TRUE, &info.srWindow ) )
			return FALSE;
	}

	if ( !GetConsoleScreenBufferInfo( output, &info ) )
		return FALSE;

// width
	info.srWindow.Left = 0;
	info.srWindow.Right = (SHORT)( cx - 1 );
	info.srWindow.Top = 0;
	info.srWindow.Bottom = (SHORT)( info.dwSize.Y - 1 );

	if ( cx < info.dwSize.X )
	{
		if ( !SetConsoleWindowInfo( output, TRUE, &info.srWindow ) )
			return FALSE;

		info.dwSize.X = (SHORT)cx;
		if ( !SetConsoleScreenBufferSize( output, info.dwSize ) )
			return FALSE;
	}
	else if ( cx > info.dwSize.X )
	{
		info.dwSize.X = (SHORT)cx;
		if ( !SetConsoleScreenBufferSize( output, info.dwSize ) )
			return FALSE;
		if ( !SetConsoleWindowInfo( output, TRUE, &info.srWindow ) )
			return FALSE;
	}

	return TRUE;
}

void* GetMappedBuffer( HANDLE file )
{
	return MapViewOfFile( file, FILE_MAP_READ | FILE_MAP_WRITE, 0, 0, 0 );
}

void ReleaseMappedBuffer( void* buffer )
{
	UnmapViewOfFile( buffer );
}

/*
==============
GetScreenBufferLines

==============
*/
BOOL GetScreenBufferLines( int* lines )
{
	CONSOLE_SCREEN_BUFFER_INFO info;
	BOOL success = GetConsoleScreenBufferInfo( hStdout, &info );
	if ( success )
		*lines = info.dwSize.Y;
	return success;
}

/*
==============
SetScreenBufferLines

==============
*/
BOOL SetScreenBufferLines( int lines )
{
	return SetConsoleCXCY( hStdout, 80, lines );
}

BOOL ReadText( char* text, int beginLine, int endLine )
{
	COORD coord;
	coord.X = 0;
	coord.Y = (SHORT)beginLine;

	DWORD read;
	BOOL success = ReadConsoleOutputCharacterA( hStdout, text, 80 * ( endLine - beginLine + 1 ), coord, &read );
	if ( success )
		text[read] = '\0';
	return success;
}

/*
==============
CharToCode

==============
*/
int CharToCode( char c )
{
	char upper = (char)toupper( c );
	if ( c == '\r' )
		return 28;
	if ( isalpha( c ) )
		return 30 + upper - 65;
	if ( isdigit( c ) )
		return 1 + upper - 47;
	return c;
}

BOOL WriteText( const char* text )
{
	char* sz = (char*)text;
	INPUT_RECORD rec;
	DWORD written;

	while ( *sz )
	{
		// 13 is the code for a carriage return (\n) instead of 10.
		if ( *sz == '\n' )
			*sz = '\r';

		char upper = (char)toupper( *sz );
		rec.EventType = KEY_EVENT;
		rec.Event.KeyEvent.bKeyDown = TRUE;
		rec.Event.KeyEvent.wRepeatCount = 1;
		rec.Event.KeyEvent.wVirtualKeyCode = (WORD)upper;
		rec.Event.KeyEvent.wVirtualScanCode = (WORD)CharToCode( *sz );
		rec.Event.KeyEvent.uChar.AsciiChar = *sz;
		rec.Event.KeyEvent.uChar.UnicodeChar = (WCHAR)*sz;
		rec.Event.KeyEvent.dwControlKeyState = isupper( *sz ) ? 0x80 : 0;

		WriteConsoleInputA( hStdin, &rec, 1, &written );
		rec.Event.KeyEvent.bKeyDown = FALSE;
		WriteConsoleInputA( hStdin, &rec, 1, &written );
		++sz;
	}

	return TRUE;
}

unsigned __stdcall RequestProc( void* )
{
	HANDLE waitEvents[2];
	waitEvents[0] = heventDone;
	waitEvents[1] = heventParentSend;

	for ( ;; )
	{
		DWORD event = WaitForMultipleObjects( 2, waitEvents, FALSE, INFINITE );
		// heventDone fired, so we're exiting.
		if (event != WAIT_OBJECT_0 + 1) {
            break;
        }

		int* buffer = (int*)GetMappedBuffer( hfileBuffer );
		if ( !buffer )
		{
			Sys_Printf( "Request Proc:  Invalid -HFILE handle\n" );
			break;
		}

		switch ( buffer[0] )
		{
		case CCOM_WRITE_TEXT:
			buffer[0] = WriteText( (const char*)( buffer + 1 ) );
			break;

		case CCOM_GET_TEXT:
		{
			int beginLine = buffer[1];
			int endLine = buffer[2];
			buffer[0] = ReadText( (char*)( buffer + 1 ), beginLine, endLine );
			break;
		}

		case CCOM_GET_SCR_LINES:
			buffer[0] = GetScreenBufferLines( &buffer[1] );
			break;

		case CCOM_SET_SCR_LINES:
			buffer[0] = SetScreenBufferLines( buffer[1] );
			break;
		}

		ReleaseMappedBuffer( buffer );
		SetEvent( heventChildSend );
	}

	_endthreadex( 0 );
	return 0;
}

/*
==============
DeinitConProc

==============
*/
void DeinitConProc() {
    if (heventDone) {
        SetEvent(heventDone);
    }
    if (workerThread) {
        // The worker borrows all remaining globals, including caller-owned
        // inherited events/mapping and the console. Quiesce before release.
        if (WaitForSingleObject(workerThread, INFINITE) != WAIT_OBJECT_0) {
            std::abort();
        }
        CloseHandle(workerThread);
        workerThread = nullptr;
    }
    if (heventDone) {
        CloseHandle(heventDone);
        heventDone = nullptr;
    }
    hfileBuffer = nullptr;
    heventParentSend = nullptr;
    heventChildSend = nullptr;
    hStdout = nullptr;
    hStdin = nullptr;
}

void InitConProc() {
    DeinitConProc();
    HANDLE inherited[3]{};
    constexpr const char* Names[] = {"-HFILE", "-HPARENT", "-HCHILD"};
    for (std::size_t index = 0; index < 3; ++index) {
        char* parameter = nullptr;
        std::uintptr_t value = 0;
        if (!cmdline->CheckParm(Names[index], &parameter)) {
            return;
        }
        if (!cse::dedicated::ParseInheritedHandle(parameter, value)) {
            Sys_Printf((char*)"Invalid inherited console handle: %s\n", Names[index]);
            return;
        }
        inherited[index] = reinterpret_cast<HANDLE>(value);
    }
    hfileBuffer = inherited[0];
    heventParentSend = inherited[1];
    heventChildSend = inherited[2];
    hStdout = GetStdHandle(STD_OUTPUT_HANDLE);
    hStdin = GetStdHandle(STD_INPUT_HANDLE);
    heventDone = CreateEventA(nullptr, TRUE, FALSE, nullptr);
    if (!heventDone) {
        DeinitConProc();
        return;
    }
    constexpr int ConsoleWidth = 80;
    constexpr int DefaultConsoleHeight = 50;
    int height = DefaultConsoleHeight;
    char* parameter = nullptr;
    if (cmdline->CheckParm("-conheight", &parameter) && parameter) {
        height = atoi(parameter);
    }
    SetConsoleCXCY(hStdout, ConsoleWidth, height);
    unsigned int threadId = 0;
    workerThread = reinterpret_cast<HANDLE>(_beginthreadex(nullptr, 0, RequestProc, nullptr, 0, &threadId));
    if (!workerThread) {
        DeinitConProc();
        Sys_Printf((char*)"InitConProc: Could not create control worker\n");
    }
}

#endif
