#include "tier0/threadtools.h"
#include "tier0/dbg.h"

#define DBG_SOURCE_FILE	DBG_BUILDSLAVE_PATH( "tier0\\testthread.cpp" )

#if defined( _WIN32 )

typedef void ( __stdcall *TestFunc )( void* );

class CTestHarness
{
public:
	CTestHarness();
	~CTestHarness();

	void RunTest( TestFunc func, void* arg );
	void RunFrame();
	void Yield();
	void TerminateTestThread();

	static DWORD WINAPI TestThreadProxy( void* arg );

	HANDLE		m_hThreadTestDriver;
	HANDLE		m_hRunTestThread;
	HANDLE		m_hRunMainThread;

	bool		m_bTestThreadRunning;
	bool		m_bStopTestThread;
	bool		m_bLetTestThreadRun;
	bool		m_bTestFailed;

	DWORD		m_ulTestThreadID;
	DWORD		m_ulMainThreadID;
	void*		m_pvTestThreadArg;
	TestFunc	m_pTestFunc;
};

CTestHarness g_TestHarness;

CTestHarness::CTestHarness()
{
	m_ulMainThreadID = GetCurrentThreadId();
	m_ulTestThreadID = 0;
	m_hRunTestThread = NULL;
	m_hRunMainThread = NULL;
	m_hThreadTestDriver = NULL;
}

CTestHarness::~CTestHarness()
{
	if ( m_bTestThreadRunning )
	{
		const DWORD TEST_SHUTDOWN_TIMEOUT = 100;
		m_bStopTestThread = true;
		SetEvent( m_hRunTestThread );
		WaitForSingleObject( m_hThreadTestDriver, TEST_SHUTDOWN_TIMEOUT );
		CloseHandle( m_hThreadTestDriver );
		CloseHandle( m_hRunTestThread );
		CloseHandle( m_hRunMainThread );
	}
}

void CTestHarness::RunTest( TestFunc func, void* arg )
{
	m_ulMainThreadID = GetCurrentThreadId();
	AssertAt( !m_bTestThreadRunning, 122 );
	m_pvTestThreadArg = arg;
	m_pTestFunc = func;
	if ( !m_bTestThreadRunning )
	{
		m_bTestThreadRunning = true;
		if ( !m_hRunTestThread )
		{
			m_hRunTestThread = CreateEventA( NULL, FALSE, FALSE, NULL );
		}
		if ( !m_hRunMainThread )
		{
			m_hRunMainThread = CreateEventA( NULL, FALSE, FALSE, NULL );
		}
		m_hThreadTestDriver = CreateThread( NULL, 0, TestThreadProxy, this, 0, &m_ulTestThreadID );
	}
}

void CTestHarness::RunFrame()
{
	if ( m_bTestThreadRunning )
	{
		AssertAt( GetCurrentThreadId() == m_ulMainThreadID, 141 );
		if ( m_bLetTestThreadRun )
		{
			m_bLetTestThreadRun = false;
			SetEvent( m_hRunTestThread );
			WaitForSingleObject( m_hRunMainThread, INFINITE );
		}
	}
}

void CTestHarness::Yield()
{
	AssertAt( GetCurrentThreadId() == m_ulTestThreadID, 173 );
	SetEvent( m_hRunMainThread );
	if ( m_bStopTestThread )
	{
		ExitThread( 0 );
	}
	m_bLetTestThreadRun = true;
	WaitForSingleObject( m_hRunTestThread, INFINITE );
}

void CTestHarness::TerminateTestThread()
{
	AssertAt( GetCurrentThreadId() == m_ulMainThreadID, 239 );
	if ( m_hThreadTestDriver )
	{
#pragma warning( suppress: 6258 )
		TerminateThread( m_hThreadTestDriver, 0 );
		m_hThreadTestDriver = NULL;
		m_bTestThreadRunning = false;
		m_bStopTestThread = false;
	}
}

DWORD WINAPI CTestHarness::TestThreadProxy( void* arg )
{
	CTestHarness* harness = (CTestHarness*)arg;
	harness->Yield();
	harness->m_pTestFunc( harness->m_pvTestThreadArg );
	harness->m_hThreadTestDriver = NULL;
	harness->m_bTestThreadRunning = false;
	harness->m_bStopTestThread = false;
	SetEvent( harness->m_hRunMainThread );
	return 0;
}

extern "C"
{

TIER0_API void Test_RunTest( TestFunc func, void* arg )
{
	g_TestHarness.RunTest( func, arg );
}

TIER0_API void Test_RunFrame()
{
	g_TestHarness.RunFrame();
}

TIER0_API void TestThread_Yield()
{
	g_TestHarness.Yield();
}

void Test_SetFailed()
{
	g_TestHarness.m_bTestFailed = true;
}

TIER0_API bool Test_HasFailed()
{
	return g_TestHarness.m_bTestFailed;
}

TIER0_API bool Test_HasFinished()
{
	return !g_TestHarness.m_bTestThreadRunning && g_TestHarness.m_hRunMainThread != NULL;
}

TIER0_API bool Test_IsActive()
{
	return g_TestHarness.m_hRunMainThread != NULL;
}

TIER0_API void Test_TerminateThread()
{
	g_TestHarness.TerminateTestThread();
}

}

#else
#include <pthread.h>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

typedef void (*TestFunc)(void*);

class CTestHarness {
public:
    CTestHarness();
    ~CTestHarness();

    void RunTest(TestFunc function, void* argument);
    void RunFrame();
    void Yield();
    void TerminateTestThread();
    void SetFailed();
    bool HasFailed();
    bool HasFinished();
    bool IsActive();

private:
    static void* TestThreadProxy(void* argument);
    static void FinishTestThread(void* argument);
    static void CheckPthread(int result);
    void RejectLocked(const char* reason);
    bool JoinLocked();
    void StopAndJoin(bool requireOwner);

    pthread_mutex_t m_Mutex = PTHREAD_MUTEX_INITIALIZER;
    pthread_cond_t m_RunWorker = PTHREAD_COND_INITIALIZER;
    pthread_cond_t m_RunMain = PTHREAD_COND_INITIALIZER;
    pthread_t m_Thread{};
    pthread_t m_MainThread;
    bool m_Joinable = false;
    bool m_Joining = false;
    bool m_Active = false;
    bool m_Running = false;
    bool m_Stop = false;
    bool m_Ready = false;
    bool m_RunPermit = false;
    bool m_Failed = false;
    std::uint64_t m_YieldGeneration = 0;
    void* m_Argument = nullptr;
    TestFunc m_Function = nullptr;
};

extern "C" {
CTestHarness g_TestHarness;
}

CTestHarness::CTestHarness() : m_MainThread(pthread_self()) {
}

CTestHarness::~CTestHarness() {
    StopAndJoin(false);
    CheckPthread(pthread_cond_destroy(&m_RunWorker));
    CheckPthread(pthread_cond_destroy(&m_RunMain));
    CheckPthread(pthread_mutex_destroy(&m_Mutex));
}

void CTestHarness::CheckPthread(int result) {
    if (result != 0) {
        std::fprintf(stderr, "Test harness synchronization failure: %d\n", result);
        std::abort();
    }
}

void CTestHarness::RejectLocked(const char* reason) {
    m_Failed = true;
    std::fprintf(stderr, "Test harness rejected operation: %s\n", reason);
}

bool CTestHarness::JoinLocked() {
    if (!m_Joinable) {
        return true;
    }
    // Joining must not hold the state mutex: thread-local destructors may report
    // a failure through Test_SetFailed before pthread_join can complete.
    m_Joining = true;
    const pthread_t thread = m_Thread;
    CheckPthread(pthread_mutex_unlock(&m_Mutex));
    const int result = pthread_join(thread, nullptr);
    CheckPthread(pthread_mutex_lock(&m_Mutex));
    m_Joining = false;
    if (result != 0) {
        RejectLocked("pthread_join failed; owned handle retained");
        return false;
    }
    m_Joinable = false;
    m_Thread = pthread_t{};
    return true;
}

void CTestHarness::RunTest(TestFunc function, void* argument) {
    CheckPthread(pthread_mutex_lock(&m_Mutex));
    if (m_Running || m_Joining || !function) {
        RejectLocked("RunTest requires an idle harness and a callback");
        CheckPthread(pthread_mutex_unlock(&m_Mutex));
        return;
    }
    if (!JoinLocked()) {
        CheckPthread(pthread_mutex_unlock(&m_Mutex));
        return;
    }
    m_MainThread = pthread_self();
    m_Function = function;
    m_Argument = argument;
    m_Stop = false;
    m_RunPermit = false;
    m_Ready = false;
    m_Running = true;
    const int result = pthread_create(&m_Thread, nullptr, TestThreadProxy, this);
    if (result != 0) {
        m_Running = false;
        m_Thread = pthread_t{};
        m_Function = nullptr;
        m_Argument = nullptr;
        RejectLocked("pthread_create failed; callback not started");
    } else {
        m_Joinable = true;
        m_Active = true;
    }
    CheckPthread(pthread_mutex_unlock(&m_Mutex));
}

void CTestHarness::RunFrame() {
    CheckPthread(pthread_mutex_lock(&m_Mutex));
    if (!pthread_equal(pthread_self(), m_MainThread) || m_Joining) {
        RejectLocked("RunFrame requires the controlling thread");
        CheckPthread(pthread_mutex_unlock(&m_Mutex));
        return;
    }
    while (m_Running && !m_Ready) {
        CheckPthread(pthread_cond_wait(&m_RunMain, &m_Mutex));
    }
    if (m_Running) {
        const std::uint64_t generation = m_YieldGeneration;
        m_Ready = false;
        m_RunPermit = true;
        CheckPthread(pthread_cond_signal(&m_RunWorker));
        while (m_Running && generation == m_YieldGeneration) {
            CheckPthread(pthread_cond_wait(&m_RunMain, &m_Mutex));
        }
    }
    CheckPthread(pthread_mutex_unlock(&m_Mutex));
}

void CTestHarness::Yield() {
    CheckPthread(pthread_mutex_lock(&m_Mutex));
    if (!m_Joinable || !m_Running || !pthread_equal(pthread_self(), m_Thread)) {
        RejectLocked("Yield requires the active test thread");
        CheckPthread(pthread_mutex_unlock(&m_Mutex));
        return;
    }
    m_Ready = true;
    ++m_YieldGeneration;
    CheckPthread(pthread_cond_signal(&m_RunMain));
    while (!m_RunPermit && !m_Stop) {
        CheckPthread(pthread_cond_wait(&m_RunWorker, &m_Mutex));
    }
    const bool stop = m_Stop;
    m_RunPermit = false;
    m_Ready = false;
    CheckPthread(pthread_mutex_unlock(&m_Mutex));
    if (stop) {
        // This is a cooperative exit by the worker at the exported yield point.
        // The proxy cleanup completes the state transition before the owner joins.
        pthread_exit(nullptr);
    }
}

void CTestHarness::StopAndJoin(bool requireOwner) {
    CheckPthread(pthread_mutex_lock(&m_Mutex));
    if (m_Joining || (requireOwner && !pthread_equal(pthread_self(), m_MainThread))) {
        RejectLocked("Stop requires the controlling thread and no concurrent join");
        CheckPthread(pthread_mutex_unlock(&m_Mutex));
        if (!requireOwner) {
            std::fputs("Test harness destruction overlaps a live operation\n", stderr);
            std::abort();
        }
        return;
    }
    if (m_Joinable && pthread_equal(pthread_self(), m_Thread)) {
        CheckPthread(pthread_mutex_unlock(&m_Mutex));
        std::fputs("Test harness cannot destroy itself from its worker\n", stderr);
        std::abort();
    }
    if (m_Running) {
        m_Stop = true;
        CheckPthread(pthread_cond_signal(&m_RunWorker));
        // Callbacks must yield or return. There is no safe forced-stop/detach
        // fallback, and no production timeout that would abandon live storage.
        while (m_Running) {
            CheckPthread(pthread_cond_wait(&m_RunMain, &m_Mutex));
        }
    }
    const bool joined = JoinLocked();
    CheckPthread(pthread_mutex_unlock(&m_Mutex));
    if (!joined && !requireOwner) {
        std::fputs("Test harness destruction could not join its owned worker\n", stderr);
        std::abort();
    }
}

void CTestHarness::TerminateTestThread() {
    StopAndJoin(true);
}

void CTestHarness::SetFailed() {
    CheckPthread(pthread_mutex_lock(&m_Mutex));
    m_Failed = true;
    CheckPthread(pthread_mutex_unlock(&m_Mutex));
}

bool CTestHarness::HasFailed() {
    CheckPthread(pthread_mutex_lock(&m_Mutex));
    const bool failed = m_Failed;
    CheckPthread(pthread_mutex_unlock(&m_Mutex));
    return failed;
}

bool CTestHarness::HasFinished() {
    CheckPthread(pthread_mutex_lock(&m_Mutex));
    const bool finished = m_Active && !m_Running;
    CheckPthread(pthread_mutex_unlock(&m_Mutex));
    return finished;
}

bool CTestHarness::IsActive() {
    CheckPthread(pthread_mutex_lock(&m_Mutex));
    const bool active = m_Active;
    CheckPthread(pthread_mutex_unlock(&m_Mutex));
    return active;
}

void CTestHarness::FinishTestThread(void* argument) {
    CTestHarness* harness = static_cast<CTestHarness*>(argument);
    CheckPthread(pthread_mutex_lock(&harness->m_Mutex));
    harness->m_Running = false;
    harness->m_Stop = false;
    harness->m_Ready = false;
    harness->m_RunPermit = false;
    harness->m_Function = nullptr;
    harness->m_Argument = nullptr;
    CheckPthread(pthread_cond_broadcast(&harness->m_RunMain));
    CheckPthread(pthread_mutex_unlock(&harness->m_Mutex));
}

void* CTestHarness::TestThreadProxy(void* argument) {
    CTestHarness* harness = static_cast<CTestHarness*>(argument);
    pthread_cleanup_push(FinishTestThread, harness);
    CheckPthread(pthread_mutex_lock(&harness->m_Mutex));
    harness->m_Ready = true;
    CheckPthread(pthread_cond_signal(&harness->m_RunMain));
    while (!harness->m_RunPermit && !harness->m_Stop) {
        CheckPthread(pthread_cond_wait(&harness->m_RunWorker, &harness->m_Mutex));
    }
    const bool stop = harness->m_Stop;
    const TestFunc function = harness->m_Function;
    void* const callbackArgument = harness->m_Argument;
    harness->m_RunPermit = false;
    harness->m_Ready = false;
    CheckPthread(pthread_mutex_unlock(&harness->m_Mutex));
    if (!stop) {
        function(callbackArgument);
    }
    pthread_cleanup_pop(1);
    return nullptr;
}

extern "C" {
void Test_RunTest(TestFunc function, void* argument) {
    g_TestHarness.RunTest(function, argument);
}

void Test_RunFrame() {
    g_TestHarness.RunFrame();
}

void TestThread_Yield() {
    g_TestHarness.Yield();
}

void Test_SetFailed() {
    g_TestHarness.SetFailed();
}

bool Test_HasFailed() {
    return g_TestHarness.HasFailed();
}

bool Test_HasFinished() {
    return g_TestHarness.HasFinished();
}

bool Test_IsActive() {
    return g_TestHarness.IsActive();
}

void Test_TerminateThread() {
    g_TestHarness.TerminateTestThread();
}
}

#endif
