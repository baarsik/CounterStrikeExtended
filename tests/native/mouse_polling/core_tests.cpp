#include "fake_backend.h"
#include <limits>

static std::atomic<int> checks{0};
void Check(bool condition, const char *expression, int line) {
    ++checks;
    if (!condition) {
        std::fprintf(stderr, "poller-check:%d:%s\n", line, expression);
        std::abort();
    }
}

using Poller = cse::mouse::Poller<FakeBackend>;
constexpr std::uint32_t TestDeadlineMs = 2000;

static cse::mouse::Delta AwaitDelta(Poller &poller, int x, int y) {
    cse::mouse::Delta total{};
    const auto start = std::chrono::steady_clock::now();
    do {
        cse::mouse::Delta delta{};
        CHECK(poller.Consume(delta));
        total.x += delta.x;
        total.y += delta.y;
        if (total.x == x && total.y == y) {
            return total;
        }
        std::this_thread::yield();
    } while (std::chrono::steady_clock::now() - start < std::chrono::seconds(2));
    CHECK(false);
    return total;
}

static void TestPairAndRecenter() {
    FakeBackend backend;
    Poller poller(backend);
    CHECK(poller.Start());
    CHECK(backend.pollReads == 0);
    CHECK(poller.Publish(true, 1, TestDeadlineMs));
    backend.SetCursor(10, 20);
    AwaitDelta(poller, 10, 20);
    backend.HoldPoll();
    backend.AwaitPoll();
    backend.SetCursor(15, 25);
    std::atomic<bool> barrierDone{false};
    std::thread owner([&] {
        CHECK(poller.BeginRecenter(TestDeadlineMs));
        barrierDone = true;
    });
    backend.AwaitOwnerWait();
    CHECK(!barrierDone);
    backend.ReleasePoll();
    owner.join();
    CHECK(barrierDone);
    backend.SetCursor(320, 240); // Owned fake warp, never an OS call.
    CHECK(poller.EndRecenter(true, 1));
    AwaitDelta(poller, 5, 5);
    cse::mouse::Delta empty{};
    CHECK(poller.Consume(empty) && empty.x == 0 && empty.y == 0);
    CHECK(poller.Stop(TestDeadlineMs));
    CHECK(backend.closes == 1);
    CHECK(poller.Stop(TestDeadlineMs));
    CHECK(backend.closes == 1);
}

static void TestSuppressedLateSample() {
    FakeBackend backend;
    Poller poller(backend);
    CHECK(poller.Start());
    CHECK(poller.Publish(false, 1, TestDeadlineMs));
    CHECK(backend.pollReads == 0 && backend.mainReads == 0);
    CHECK(poller.Publish(true, 1, TestDeadlineMs));
    backend.HoldPoll();
    backend.AwaitPoll();
    std::atomic<bool> suspended{false};
    std::thread owner([&] {
        CHECK(poller.Suspend(true, TestDeadlineMs));
        suspended = true;
    });
    backend.AwaitOwnerWait();
    CHECK(!suspended);
    backend.SetCursor(100, 200);
    backend.ReleasePoll();
    owner.join();
    CHECK(suspended);
    cse::mouse::Delta delta{};
    CHECK(poller.Consume(delta) && delta.x == 0 && delta.y == 0);
    const int reads = backend.pollReads;
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    CHECK(backend.pollReads == reads);
    CHECK(poller.Publish(true, 1, TestDeadlineMs));
    backend.SetCursor(103, 204);
    AwaitDelta(poller, 3, 4);
    CHECK(poller.Stop(TestDeadlineMs));
}

static void TestBoundedStop() {
    FakeBackend backend;
    Poller poller(backend);
    CHECK(poller.Start());
    CHECK(poller.Publish(true, 1, TestDeadlineMs));
    backend.HoldPoll();
    backend.AwaitPoll();
    CHECK(!poller.Stop(10));
    CHECK(backend.worker.joinable() && backend.closes == 0);
    CHECK(poller.LastFailure().kind == cse::mouse::FailureKind::JoinTimeout);
    backend.ReleasePoll();
    CHECK(poller.Stop(TestDeadlineMs));
    CHECK(backend.closes == 1);
}

static void TestFailures() {
    for (int stage = 0; stage < 2; ++stage) {
        FakeBackend backend;
        Poller poller(backend);
        backend.failEvent = stage == 0;
        backend.failThread = stage == 1;
        CHECK(!poller.Start());
        CHECK(!poller.Ready());
        CHECK(poller.Stop(TestDeadlineMs));
    }
    FakeBackend backend;
    Poller poller(backend);
    CHECK(poller.Start());
    backend.failRead = true;
    CHECK(!poller.Publish(true, 1, TestDeadlineMs));
    CHECK(poller.FirstFailure().kind == cse::mouse::FailureKind::ReadCursor);
    CHECK(poller.Stop(TestDeadlineMs));
}

template<class Predicate>
static void Await(Predicate predicate) {
    const auto start = std::chrono::steady_clock::now();
    while (!predicate() && std::chrono::steady_clock::now() - start < std::chrono::seconds(2)) {
        std::this_thread::yield();
    }
    CHECK(predicate());
}

static void TestWorkerFailures() {
    using cse::mouse::FailureKind;
    for (int stage = 0; stage < 5; ++stage) {
        FakeBackend backend;
        Poller poller(backend);
        if (stage == 0) {
            backend.failReset = true;
        } else if (stage == 1) {
            backend.failWait = true;
        }
        const bool started = poller.Start();
        if (stage < 2) {
            (void)started; // Worker may fail before or after Start observes readiness.
            Await([&] { return poller.FirstFailure().kind != FailureKind::None; });
            CHECK(poller.FirstFailure().kind == (stage == 0 ? FailureKind::ResetControl : FailureKind::WaitControl));
        } else {
            CHECK(started);
            CHECK(poller.Publish(true, 1, TestDeadlineMs));
            if (stage == 2) {
                backend.failSignal = true;
                CHECK(!poller.Publish(false, 1, TestDeadlineMs));
                CHECK(poller.FirstFailure().kind == FailureKind::SignalControl);
            } else if (stage == 3) {
                backend.failRead = true;
                Await([&] { return poller.FirstFailure().kind == FailureKind::ReadCursor; });
            } else {
                CHECK(poller.Publish(false, 1, TestDeadlineMs));
                backend.SetCursor(INT32_MIN, 0);
                CHECK(poller.Publish(true, 1, TestDeadlineMs));
                backend.SetCursor(INT32_MAX, 0);
                Await([&] { return poller.FirstFailure().kind == FailureKind::ArithmeticRange; });
            }
        }
        const auto first = poller.FirstFailure();
        CHECK(poller.Stop(TestDeadlineMs));
        CHECK(poller.FirstFailure().kind == first.kind && poller.FirstFailure().error == first.error);
    }
}

static void TestQuiescenceFailure() {
    using cse::mouse::FailureKind;
    for (int stage = 0; stage < 2; ++stage) {
        FakeBackend backend;
        Poller poller(backend);
        CHECK(poller.Start());
        CHECK(poller.Publish(true, 1, TestDeadlineMs));
        backend.HoldPoll();
        backend.AwaitPoll();
        backend.failCondition = stage == 1;
        CHECK(!poller.Suspend(true, 10));
        CHECK(poller.FirstFailure().kind == (stage == 0 ? FailureKind::QuiescenceTimeout :
            FailureKind::QuiescenceWait));
        CHECK(backend.worker.joinable() && backend.closes == 0);
        backend.ReleasePoll();
        CHECK(poller.Stop(TestDeadlineMs));
    }
}

static void TestStopFailuresAndCycles() {
    using cse::mouse::FailureKind;
    for (int stage = 0; stage < 3; ++stage) {
        FakeBackend backend;
        Poller poller(backend);
        CHECK(poller.Start());
        backend.failSignal = stage == 0;
        backend.failJoin = stage == 1;
        backend.failClose = stage == 2;
        CHECK(!poller.Stop(TestDeadlineMs));
        CHECK(backend.closes == 0);
        CHECK(poller.LastFailure().kind == (stage == 0 ? FailureKind::SignalControl :
            stage == 1 ? FailureKind::JoinWait : FailureKind::CloseHandles));
        CHECK(poller.Stop(TestDeadlineMs));
        CHECK(backend.closes == 1);
    }
    FakeBackend backend;
    Poller poller(backend);
    for (int cycle = 0; cycle < 20; ++cycle) {
        CHECK(poller.Start());
        CHECK(poller.Publish(false, cse::mouse::InfiniteWait)); // Initial raw input parks the worker.
        CHECK(poller.Publish(true, cse::mouse::InfiniteWait));
        CHECK(poller.Publish(true, 1)); // Timeout snapshot wakes an infinite wait.
        const int nextX = cycle * 3 + 3;
        const int nextY = cycle * 5 + 5;
        backend.SetCursor(nextX, nextY);
        AwaitDelta(poller, 3, 5);
        CHECK(poller.Publish(false, 1));
        cse::mouse::Delta delta{};
        CHECK(poller.Consume(delta) && delta.x == 0 && delta.y == 0);
        CHECK(poller.Stop(TestDeadlineMs));
    }
    CHECK(backend.closes == 20);
}

static void TestQueuedRecenterAndDiscard() {
    FakeBackend backend;
    Poller poller(backend);
    CHECK(poller.Start());
    CHECK(poller.Publish(true, cse::mouse::InfiniteWait));
    backend.SetCursor(10, 20);
    CHECK(poller.BeginRecenter());
    backend.SetCursor(320, 240);
    CHECK(poller.EndRecenter(true, cse::mouse::InfiniteWait));
    backend.SetCursor(323, 244);
    CHECK(poller.BeginRecenter());
    backend.SetCursor(320, 240);
    CHECK(poller.EndRecenter(true, cse::mouse::InfiniteWait));
    cse::mouse::Delta delta{};
    CHECK(poller.Consume(delta) && delta.x == 13 && delta.y == 24);
    backend.SetCursor(331, 257);
    CHECK(poller.BeginRecenter());
    backend.SetCursor(320, 240);
    CHECK(poller.EndRecenter(true, cse::mouse::InfiniteWait));
    CHECK(poller.Suspend(true));
    CHECK(poller.Consume(delta) && delta.x == 0 && delta.y == 0);
    CHECK(poller.Publish(true, 1));
    backend.failRead = true;
    Await([&] { return poller.FirstFailure().kind == cse::mouse::FailureKind::ReadCursor; });
    backend.failJoin = true;
    CHECK(!poller.Stop());
    CHECK(poller.FirstFailure().kind == cse::mouse::FailureKind::ReadCursor);
    CHECK(poller.LastFailure().kind == cse::mouse::FailureKind::JoinWait);
    CHECK(poller.Stop());
}

static void TestTimeoutConversion() {
    std::uint32_t timeout = 77;
    CHECK(cse::mouse::TryLegacyTimeout(10.75f, timeout) && timeout == 10);
    CHECK(cse::mouse::TryLegacyTimeout(0.0f, timeout) && timeout == 0);
    CHECK(cse::mouse::TryLegacyTimeout(-1.0f, timeout) && timeout == cse::mouse::InfiniteWait);
    CHECK(cse::mouse::TryLegacyTimeout(-2.75f, timeout) && timeout == 0xfffffffeU);
    CHECK(cse::mouse::TryLegacyTimeout(-0.75f, timeout) && timeout == 0);
    CHECK(cse::mouse::TryLegacyTimeout(-2147483648.0f, timeout) && timeout == 0x80000000U);
    CHECK(cse::mouse::TryLegacyTimeout(2147483520.0f, timeout) && timeout == 2147483520U);
    timeout = 77;
    CHECK(!cse::mouse::TryLegacyTimeout(std::numeric_limits<float>::quiet_NaN(), timeout) && timeout == 77);
    CHECK(!cse::mouse::TryLegacyTimeout(std::numeric_limits<float>::infinity(), timeout) && timeout == 77);
    CHECK(!cse::mouse::TryLegacyTimeout(2147483648.0f, timeout) && timeout == 77);
}

int main() {
    TestTimeoutConversion();
    TestPairAndRecenter();
    TestSuppressedLateSample();
    TestBoundedStop();
    TestFailures();
    TestWorkerFailures();
    TestQuiescenceFailure();
    TestStopFailuresAndCycles();
    TestQueuedRecenterAndDiscard();
    std::printf("checks=%d owned-fake-worker-only\n", checks.load());
}
