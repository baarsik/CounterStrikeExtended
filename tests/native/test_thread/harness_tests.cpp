#include <pthread.h>
#include <atomic>
#include <cerrno>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <mutex>
#include <thread>

extern "C" {
void Test_RunTest(void (*function)(void*), void* argument);
void Test_RunFrame();
void TestThread_Yield();
void Test_SetFailed();
bool Test_HasFailed();
bool Test_HasFinished();
bool Test_IsActive();
void Test_TerminateThread();
}

namespace {
std::atomic<int> createFailures{0};
std::atomic<int> joinFailures{0};
std::atomic<int> createCalls{0};
std::atomic<int> joinCalls{0};
std::atomic<int> successfulJoins{0};
const char* joinMarker = nullptr;
std::atomic<int> spuriousWakes{0};
struct TlsContext {
    std::mutex mutex;
    std::condition_variable condition;
    bool entered = false;
    bool joining = false;
    bool release = false;
    bool complete = false;
};
TlsContext* joinContext = nullptr;
pthread_key_t tlsKey{};

void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "FAILED: %s\n", message);
        std::abort();
    }
}

struct Context {
    std::atomic<int> stage{0};
    std::atomic<bool> argumentMatched{false};
    void* expected = nullptr;
};

void Empty(void* argument) {
    auto* context = static_cast<Context*>(argument);
    context->argumentMatched = argument == context->expected;
    context->stage = 1;
}

void ThreeStages(void* argument) {
    auto* context = static_cast<Context*>(argument);
    context->argumentMatched = argument == context->expected;
    context->stage = 1;
    TestThread_Yield();
    context->stage = 2;
    TestThread_Yield();
    context->stage = 3;
}

void Failed(void* argument) {
    Test_SetFailed();
    Empty(argument);
}

void InvalidWorkerOperations(void* argument) {
    Test_RunFrame();
    Test_TerminateThread();
    Test_RunTest(Empty, argument);
    Empty(argument);
}

void Initial() {
    Require(!Test_IsActive() && !Test_HasFinished() && !Test_HasFailed(), "initial exported state");
    Test_RunFrame();
    Test_TerminateThread();
    Require(!Test_IsActive() && !Test_HasFinished() && !Test_HasFailed(), "idle no-op state");
}

void Lifecycle() {
    for (int cycle = 0; cycle < 100; ++cycle) {
        Context context;
        context.expected = &context;
        Require(reinterpret_cast<std::uintptr_t>(&context) > std::numeric_limits<std::uint32_t>::max(),
            "actual argument address is above 4 GiB");
        Test_RunTest(ThreeStages, &context);
        Require(Test_IsActive() && !Test_HasFinished() && context.stage == 0, "initial worker parks before callback");
        Test_RunFrame();
        Require(context.stage == 1 && context.argumentMatched && !Test_HasFinished(),
            "first grant reaches first yield");
        Test_RunFrame();
        Require(context.stage == 2 && !Test_HasFinished(), "second grant reaches corresponding second yield");
        Test_RunFrame();
        Require(context.stage == 3 && Test_HasFinished() && Test_IsActive(), "completion and sticky active state");
        Test_TerminateThread();
        Require(successfulJoins == cycle + 1, "completed join handle retained and reaped once");
    }
    Require(!Test_HasFailed(), "valid repeated lifecycle never marks failure");
}

void Stop() {
    Context context;
    context.expected = &context;
    Test_RunTest(ThreeStages, &context);
    Test_TerminateThread();
    Require(context.stage == 0 && Test_IsActive() && Test_HasFinished(), "stop at initial park without callback");
    Test_RunTest(ThreeStages, &context);
    Test_RunFrame();
    Require(context.stage == 1, "callback at first yield");
    Test_TerminateThread();
    Require(context.stage == 1 && Test_HasFinished() && successfulJoins == 2, "cooperative parked stop joins once");
    Test_TerminateThread();
    Require(successfulJoins == 2 && !Test_HasFailed(), "repeated stop is an owned no-op");
}

void Failures() {
    createFailures = 1;
    Context context;
    context.expected = &context;
    Test_RunTest(Empty, &context);
    Require(Test_HasFailed() && !Test_IsActive() && !Test_HasFinished() && context.stage == 0,
        "startup failure closes state without fabricated active worker");
    Require(joinCalls == 0, "failed create owns no join handle");
    Test_RunTest(Failed, &context);
    Test_RunFrame();
    Require(Test_HasFinished() && context.argumentMatched && Test_HasFailed(), "callback failure propagation");
    joinFailures = 1;
    Test_TerminateThread();
    Require(successfulJoins == 0 && Test_HasFailed(), "failed join retains ownership");
    Test_TerminateThread();
    Require(successfulJoins == 1, "failed join can be retried on same owned thread");
    Test_RunTest(Empty, &context);
    Test_RunFrame();
    Test_TerminateThread();
    Require(Test_HasFailed() && Test_IsActive() && Test_HasFinished() && successfulJoins == 2,
        "failed flag stays sticky through restart");
    createFailures = 1;
    Test_RunTest(Empty, &context);
    Require(Test_IsActive() && Test_HasFinished() && successfulJoins == 2,
        "later startup failure preserves sticky active and finished state");
}

void InvalidOperations() {
    Test_RunTest(nullptr, nullptr);
    Require(Test_HasFailed() && !Test_IsActive() && !Test_HasFinished(), "null callback is rejected before startup");
    Context context;
    context.expected = &context;
    Test_RunTest(InvalidWorkerOperations, &context);
    Test_RunFrame();
    Require(Test_HasFailed() && Test_HasFinished() && context.stage == 1,
        "worker reentrancy is rejected without deadlock");
    Test_TerminateThread();
    Context original;
    original.expected = &original;
    Context replacement;
    replacement.expected = &replacement;
    Test_RunTest(ThreeStages, &original);
    Test_RunTest(Empty, &replacement);
    TestThread_Yield();
    std::thread outsider([] {
        Test_RunFrame();
        Test_TerminateThread();
    });
    outsider.join();
    Test_RunFrame();
    Require(original.stage == 1 && original.argumentMatched && replacement.stage == 0,
        "invalid owner/reentrant calls cannot replace an active callback or consume its grant");
    Test_TerminateThread();
}

void ConcurrentQueries() {
    Context context;
    context.expected = &context;
    std::atomic<bool> stop{false};
    std::thread observer([&] {
        while (!stop.load()) {
            static_cast<void>(Test_IsActive());
            static_cast<void>(Test_HasFinished());
            static_cast<void>(Test_HasFailed());
            Test_SetFailed();
        }
    });
    for (int cycle = 0; cycle < 100; ++cycle) {
        Test_RunTest(ThreeStages, &context);
        Test_RunFrame();
        Test_RunFrame();
        Test_RunFrame();
        Test_TerminateThread();
    }
    stop = true;
    observer.join();
    Require(Test_IsActive() && Test_HasFinished() && Test_HasFailed(), "concurrent status and failure reporting");
}

void TlsDestructor(void* argument) {
    auto* context = static_cast<TlsContext*>(argument);
    {
        std::unique_lock<std::mutex> lock(context->mutex);
        context->entered = true;
        context->condition.notify_all();
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
        Require(context->condition.wait_until(lock, deadline, [&] { return context->release; }),
            "TLS destructor release deadline");
    }
    Test_SetFailed();
    {
        std::lock_guard<std::mutex> lock(context->mutex);
        context->complete = true;
    }
}

void TlsCallback(void* argument) {
    Require(pthread_setspecific(tlsKey, argument) == 0, "owned worker TLS value");
}

void TlsJoin() {
    TlsContext context;
    joinContext = &context;
    Require(pthread_key_create(&tlsKey, TlsDestructor) == 0, "owned TLS key");
    std::thread releaser([&] {
        std::unique_lock<std::mutex> lock(context.mutex);
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
        Require(context.condition.wait_until(lock, deadline, [&] { return context.joining && context.entered; }),
            "join and TLS entry predicate deadline");
        context.release = true;
        context.condition.notify_all();
    });
    Test_RunTest(TlsCallback, &context);
    Test_RunFrame();
    Test_TerminateThread();
    releaser.join();
    Require(context.complete && Test_HasFailed() && successfulJoins == 1,
        "join releases harness mutex while worker TLS reports failure");
    Require(pthread_key_delete(tlsKey) == 0, "owned TLS key released after join");
    joinContext = nullptr;
}

void Spurious() {
    for (int cycle = 0; cycle < 50; ++cycle) {
        Context context;
        context.expected = &context;
        spuriousWakes = 6;
        Test_RunTest(ThreeStages, &context);
        Test_RunFrame();
        Require(context.stage == 1, "initial spurious wake does not consume a callback grant");
        spuriousWakes = 6;
        Test_RunFrame();
        Require(context.stage == 2, "spurious wake preserves matching yield predicate");
        spuriousWakes = 6;
        Test_RunFrame();
        Require(context.stage == 3 && Test_HasFinished(), "spurious wake preserves completion");
        Test_TerminateThread();
    }
    Require(!Test_HasFailed(), "spurious wake is not an API failure");
}

Context destructionContext;

void Destruction() {
    Context* context = &destructionContext;
    context->expected = context;
    // Static trivial callback storage outlives the global harness join.
    Test_RunTest(ThreeStages, context);
    Test_RunFrame();
    Require(context->stage == 1 && !Test_HasFinished(), "global destruction begins with worker parked");
}
}

extern "C" int CseTestFixtureCreate(pthread_t* thread, const pthread_attr_t* attributes,
    void* (*function)(void*), void* argument) {
    ++createCalls;
    if (createFailures.exchange(0) != 0) {
        return EAGAIN;
    }
    return pthread_create(thread, attributes, function, argument);
}

extern "C" int CseTestFixtureJoin(pthread_t thread, void** value) {
    ++joinCalls;
    if (joinFailures.exchange(0) != 0) {
        return EINVAL;
    }
    if (joinContext) {
        std::lock_guard<std::mutex> lock(joinContext->mutex);
        joinContext->joining = true;
        joinContext->condition.notify_all();
    }
    const int result = pthread_join(thread, value);
    if (result == 0) {
        ++successfulJoins;
        if (joinMarker) {
            FILE* file = std::fopen(joinMarker, "ab");
            Require(file != nullptr && std::fputc('J', file) == 'J' && std::fclose(file) == 0,
                "actual completed join marker");
        }
    }
    return result;
}

extern "C" int CseTestFixtureConditionWait(pthread_cond_t* condition, pthread_mutex_t* mutex) {
    int remaining = spuriousWakes.load();
    while (remaining > 0) {
        if (spuriousWakes.compare_exchange_weak(remaining, remaining - 1)) {
            return 0;
        }
    }
    return pthread_cond_wait(condition, mutex);
}

int main(int argc, const char** argv) {
    Require(argc == 3, "scenario and owned join-marker path required");
    joinMarker = argv[2];
    if (std::strcmp(argv[1], "initial") == 0) {
        Initial();
    } else if (std::strcmp(argv[1], "lifecycle") == 0) {
        Lifecycle();
    } else if (std::strcmp(argv[1], "stop") == 0) {
        Stop();
    } else if (std::strcmp(argv[1], "failure") == 0) {
        Failures();
    } else if (std::strcmp(argv[1], "invalid") == 0) {
        InvalidOperations();
    } else if (std::strcmp(argv[1], "tls") == 0) {
        TlsJoin();
    } else if (std::strcmp(argv[1], "spurious") == 0) {
        Spurious();
    } else if (std::strcmp(argv[1], "queries") == 0) {
        ConcurrentQueries();
    } else {
        Require(std::strcmp(argv[1], "destruction") == 0, "known scenario");
        Destruction();
    }
    std::printf("test_harness_body_passed=1 create_calls=%d successful_joins_before_exit=%d\n",
        createCalls.load(), successfulJoins.load());
    return 0;
}
