#pragma once

#include "cd.h"
#include "ithread.h"

#include <cstddef>
#include <atomic>
#include <type_traits>

#if defined( _WIN32 )
#include <Windows.h>
#else
#include <pthread.h>
#endif

#define MAX_THREAD_CMDS 10

class CThread : public IThread
{
public:
    struct threadcmds {
        vfunc pfn = nullptr;
        filefunc fileFunction = nullptr;
        int param1 = 0;
        int param2 = 0;
        char filename[CCDAudio::TrackPathCapacity]{};
    };

	CThread();
	~CThread() override;

	bool Init() override;
	bool Shutdown() override;
	bool AddThreadItem( vfunc pfn, int param1, int param2 ) override;
    bool AddFileThreadItem(filefunc function, const char *filename, int looping) override;

#if defined( _WIN32 )
	HANDLE m_hEvent;
	CRITICAL_SECTION m_mutex;
#else
	pthread_mutex_t m_mutex;
	pthread_cond_t m_Condition;
	int m_cSet;
#endif

    std::atomic<bool> m_bThreadRunning;
    std::atomic<bool> m_bThreadStopped;

	int m_nCmds;
	int _m_nCmds;
	threadcmds m_rgCmds[MAX_THREAD_CMDS + 1];
	threadcmds _m_rgCmds[MAX_THREAD_CMDS + 1];

#if defined( _WIN32 )
	HANDLE m_hThread;
	DWORD m_dwThreadID;
#else
	pthread_t m_dwThreadID;
#endif
private:
    bool AddCommand(const threadcmds &command);
    bool ShutdownLocked();
    bool InitializeSynchronization();
    bool LockLifecycle();
    void UnlockLifecycle();
    bool LockCommandAccess();
    void UnlockCommandAccess();

#if defined(_WIN32)
    SRWLOCK lifecycleMutex_ = SRWLOCK_INIT;
    SRWLOCK commandAccessMutex_ = SRWLOCK_INIT;
#else
    pthread_mutex_t lifecycleMutex_ = PTHREAD_MUTEX_INITIALIZER;
    pthread_mutex_t commandAccessMutex_ = PTHREAD_MUTEX_INITIALIZER;
#endif
    std::atomic<bool> controllerTransition_{false};
    bool synchronizationInitialized_ = false;
    bool controllerInitialized_ = false;
};

// Records are private to this worker, not an exported engine ABI. They own paths
// and remain trivially copyable for the existing batch transfer.
static_assert(std::is_trivially_copyable<CThread::threadcmds>::value, "CD command batch transfer");
