#include "native_mouse_poller_win.h"
#include <atomic>
#include <initializer_list>
#include <cstdio>
#include <cstdlib>

static void Check(bool condition, int line) {
    if (!condition) {
        std::fprintf(stderr, "kernel-check:%d\n", line);
        std::abort();
    }
}
#define CHECK(condition) Check((condition), __LINE__)
using namespace cse::mouse;
enum class Fault { None, Event, Thread, Signal, Reset, Wait, Join, Close, Condition, Cursor };
constexpr DWORD InjectedError = 6789;
struct Context {
    WinBackend::Services kernel;
    std::atomic<Fault> fault{Fault::None};
    std::atomic<bool> hold{false};
    HANDLE entered = nullptr;
    HANDLE release = nullptr;
    HANDLE control = nullptr;
    int closes = 0;

    bool Fail(Fault kind) {
        Fault expected = kind;
        if (fault.compare_exchange_strong(expected, Fault::None)) {
            SetLastError(InjectedError);
            return true;
        }
        return false;
    }
};

static CallResult Cursor(void *opaque, Point &point, SampleReason reason) {
    Context &ctx = *static_cast<Context *>(opaque);
    if (reason == SampleReason::Poll && ctx.hold) {
        CHECK(SetEvent(ctx.entered));
        CHECK(WaitForSingleObject(ctx.release, 2000) == WAIT_OBJECT_0);
    }
    point = Point{};
    return ctx.Fail(Fault::Cursor) ? CallResult{false, InjectedError} : CallResult{true, 0};
}

static WinBackend::Services Bind(Context &ctx) {
    ctx.kernel = WinBackend::KernelServices(Cursor, &ctx);
    auto services = ctx.kernel;
    services.createControl = [](void *opaque) -> HANDLE {
        Context &c = *static_cast<Context *>(opaque);
        c.control = c.Fail(Fault::Event) ? nullptr : c.kernel.createControl(opaque);
        return c.control;
    };
    services.createThread = [](void *opaque, LPTHREAD_START_ROUTINE entry, LPVOID argument) -> HANDLE {
        Context &c = *static_cast<Context *>(opaque);
        return c.Fail(Fault::Thread) ? nullptr : c.kernel.createThread(opaque, entry, argument);
    };
    services.signal = [](void *opaque, HANDLE handle) -> BOOL {
        Context &c = *static_cast<Context *>(opaque);
        return c.Fail(Fault::Signal) ? FALSE : c.kernel.signal(opaque, handle);
    };
    services.reset = [](void *opaque, HANDLE handle) -> BOOL {
        Context &c = *static_cast<Context *>(opaque);
        return c.Fail(Fault::Reset) ? FALSE : c.kernel.reset(opaque, handle);
    };
    services.wait = [](void *opaque, HANDLE handle, DWORD timeout) -> DWORD {
        Context &c = *static_cast<Context *>(opaque);
        return c.Fail(handle == c.control ? Fault::Wait : Fault::Join) ? WAIT_FAILED :
            c.kernel.wait(opaque, handle, timeout);
    };
    services.close = [](void *opaque, HANDLE handle) -> BOOL {
        Context &c = *static_cast<Context *>(opaque);
        if (c.Fail(Fault::Close)) {
            return FALSE;
        }
        ++c.closes;
        return c.kernel.close(opaque, handle);
    };
    services.waitCondition = [](void *opaque, PCONDITION_VARIABLE condition, PSRWLOCK lock, DWORD timeout) -> BOOL {
        Context &c = *static_cast<Context *>(opaque);
        return c.Fail(Fault::Condition) ? FALSE : c.kernel.waitCondition(opaque, condition, lock, timeout);
    };
    return services;
}

template<class Predicate>
static void Await(Predicate predicate) {
    const ULONGLONG start = GetTickCount64();
    while (!predicate() && GetTickCount64() - start < 2000) {
        SwitchToThread();
    }
    CHECK(predicate());
}

int main() {
    for (Fault fault : {Fault::Event, Fault::Thread, Fault::Reset, Fault::Wait, Fault::Signal, Fault::Join,
        Fault::Close, Fault::Condition, Fault::Cursor}) {
        Context context;
        context.entered = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        context.release = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        CHECK(context.entered && context.release);
        WinBackend backend(Bind(context));
        Poller<WinBackend> poller(backend);
        const bool startupFault = fault == Fault::Event || fault == Fault::Thread ||
            fault == Fault::Reset || fault == Fault::Wait;
        if (startupFault) {
            context.fault = fault;
        }
        const bool started = poller.Start();
        if (startupFault) {
            (void)started;
            Await([&] { return poller.FirstFailure().kind != FailureKind::None; });
        } else {
            CHECK(started);
            if (fault == Fault::Signal) {
                context.fault = fault;
                CHECK(!poller.Publish(true, 1));
            } else if (fault == Fault::Cursor) {
                context.fault = fault;
                CHECK(!poller.Publish(true, 1));
            } else if (fault == Fault::Condition) {
                context.hold = true;
                CHECK(poller.Publish(true, 1));
                CHECK(WaitForSingleObject(context.entered, 2000) == WAIT_OBJECT_0);
                context.fault = fault;
                CHECK(!poller.Suspend(true, 20));
                CHECK(context.closes == 0);
                CHECK(SetEvent(context.release));
            } else {
                context.fault = fault;
                CHECK(!poller.Stop());
                CHECK(context.closes == 0);
            }
        }
        CHECK(poller.FirstFailure().error == InjectedError);
        CHECK(poller.Stop());
        CHECK(context.closes == (fault == Fault::Event ? 0 : fault == Fault::Thread ? 1 : 2));
        CHECK(CloseHandle(context.entered));
        CHECK(CloseHandle(context.release));
    }
    std::puts("kernel-faults=9 fake-cursor-only");
}
