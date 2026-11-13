//========= Copyright © 1996-2001, Valve LLC, All rights reserved. ============
//
// Purpose: Thread management routines
//
// $NoKeywords: $
//=============================================================================

#include "tier0/platform.h"

#if defined( _WIN32 )
#include <windows.h>

extern "C" TIER0_API unsigned long Plat_PrimaryThreadID = 0;
static thread_local unsigned long Plat_CurrentThreadID;

extern "C" TIER0_API void Plat_SetThreadName( unsigned int threadId, const char* name )
{
	const DWORD MS_VC_EXCEPTION = 0x406D1388;
	const ULONG_PTR THREADNAME_INFO_TYPE = 0x1000;
	const ULONG_PTR arguments[] = { THREADNAME_INFO_TYPE, (ULONG_PTR)name, threadId, 0 };
	__try
	{
		RaiseException( MS_VC_EXCEPTION, 0, sizeof( arguments ) / sizeof( arguments[0] ), arguments );
	}
	__except ( EXCEPTION_EXECUTE_HANDLER )
	{
	}
}

extern "C" TIER0_API unsigned int Plat_RegisterThread( const char* )
{
	// Initialize the current thread ID.
	Plat_CurrentThreadID = GetCurrentThreadId();
	//Assert( pName );
	//if( pName )
	//{
	//	Plat_SetThreadName( -1, pName );
	//}

	return Plat_CurrentThreadID;
}

extern "C" TIER0_API unsigned int Plat_RegisterPrimaryThread()
{
	Plat_PrimaryThreadID = GetCurrentThreadId();
	Plat_CurrentThreadID = Plat_PrimaryThreadID;
	return Plat_PrimaryThreadID;
}

extern "C" TIER0_API unsigned int Plat_GetCurrentThreadID()
{
	return Plat_CurrentThreadID;
}

#elif defined(__APPLE__)
#include <limits>
#include <mach/port.h>
#include <pthread.h>
#include <type_traits>

// Preserve the existing exported unsigned-long storage; logical IDs remain 32-bit.
unsigned long Plat_PrimaryThreadID = 0;

namespace {
constexpr unsigned int kUnregisteredThreadId = 0;
constexpr int kLogicalThreadIdBits = 32;
thread_local unsigned int Plat_CurrentThreadID = kUnregisteredThreadId;

unsigned int NativeCurrentThreadId() {
    static_assert(std::numeric_limits<unsigned int>::digits == kLogicalThreadIdBits,
        "Logical thread IDs require 32-bit storage");
    static_assert(std::is_same<mach_port_t, unsigned int>::value,
        "The native Mach port identity must already have the logical ID type");
    // This is the borrowed task-local thread-port name, not a pthread pointer or an acquired send right.
    const mach_port_t identity = pthread_mach_thread_np(pthread_self());
    return MACH_PORT_VALID(identity) ? identity : kUnregisteredThreadId;
}
}

void Plat_SetThreadName(unsigned int, const char*) {
}

unsigned int Plat_RegisterThread(const char*) {
    Plat_CurrentThreadID = NativeCurrentThreadId();
    return Plat_CurrentThreadID;
}

unsigned int Plat_RegisterPrimaryThread() {
    const unsigned int identity = Plat_RegisterThread(nullptr);
    if (identity != kUnregisteredThreadId) {
        Plat_PrimaryThreadID = identity;
    }
    return identity;
}

unsigned int Plat_GetCurrentThreadID() {
    return Plat_CurrentThreadID;
}

#else
#include <pthread.h>

unsigned long Plat_PrimaryThreadID;
unsigned long Plat_CurrentThreadID;

void Plat_SetThreadName( unsigned int, const char* )
{
}

unsigned int Plat_RegisterThread( const char* )
{
	return pthread_self();
}

pthread_t Plat_RegisterPrimaryThread()
{
	pthread_t result;

	result = pthread_self();
	Plat_PrimaryThreadID = result;
	return result;
}

unsigned int Plat_GetCurrentThreadID()
{
	return Plat_CurrentThreadID;
}

#endif
