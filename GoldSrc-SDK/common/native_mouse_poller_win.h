#pragma once

#if defined(_WIN32) && defined(_WIN64)

#include "native_mouse_poller.h"
#include <windows.h>
#include <intrin.h>
#include <cstdlib>

namespace cse { namespace mouse {

class WinBackend {
public:
    struct Services {
        void *context;
        HANDLE (*createControl)(void *);
        HANDLE (*createThread)(void *, LPTHREAD_START_ROUTINE, LPVOID);
        BOOL (*signal)(void *, HANDLE);
        BOOL (*reset)(void *, HANDLE);
        DWORD (*wait)(void *, HANDLE, DWORD);
        BOOL (*close)(void *, HANDLE);
        BOOL (*waitCondition)(void *, PCONDITION_VARIABLE, PSRWLOCK, DWORD);
        void (*wakeCondition)(void *, PCONDITION_VARIABLE);
        std::uint64_t (*clock)(void *);
        DWORD (*lastError)(void *);
        CallResult (*cursor)(void *, Point &, SampleReason);
    };

    constexpr explicit WinBackend(Services bindings) : services_(bindings) {}

    static constexpr Services KernelServices(CallResult (*cursor)(void *, Point &, SampleReason), void *context) {
        return Services{context, &CreateControlHandle, &CreateWorkerHandle, &SignalHandle, &ResetHandle,
            &WaitHandle, &CloseHandleService, &WaitConditionService, &WakeConditionService, &Clock, &LastError, cursor};
    }

    static constexpr Services SystemServices() {
        return KernelServices(&SystemCursor, nullptr);
    }

    void Lock() {
        AcquireSRWLockExclusive(&lock_);
    }

    void Unlock() {
        ReleaseSRWLockExclusive(&lock_);
    }

    std::uint64_t NowMs() {
        return services_.clock(services_.context);
    }

    CallResult CreateControl() {
        if (!Valid() || control_) {
            return {false, ERROR_INVALID_PARAMETER};
        }
        control_ = services_.createControl(services_.context);
        return control_ ? CallResult{true, 0} : Error();
    }

    CallResult StartThread(void (*entry)(void *), void *context) {
        if (thread_ || !entry) {
            return {false, ERROR_INVALID_PARAMETER};
        }
        entry_ = entry;
        context_ = context;
        thread_ = services_.createThread(services_.context, &ThreadEntry, this);
        return thread_ ? CallResult{true, 0} : Error();
    }

    CallResult SignalControl() {
        return services_.signal(services_.context, control_) ? CallResult{true, 0} : Error();
    }

    CallResult ResetControl() {
        return services_.reset(services_.context, control_) ? CallResult{true, 0} : Error();
    }

    WaitResult WaitControl(std::uint32_t timeout) {
        return Wait(control_, timeout);
    }

    WaitResult WaitCondition(std::uint32_t timeout) {
        if (services_.waitCondition(services_.context, &condition_, &lock_, timeout)) {
            return {WaitKind::Signaled, 0};
        }
        const DWORD error = services_.lastError(services_.context);
        return {error == ERROR_TIMEOUT ? WaitKind::Timeout : WaitKind::Failed, error};
    }

    void WakeCondition() {
        services_.wakeCondition(services_.context, &condition_);
    }

    CallResult ReadCursor(Point &point, SampleReason reason) {
        return services_.cursor(services_.context, point, reason);
    }

    WaitResult Join(std::uint32_t timeout) {
        return thread_ ? Wait(thread_, timeout) : WaitResult{WaitKind::Signaled, 0};
    }

    CallResult CloseHandles() {
        if (thread_) {
            if (!services_.close(services_.context, thread_)) {
                return Error();
            }
            thread_ = nullptr;
        }
        if (control_) {
            if (!services_.close(services_.context, control_)) {
                return Error();
            }
            control_ = nullptr;
        }
        entry_ = nullptr;
        context_ = nullptr;
        return {true, 0};
    }

private:
    bool Valid() const {
        return services_.createControl && services_.createThread && services_.signal && services_.reset &&
            services_.wait && services_.close && services_.waitCondition && services_.wakeCondition &&
            services_.clock && services_.lastError && services_.cursor;
    }

    CallResult Error() const {
        return {false, services_.lastError ? services_.lastError(services_.context) : ERROR_INVALID_PARAMETER};
    }

    WaitResult Wait(HANDLE handle, std::uint32_t timeout) {
        const DWORD result = services_.wait(services_.context, handle, timeout);
        if (result == WAIT_OBJECT_0) {
            return {WaitKind::Signaled, 0};
        }
        if (result == WAIT_TIMEOUT) {
            return {WaitKind::Timeout, 0};
        }
        return {WaitKind::Failed, services_.lastError(services_.context)};
    }

    static DWORD WINAPI ThreadEntry(LPVOID context) {
        WinBackend *backend = static_cast<WinBackend *>(context);
        backend->entry_(backend->context_);
        return 0;
    }

    static HANDLE CreateControlHandle(void *) {
        return CreateEventW(nullptr, TRUE, FALSE, nullptr);
    }

    static HANDLE CreateWorkerHandle(void *, LPTHREAD_START_ROUTINE entry, LPVOID context) {
        return CreateThread(nullptr, 0, entry, context, 0, nullptr);
    }

    static BOOL SignalHandle(void *, HANDLE handle) {
        return SetEvent(handle);
    }

    static BOOL ResetHandle(void *, HANDLE handle) {
        return ResetEvent(handle);
    }

    static DWORD WaitHandle(void *, HANDLE handle, DWORD timeout) {
        return WaitForSingleObject(handle, timeout);
    }

    static BOOL CloseHandleService(void *, HANDLE handle) {
        return CloseHandle(handle);
    }

    static BOOL WaitConditionService(void *, PCONDITION_VARIABLE condition, PSRWLOCK lock, DWORD timeout) {
        return SleepConditionVariableSRW(condition, lock, timeout, 0);
    }

    static void WakeConditionService(void *, PCONDITION_VARIABLE condition) {
        WakeAllConditionVariable(condition);
    }

    static std::uint64_t Clock(void *) {
        return GetTickCount64();
    }

    static DWORD LastError(void *) {
        return GetLastError();
    }

    static CallResult SystemCursor(void *, Point &point, SampleReason) {
        POINT cursor{};
        if (!GetCursorPos(&cursor)) {
            return {false, GetLastError()};
        }
        static_assert(sizeof(LONG) == sizeof(std::int32_t), "Windows cursor coordinates are signed 32-bit");
        point.x = cursor.x;
        point.y = cursor.y;
        return {true, 0};
    }

    const Services services_;
    SRWLOCK lock_ = SRWLOCK_INIT;
    CONDITION_VARIABLE condition_ = CONDITION_VARIABLE_INIT;
    HANDLE control_ = nullptr;
    HANDLE thread_ = nullptr;
    void (*entry_)(void *) = nullptr;
    void *context_ = nullptr;
};

[[noreturn]] inline void FailMouseWorkerProcess() {
    // A void legacy shutdown callback must not return to FreeLibrary while an
    // owned worker may still execute client code. Do not run DLL detach cleanup.
    TerminateProcess(GetCurrentProcess(), EXIT_FAILURE);
    __fastfail(FAST_FAIL_FATAL_APP_EXIT);
}

} } // namespace cse::mouse

#endif
