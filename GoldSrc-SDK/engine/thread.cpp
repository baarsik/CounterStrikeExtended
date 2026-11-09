#include "thread.h"

#include <cstring>
#include <cstdlib>

#if defined( _WIN32 )
#include <process.h>

extern "C" _ACRTIMP void __cdecl _wassert( const wchar_t* message, const wchar_t* file, unsigned int line );
#else
#include <cerrno>
#include <unistd.h>
#endif

#if defined( _WIN32 )
//-----------------------------------------------------------------------------
// Purpose: Thread processing function
// Input  : pv - 
// Output : static DWORD WINAPI
//-----------------------------------------------------------------------------
static DWORD WINAPI ThreadFunc(void* context) {
    auto* self = static_cast<CThread*>(context);
    if (!self) {
        return 0;
    }
    while (self->m_bThreadRunning) {
        if (WaitForSingleObject(self->m_hEvent, INFINITE) != WAIT_OBJECT_0) {
            self->m_bThreadRunning = false;
            break;
        }
        EnterCriticalSection(&self->m_mutex);
        const int count = self->m_bThreadRunning ? self->m_nCmds : 0;
        std::memcpy(self->_m_rgCmds, self->m_rgCmds, sizeof(CThread::threadcmds) * count);
        self->_m_nCmds = count;
        self->m_nCmds = 0;
        ResetEvent(self->m_hEvent);
        LeaveCriticalSection(&self->m_mutex);
        for (int index = 0; index < count && self->m_bThreadRunning; ++index) {
            const CThread::threadcmds& command = self->_m_rgCmds[index];
            if (command.fileFunction) {
                (GetInteralCDAudio()->*command.fileFunction)(command.filename, command.param2);
            } else if (command.pfn) {
                (GetInteralCDAudio()->*command.pfn)(command.param1, command.param2);
            }
        }
    }
    self->m_bThreadStopped = true;
    return 1;
}
#else
static void* ThreadFunc(void* context) {
    auto* self = static_cast<CThread*>(context);
    while (self->m_bThreadRunning) {
        pthread_mutex_lock(&self->m_mutex);
        while (!self->m_cSet && self->m_bThreadRunning) {
            const int status = pthread_cond_wait(&self->m_Condition, &self->m_mutex);
            if (status != 0 && status != EINTR) {
                self->m_bThreadRunning = false;
            }
        }
        const int count = self->m_bThreadRunning ? self->m_nCmds : 0;
        self->m_cSet = 0;
        std::memcpy(self->_m_rgCmds, self->m_rgCmds, sizeof(CThread::threadcmds) * count);
        self->_m_nCmds = count;
        self->m_nCmds = 0;
        pthread_mutex_unlock(&self->m_mutex);
        for (int index = 0; index < count && self->m_bThreadRunning; ++index) {
            const CThread::threadcmds& command = self->_m_rgCmds[index];
            if (command.fileFunction) {
                (GetInteralCDAudio()->*command.fileFunction)(command.filename, command.param2);
            } else if (command.pfn) {
                (GetInteralCDAudio()->*command.pfn)(command.param1, command.param2);
            }
        }
    }
    self->m_bThreadStopped = true;
    return nullptr;
}
#endif

CThread::CThread()
#if defined(_WIN32)
    : m_hEvent(nullptr),
      m_mutex{},
#else
    : m_mutex{},
      m_Condition{},
      m_cSet(0),
#endif
      m_bThreadRunning(false),
      m_bThreadStopped(false),
      m_nCmds(0),
      _m_nCmds(0),
      m_rgCmds{},
      _m_rgCmds{},
#if defined(_WIN32)
      m_hThread(nullptr),
#endif
      m_dwThreadID(0) {
}

CThread::~CThread()
{
#if !defined(_WIN32)
    pthread_mutex_destroy(&commandAccessMutex_);
    pthread_mutex_destroy(&lifecycleMutex_);
#endif
}

bool CThread::LockLifecycle() {
#if defined(_WIN32)
    AcquireSRWLockExclusive(&lifecycleMutex_);
    return true;
#else
    return pthread_mutex_lock(&lifecycleMutex_) == 0;
#endif
}

void CThread::UnlockLifecycle() {
#if defined(_WIN32)
    ReleaseSRWLockExclusive(&lifecycleMutex_);
#else
    pthread_mutex_unlock(&lifecycleMutex_);
#endif
}

bool CThread::LockCommandAccess() {
#if defined(_WIN32)
    AcquireSRWLockExclusive(&commandAccessMutex_);
    return true;
#else
    return pthread_mutex_lock(&commandAccessMutex_) == 0;
#endif
}

void CThread::UnlockCommandAccess() {
#if defined(_WIN32)
    ReleaseSRWLockExclusive(&commandAccessMutex_);
#else
    pthread_mutex_unlock(&commandAccessMutex_);
#endif
}

bool CThread::AddThreadItem(vfunc function, int first, int second) {
    threadcmds command{};
    command.pfn = function;
    command.param1 = first;
    command.param2 = second;
    return AddCommand(command);
}

bool CThread::AddFileThreadItem(filefunc function, const char *filename, int looping) {
    if (!function || !filename || !filename[0] || std::strlen(filename) >= CCDAudio::TrackPathCapacity) {
        return false;
    }
    threadcmds command{};
    command.fileFunction = function;
    command.param2 = looping;
    std::memcpy(command.filename, filename, std::strlen(filename) + 1);
    return AddCommand(command);
}

bool CThread::AddCommand(const threadcmds &command) {
    // Remains alive across Init/Shutdown, unlike the platform queue primitives.
    if (!LockCommandAccess()) {
        return false;
    }
    if (!synchronizationInitialized_ || !m_bThreadRunning) {
        UnlockCommandAccess();
        return false;
    }
#if defined(_WIN32)
    EnterCriticalSection(&m_mutex);
#else
    pthread_mutex_lock(&m_mutex);
#endif
    bool accepted = m_bThreadRunning && m_nCmds < MAX_THREAD_CMDS;
    // Periodic status work may coalesce. Playback controls retain their order;
    // dropping a second Stop could otherwise make an earlier Play survive it.
    if (command.pfn == &CCDAudio::_CDUpdate) {
        for (int index = 0; index < m_nCmds; ++index) {
            if (m_rgCmds[index].pfn == command.pfn) {
                accepted = false;
                break;
            }
        }
    }
    if (accepted) {
        m_rgCmds[m_nCmds++] = command;
    }
#if defined(_WIN32)
    LeaveCriticalSection(&m_mutex);
    if (accepted) {
        SetEvent(m_hEvent);
    }
#else
    if (accepted) {
        m_cSet = 1;
        pthread_cond_signal(&m_Condition);
    }
    pthread_mutex_unlock(&m_mutex);
#endif
    UnlockCommandAccess();
    return accepted;
}

bool CThread::Shutdown() {
    if (controllerTransition_ || !LockLifecycle()) {
        return false;
    }
    const bool stopped = !controllerTransition_ && ShutdownLocked();
    UnlockLifecycle();
    return stopped;
}

bool CThread::ShutdownLocked() {
    if (!LockCommandAccess()) {
        return false;
    }
    if (!synchronizationInitialized_) {
        UnlockCommandAccess();
        return true;
    }
    m_bThreadRunning = false;
#if defined(_WIN32)
    EnterCriticalSection(&m_mutex);
    m_nCmds = 0;
    LeaveCriticalSection(&m_mutex);
    const bool signaled = SetEvent(m_hEvent) != FALSE;
#else
    if (pthread_mutex_lock(&m_mutex) != 0) {
        UnlockCommandAccess();
        return false;
    }
    m_nCmds = 0;
    m_cSet = 1;
    const bool signaled = pthread_cond_signal(&m_Condition) == 0;
    pthread_mutex_unlock(&m_mutex);
#endif
    UnlockCommandAccess();
    if (!signaled) {
        return false;
    }
    // An in-flight callback may enqueue and observe the stopped state while joining.
#if defined(_WIN32)
    if (m_hThread && (m_dwThreadID == GetCurrentThreadId()
        || WaitForSingleObject(m_hThread, INFINITE) != WAIT_OBJECT_0)) {
        return false;
    }
#else
    if (m_dwThreadID && (pthread_equal(m_dwThreadID, pthread_self())
        || pthread_join(m_dwThreadID, nullptr) != 0)) {
        return false;
    }
#endif
    if (!LockCommandAccess()) {
        return false;
    }
#if defined(_WIN32)
    if (m_hThread && !CloseHandle(m_hThread)) {
        UnlockCommandAccess();
        return false;
    }
    m_hThread = nullptr;
    m_dwThreadID = 0;
    if (!CloseHandle(m_hEvent)) {
        UnlockCommandAccess();
        return false;
    }
    m_hEvent = nullptr;
    DeleteCriticalSection(&m_mutex);
#else
    m_dwThreadID = 0;
    if (pthread_cond_destroy(&m_Condition) != 0 || pthread_mutex_destroy(&m_mutex) != 0) {
        UnlockCommandAccess();
        return false;
    }
    m_cSet = 0;
#endif
    synchronizationInitialized_ = false;
    m_nCmds = 0;
    _m_nCmds = 0;
    const bool closeController = controllerInitialized_;
    controllerInitialized_ = false;
    UnlockCommandAccess();
    if (closeController) {
        // No lock may survive a callback failure across the retained EH boundary.
        controllerTransition_ = true;
        UnlockLifecycle();
        cdaudio->Shutdown();
        if (!LockLifecycle()) {
            std::abort();
        }
        controllerTransition_ = false;
    }
    return true;
}

bool CThread::InitializeSynchronization() {
#if defined(_WIN32)
    if (!InitializeCriticalSectionAndSpinCount(&m_mutex, 0)) {
        return false;
    }
    m_hEvent = CreateEventA(nullptr, TRUE, FALSE, nullptr);
    if (!m_hEvent) {
        DeleteCriticalSection(&m_mutex);
        return false;
    }
#else
    pthread_mutexattr_t attribute;
    if (pthread_mutexattr_init(&attribute) != 0) {
        return false;
    }
    if (pthread_mutexattr_settype(&attribute, PTHREAD_MUTEX_RECURSIVE) != 0) {
        pthread_mutexattr_destroy(&attribute);
        return false;
    }
    const int mutexCreated = pthread_mutex_init(&m_mutex, &attribute);
    pthread_mutexattr_destroy(&attribute);
    if (mutexCreated != 0) {
        return false;
    }
    if (pthread_cond_init(&m_Condition, nullptr) != 0) {
        pthread_mutex_destroy(&m_mutex);
        return false;
    }
    m_cSet = 0;
#endif
    return true;
}

bool CThread::Init() {
    if (controllerTransition_ || !LockLifecycle()) {
        return false;
    }
    if (controllerTransition_ || synchronizationInitialized_) {
        const bool initialized = !controllerTransition_ && m_bThreadRunning;
        UnlockLifecycle();
        return initialized;
    }
    m_nCmds = 0;
    _m_nCmds = 0;
    m_bThreadStopped = false;
    if (!InitializeSynchronization()) {
        UnlockLifecycle();
        return false;
    }
    if (!LockCommandAccess()) {
        std::abort();
    }
    synchronizationInitialized_ = true;
    controllerInitialized_ = true;
    m_bThreadRunning = true;
    UnlockCommandAccess();
    // Controller code may enqueue or fail. Retain its unfinished transition if
    // an exception crosses this TU, allowing Run's outer guard to fail closed.
    controllerTransition_ = true;
    UnlockLifecycle();
    cdaudio->Init();
    if (!LockLifecycle()) {
        std::abort();
    }
    controllerTransition_ = false;
#if defined(_WIN32)
    m_hThread = CreateThread(nullptr, 0, ThreadFunc, this, 0, &m_dwThreadID);
    const bool created = m_hThread != nullptr;
#else
    pthread_t identifier;
    const bool created = pthread_create(&identifier, nullptr, ThreadFunc, this) == 0;
    if (created) {
        m_dwThreadID = identifier;
    }
#endif
    if (!created) {
        ShutdownLocked();
    }
    UnlockLifecycle();
    return created;
}

static CThread g_Thread;
IThread* thread = &g_Thread;
