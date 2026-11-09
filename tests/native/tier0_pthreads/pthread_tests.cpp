#include "tier0/threadtools.h"
#include <chrono>
#include <condition_variable>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <mutex>
#include <type_traits>

namespace {
constexpr auto kGateTimeout = std::chrono::seconds(5);
constexpr unsigned int kSimpleResult = 41;
constexpr int kWorkerResult = 67;
constexpr int kRepeatedCycles = 8;

void Require(bool condition, const char* description) {
    if (!condition) {
        std::fprintf(stderr, "FAILED: %s\n", description);
        std::abort();
    }
}

class Gate {
public:
    void Signal() {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_signaled = true;
        m_condition.notify_all();
    }

    void Wait() {
        std::unique_lock<std::mutex> lock(m_mutex);
        Require(m_condition.wait_for(lock, kGateTimeout, [this] { return m_signaled; }), "gate timeout");
    }

    void Reset() {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_signaled = false;
    }

private:
    std::mutex m_mutex;
    std::condition_variable m_condition;
    bool m_signaled = false;
};

void CurrentIdentity() {
    static_assert(std::is_same<decltype(ThreadGetCurrentId()), pthread_t>::value,
        "The existing public pthread identity must remain unchanged");
    Require(pthread_equal(ThreadGetCurrentId(), pthread_self()) != 0, "public native identity");
    Require(ThreadInMainThread(), "initial main-thread ownership");
    Require(CThread::GetCurrentCThread() == nullptr, "main has no CThread TLS binding");
    const auto bits = reinterpret_cast<std::uintptr_t>(ThreadGetCurrentId());
    Require(bits > std::numeric_limits<std::uint32_t>::max(), "actual Apple pthread has high address bits");
    std::printf("pthread_bytes=%zu current_bits=0x%llx\n", sizeof(pthread_t),
        static_cast<unsigned long long>(bits));
}

struct SimpleContext {
    Gate ready;
    Gate changeOwner;
    Gate changedOwner;
    Gate checkRestoredOwner;
    Gate checkedRestoredOwner;
    Gate finish;
    pthread_t identity = nullptr;
    bool initiallyMain = true;
    bool declaredMain = false;
    bool restoredMain = true;
    bool tlsEmpty = false;
};

unsigned int SimpleWorker(void* parameter) {
    auto& context = *static_cast<SimpleContext*>(parameter);
    context.identity = ThreadGetCurrentId();
    context.initiallyMain = ThreadInMainThread();
    context.tlsEmpty = CThread::GetCurrentCThread() == nullptr;
    context.ready.Signal();
    context.changeOwner.Wait();
    DeclareCurrentThreadIsMainThread();
    context.declaredMain = ThreadInMainThread();
    context.changedOwner.Signal();
    context.checkRestoredOwner.Wait();
    context.restoredMain = ThreadInMainThread();
    context.checkedRestoredOwner.Signal();
    context.finish.Wait();
    return kSimpleResult;
}

void OwnershipTransfer() {
    SimpleContext context;
    const auto thread = CreateSimpleThread(SimpleWorker, &context, 0);
    Require(thread != nullptr, "create simple worker");
    context.ready.Wait();
    Require(!context.initiallyMain && context.tlsEmpty, "simple worker is initially unowned and has no CThread TLS");
    Require(pthread_equal(context.identity, pthread_self()) == 0, "distinct live pthread identities");
    Require(ThreadInMainThread(), "main retains ownership before declaration");
    context.changeOwner.Signal();
    context.changedOwner.Wait();
    Require(context.declaredMain && !ThreadInMainThread(), "ownership follows worker declaration");
    DeclareCurrentThreadIsMainThread();
    Require(ThreadInMainThread(), "main ownership restored");
    context.checkRestoredOwner.Signal();
    context.checkedRestoredOwner.Wait();
    Require(!context.restoredMain, "previous owner observes restoration");
    context.finish.Signal();
    void* result = nullptr;
    Require(pthread_join(static_cast<pthread_t>(thread), &result) == 0, "join owned simple worker exactly once");
    Require(reinterpret_cast<std::uintptr_t>(result) == kSimpleResult, "simple result preserved");
}

class Worker final : public CThread {
public:
    Gate running;
    Gate finish;
    pthread_t identity = nullptr;
    bool tlsCorrect = false;
    bool recognizedAsMain = true;
    int exits = 0;

private:
    int Run() override {
        identity = ThreadGetCurrentId();
        tlsCorrect = GetCurrentCThread() == this;
        recognizedAsMain = ThreadInMainThread();
        running.Signal();
        finish.Wait();
        return kWorkerResult;
    }

    void OnExit() override {
        ++exits;
    }
};

void WorkerLifecycle(int cycles) {
    Worker worker;
    worker.SetName("owned pthread worker");
    for (int cycle = 0; cycle < cycles; ++cycle) {
        worker.running.Reset();
        worker.finish.Reset();
        Require(worker.Start(0), "actual CThread startup handshake");
        worker.running.Wait();
        Require(worker.IsAlive() && worker.tlsCorrect && !worker.recognizedAsMain, "worker identity and TLS");
        Require(pthread_equal(worker.identity, worker.m_threadId) != 0, "CThread retains full native pthread");
        const auto bits = reinterpret_cast<std::uintptr_t>(worker.identity);
        Require(bits > std::numeric_limits<std::uint32_t>::max(), "actual worker pthread has high address bits");
        std::printf("worker_bits=0x%llx cycle=%d\n", static_cast<unsigned long long>(bits), cycle);
        Require(std::strcmp(worker.GetName(), "owned pthread worker") == 0, "worker name preserved");
        // Capture while Run is gated; ThreadProc clears the field on exit. Join the owned pthread once.
        const pthread_t owned = worker.m_threadId;
        worker.finish.Signal();
        Require(pthread_join(owned, nullptr) == 0, "native worker lifetime joined");
        Require(!worker.IsAlive() && worker.Join(0), "completed CThread state");
        Require(worker.GetResult() == kWorkerResult && worker.exits == cycle + 1, "result and one OnExit per run");
        Require(CThread::GetCurrentCThread() == nullptr && ThreadInMainThread(), "caller TLS and ownership unchanged");
    }
}
}

extern "C" void Msg(const char* format, ...) {
    va_list arguments;
    va_start(arguments, format);
    std::vfprintf(stderr, format, arguments);
    va_end(arguments);
}

extern "C" void Error(const char* format, ...) {
    va_list arguments;
    va_start(arguments, format);
    std::vfprintf(stderr, format, arguments);
    va_end(arguments);
    std::abort();
}

int main(int argc, const char** argv) {
    Require(argc == 2, "one scenario required");
    if (std::strcmp(argv[1], "current") == 0) {
        CurrentIdentity();
    } else if (std::strcmp(argv[1], "ownership") == 0) {
        OwnershipTransfer();
    } else if (std::strcmp(argv[1], "worker") == 0) {
        WorkerLifecycle(1);
    } else if (std::strcmp(argv[1], "repeated") == 0) {
        WorkerLifecycle(kRepeatedCycles);
    } else {
        Require(false, "unknown scenario");
    }
    std::puts("actual_tier0_pthread_scenario_passed=1");
    return 0;
}
