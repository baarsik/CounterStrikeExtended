#include "worker_fault_injection.h"
#include <atomic>
#include <cerrno>
#include <new>

namespace {
std::atomic<WorkerFault> failure{WorkerFault::None};
std::atomic<int> liveResources{0};
#if defined(_WIN32)
std::atomic<HANDLE> workerHandle{nullptr};
#else
std::atomic<pthread_mutex_t*> queueMutex{nullptr};
#endif
}

void SetWorkerFault(WorkerFault fault) {
    failure = fault;
}

int WorkerLiveResources() {
    return liveResources;
}

#if defined(_WIN32)
void WINAPI CseInitializeCriticalSection(LPCRITICAL_SECTION section) {
    if (failure == WorkerFault::Mutex) {
        throw std::bad_alloc();
    }
    InitializeCriticalSection(section);
    ++liveResources;
}

BOOL WINAPI CseInitializeCriticalSectionAndSpinCount(LPCRITICAL_SECTION section, DWORD count) {
    if (failure == WorkerFault::Mutex) {
        return FALSE;
    }
    const BOOL result = InitializeCriticalSectionAndSpinCount(section, count);
    if (result) {
        ++liveResources;
    }
    return result;
}

void WINAPI CseDeleteCriticalSection(LPCRITICAL_SECTION section) {
    DeleteCriticalSection(section);
    --liveResources;
}

HANDLE WINAPI CseCreateEventA(LPSECURITY_ATTRIBUTES attributes, BOOL manual, BOOL initial, LPCSTR name) {
    if (failure == WorkerFault::Event) {
        return nullptr;
    }
    HANDLE result = CreateEventA(attributes, manual, initial, name);
    if (result) {
        ++liveResources;
    }
    return result;
}

HANDLE WINAPI CseCreateThread(LPSECURITY_ATTRIBUTES attributes, SIZE_T bytes,
    LPTHREAD_START_ROUTINE entry, LPVOID data, DWORD flags, LPDWORD identifier) {
    if (failure == WorkerFault::Thread) {
        return nullptr;
    }
    HANDLE result = CreateThread(attributes, bytes, entry, data, flags, identifier);
    if (result) {
        workerHandle = result;
        ++liveResources;
    }
    return result;
}

BOOL WINAPI CseCloseHandle(HANDLE handle) {
    const BOOL result = CloseHandle(handle);
    if (result) {
        --liveResources;
    }
    return result;
}

DWORD WINAPI CseWaitForSingleObject(HANDLE handle, DWORD milliseconds) {
    if (failure == WorkerFault::Join && handle == workerHandle) {
        SetLastError(ERROR_INVALID_HANDLE);
        return WAIT_FAILED;
    }
    return WaitForSingleObject(handle, milliseconds);
}
#else
int CseMutexAttributeInit(pthread_mutexattr_t* attribute) {
    if (failure == WorkerFault::Attribute) {
        return ENOMEM;
    }
    const int result = pthread_mutexattr_init(attribute);
    if (result == 0) {
        ++liveResources;
    }
    return result;
}

int CseMutexAttributeType(pthread_mutexattr_t* attribute, int type) {
    return failure == WorkerFault::AttributeType ? EINVAL : pthread_mutexattr_settype(attribute, type);
}

int CseMutexAttributeDestroy(pthread_mutexattr_t* attribute) {
    const int result = pthread_mutexattr_destroy(attribute);
    if (result == 0) {
        --liveResources;
    }
    return result;
}

int CseMutexInit(pthread_mutex_t* mutex, const pthread_mutexattr_t* attribute) {
    if (failure == WorkerFault::Mutex) {
        return ENOMEM;
    }
    const int result = pthread_mutex_init(mutex, attribute);
    if (result == 0) {
        queueMutex = mutex;
        ++liveResources;
    }
    return result;
}

int CseMutexDestroy(pthread_mutex_t* mutex) {
    const int result = pthread_mutex_destroy(mutex);
    pthread_mutex_t* expected = mutex;
    if (result == 0 && queueMutex.compare_exchange_strong(expected, nullptr)) {
        --liveResources;
    }
    return result;
}

int CseConditionInit(pthread_cond_t* condition, const pthread_condattr_t* attribute) {
    if (failure == WorkerFault::Condition) {
        return ENOMEM;
    }
    const int result = pthread_cond_init(condition, attribute);
    if (result == 0) {
        ++liveResources;
    }
    return result;
}

int CseConditionDestroy(pthread_cond_t* condition) {
    const int result = pthread_cond_destroy(condition);
    if (result == 0) {
        --liveResources;
    }
    return result;
}

int CseThreadCreate(pthread_t* thread, const pthread_attr_t* attribute, void* (*entry)(void*), void* data) {
    if (failure == WorkerFault::Thread) {
        return EAGAIN;
    }
    const int result = pthread_create(thread, attribute, entry, data);
    if (result == 0) {
        ++liveResources;
    }
    return result;
}

int CseThreadJoin(pthread_t thread, void** result) {
    if (failure == WorkerFault::Join) {
        return EDEADLK;
    }
    const int joined = pthread_join(thread, result);
    if (joined == 0) {
        --liveResources;
    }
    return joined;
}
#endif
