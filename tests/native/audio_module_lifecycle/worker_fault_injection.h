#pragma once

#if defined(_WIN32)
#include <Windows.h>
#else
#include <pthread.h>
#endif

enum class WorkerFault {
    None,
    Mutex,
    Event,
    Thread,
    Attribute,
    AttributeType,
    Condition,
    Join
};

void SetWorkerFault(WorkerFault fault);
int WorkerLiveResources();

#if defined(_WIN32)
void WINAPI CseInitializeCriticalSection(LPCRITICAL_SECTION section);
BOOL WINAPI CseInitializeCriticalSectionAndSpinCount(LPCRITICAL_SECTION section, DWORD count);
void WINAPI CseDeleteCriticalSection(LPCRITICAL_SECTION section);
HANDLE WINAPI CseCreateEventA(LPSECURITY_ATTRIBUTES attributes, BOOL manual, BOOL initial, LPCSTR name);
HANDLE WINAPI CseCreateThread(LPSECURITY_ATTRIBUTES attributes, SIZE_T bytes,
    LPTHREAD_START_ROUTINE entry, LPVOID data, DWORD flags, LPDWORD identifier);
BOOL WINAPI CseCloseHandle(HANDLE handle);
DWORD WINAPI CseWaitForSingleObject(HANDLE handle, DWORD milliseconds);
#if defined(CSE_INJECT_WORKER_CALLS)
#define InitializeCriticalSection CseInitializeCriticalSection
#define InitializeCriticalSectionAndSpinCount CseInitializeCriticalSectionAndSpinCount
#define DeleteCriticalSection CseDeleteCriticalSection
#define CreateEventA CseCreateEventA
#define CreateThread CseCreateThread
#define CloseHandle CseCloseHandle
#define WaitForSingleObject CseWaitForSingleObject
#endif
#else
int CseMutexAttributeInit(pthread_mutexattr_t* attribute);
int CseMutexAttributeType(pthread_mutexattr_t* attribute, int type);
int CseMutexAttributeDestroy(pthread_mutexattr_t* attribute);
int CseMutexInit(pthread_mutex_t* mutex, const pthread_mutexattr_t* attribute);
int CseMutexDestroy(pthread_mutex_t* mutex);
int CseConditionInit(pthread_cond_t* condition, const pthread_condattr_t* attribute);
int CseConditionDestroy(pthread_cond_t* condition);
int CseThreadCreate(pthread_t* thread, const pthread_attr_t* attribute, void* (*entry)(void*), void* data);
int CseThreadJoin(pthread_t thread, void** result);
#if defined(CSE_INJECT_WORKER_CALLS)
#define pthread_mutexattr_init CseMutexAttributeInit
#define pthread_mutexattr_settype CseMutexAttributeType
#define pthread_mutexattr_destroy CseMutexAttributeDestroy
#define pthread_mutex_init CseMutexInit
#define pthread_mutex_destroy CseMutexDestroy
#define pthread_cond_init CseConditionInit
#define pthread_cond_destroy CseConditionDestroy
#define pthread_create CseThreadCreate
#define pthread_join CseThreadJoin
#endif
#endif
