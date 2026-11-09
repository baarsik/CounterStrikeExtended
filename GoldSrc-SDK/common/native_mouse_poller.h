#pragma once

#include <cmath>
#include <cstdint>
#include <limits>

namespace cse { namespace mouse {

constexpr std::uint32_t InfiniteWait = 0xffffffffU;
constexpr std::uint32_t ShutdownDeadlineMs = 5000;
constexpr std::uint32_t LegacyDefaultIntervalMs = 10;

struct Point {
    std::int32_t x = 0;
    std::int32_t y = 0;
};

struct Delta {
    int x = 0;
    int y = 0;
};

struct CallResult {
    bool ok;
    std::uint32_t error;
};

enum class WaitKind { Signaled, Timeout, Failed };
struct WaitResult {
    WaitKind kind;
    std::uint32_t error;
};

enum class SampleReason { Poll, Rebase, BeforeWarp, AfterWarp };
enum class FailureKind {
    None, CreateControl, CreateThread, SignalControl, ResetControl, WaitControl,
    ReadCursor, ArithmeticRange, QuiescenceTimeout, QuiescenceWait, JoinTimeout, JoinWait, CloseHandles
};

struct Failure {
    FailureKind kind = FailureKind::None;
    std::uint32_t error = 0;
};

inline bool TryLegacyTimeout(float value, std::uint32_t &timeout) {
    constexpr double lowerExclusive = static_cast<double>((std::numeric_limits<std::int32_t>::min)()) - 1.0;
    constexpr double upperExclusive = static_cast<double>((std::numeric_limits<std::int32_t>::max)()) + 1.0;
    if (!std::isfinite(value) || static_cast<double>(value) <= lowerExclusive ||
        static_cast<double>(value) >= upperExclusive) {
        return false;
    }
    timeout = static_cast<std::uint32_t>(static_cast<std::int32_t>(value));
    return true;
}

// Backend owns synchronization and explicit event/thread handles. Production
// instances have no cleanup destructor: callers must Stop before module unload.
template<class Backend>
class Poller {
public:
    constexpr explicit Poller(Backend &backend) : backend_(backend) {}
    Poller(const Poller &) = delete;
    Poller &operator=(const Poller &) = delete;

    bool Start() {
        {
            Guard guard(backend_);
            if (started_) {
                return first_.kind == FailureKind::None;
            }
            stopping_ = false;
            enabled_ = false;
            baselineValid_ = false;
            inFlight_ = false;
            pendingX_ = 0;
            pendingY_ = 0;
            ++generation_;
            first_ = Failure{};
            last_ = Failure{};
        }
        const CallResult event = backend_.CreateControl();
        if (!event.ok) {
            Guard guard(backend_);
            Fail(FailureKind::CreateControl, event.error);
            return false;
        }
        {
            Guard guard(backend_);
            started_ = true;
        }
        const CallResult thread = backend_.StartThread(&Entry, this);
        if (!thread.ok) {
            Guard guard(backend_);
            started_ = false;
            Fail(FailureKind::CreateThread, thread.error);
            return false;
        }
        return Ready();
    }

    bool Ready() {
        Guard guard(backend_);
        return started_ && !stopping_ && first_.kind == FailureKind::None;
    }

    Failure FirstFailure() {
        Guard guard(backend_);
        return first_;
    }

    Failure LastFailure() {
        Guard guard(backend_);
        return last_;
    }

    bool Publish(bool allowed, std::uint32_t timeout, std::uint32_t deadline = ShutdownDeadlineMs) {
        {
            Guard guard(backend_);
            if (!Usable()) {
                return false;
            }
            if (enabled_ == allowed) {
                if (timeout_ != timeout) {
                    timeout_ = timeout;
                    return Signal();
                }
                return true;
            }
        }
        if (!Suspend(true, deadline)) {
            return false;
        }
        if (!allowed) {
            Guard guard(backend_);
            timeout_ = timeout;
            return true;
        }
        Point point;
        const CallResult read = backend_.ReadCursor(point, SampleReason::Rebase);
        Guard guard(backend_);
        if (!read.ok) {
            Fail(FailureKind::ReadCursor, read.error);
            return false;
        }
        if (!Usable()) {
            return false;
        }
        baseline_ = point;
        baselineValid_ = true;
        enabled_ = true;
        timeout_ = timeout;
        return Signal();
    }

    bool Suspend(bool discard, std::uint32_t deadline = ShutdownDeadlineMs) {
        Guard guard(backend_);
        enabled_ = false;
        ++generation_;
        if (discard) {
            pendingX_ = 0;
            pendingY_ = 0;
            baselineValid_ = false;
        }
        if (!started_) {
            return first_.kind == FailureKind::None;
        }
        const bool signaled = Signal();
        const std::uint64_t start = backend_.NowMs();
        while (inFlight_) {
            const std::uint64_t elapsed = backend_.NowMs() - start;
            if (elapsed >= deadline) {
                Fail(FailureKind::QuiescenceTimeout, 0);
                return false;
            }
            const WaitResult wait = backend_.WaitCondition(deadline - static_cast<std::uint32_t>(elapsed));
            if (wait.kind == WaitKind::Failed) {
                Fail(FailureKind::QuiescenceWait, wait.error);
                return false;
            }
        }
        return signaled && first_.kind == FailureKind::None;
    }

    bool BeginRecenter(std::uint32_t deadline = ShutdownDeadlineMs) {
        if (!Suspend(false, deadline)) {
            return false;
        }
        Point point;
        const CallResult read = backend_.ReadCursor(point, SampleReason::BeforeWarp);
        Guard guard(backend_);
        if (!read.ok) {
            Fail(FailureKind::ReadCursor, read.error);
            return false;
        }
        return Add(point);
    }

    bool EndRecenter(bool allowed, std::uint32_t timeout) {
        Point point;
        const CallResult read = backend_.ReadCursor(point, SampleReason::AfterWarp);
        Guard guard(backend_);
        if (!read.ok) {
            Fail(FailureKind::ReadCursor, read.error);
            return false;
        }
        if (!Usable()) {
            return false;
        }
        baseline_ = point;
        baselineValid_ = allowed;
        enabled_ = allowed;
        timeout_ = timeout;
        if (!allowed) {
            pendingX_ = 0;
            pendingY_ = 0;
        }
        return Signal();
    }

    bool Consume(Delta &delta) {
        Guard guard(backend_);
        if (!Usable()) {
            return false;
        }
        delta.x = static_cast<int>(pendingX_);
        delta.y = static_cast<int>(pendingY_);
        pendingX_ = 0;
        pendingY_ = 0;
        return true;
    }

    bool Stop(std::uint32_t deadline = ShutdownDeadlineMs) {
        bool started = false;
        {
            Guard guard(backend_);
            stopping_ = true;
            enabled_ = false;
            baselineValid_ = false;
            pendingX_ = 0;
            pendingY_ = 0;
            ++generation_;
            started = started_;
            if (started && !Signal()) {
                return false;
            }
        }
        if (started) {
            const WaitResult joined = backend_.Join(deadline);
            if (joined.kind != WaitKind::Signaled) {
                Guard guard(backend_);
                Fail(joined.kind == WaitKind::Timeout ? FailureKind::JoinTimeout : FailureKind::JoinWait, joined.error);
                return false;
            }
        }
        const CallResult closed = backend_.CloseHandles();
        Guard guard(backend_);
        if (!closed.ok) {
            Fail(FailureKind::CloseHandles, closed.error);
            return false;
        }
        started_ = false;
        inFlight_ = false;
        return true;
    }

private:
    class Guard {
    public:
        explicit Guard(Backend &backend) : backend_(backend) {
            backend_.Lock();
        }
        ~Guard() {
            backend_.Unlock();
        }
        Guard(const Guard &) = delete;
        Guard &operator=(const Guard &) = delete;
    private:
        Backend &backend_;
    };

    bool Usable() const {
        return started_ && !stopping_ && first_.kind == FailureKind::None;
    }

    void Fail(FailureKind kind, std::uint32_t error) {
        last_ = Failure{kind, error};
        if (first_.kind == FailureKind::None) {
            first_ = last_;
        }
        enabled_ = false;
        baselineValid_ = false;
        pendingX_ = 0;
        pendingY_ = 0;
    }

    bool Signal() {
        const CallResult result = backend_.SignalControl();
        if (!result.ok) {
            Fail(FailureKind::SignalControl, result.error);
        }
        return result.ok;
    }

    bool Add(const Point &point) {
        if (!Usable()) {
            return false;
        }
        if (baselineValid_) {
            const std::int64_t x = pendingX_ + static_cast<std::int64_t>(point.x) - baseline_.x;
            const std::int64_t y = pendingY_ + static_cast<std::int64_t>(point.y) - baseline_.y;
            constexpr std::int64_t minimum = (std::numeric_limits<std::int32_t>::min)();
            constexpr std::int64_t maximum = (std::numeric_limits<std::int32_t>::max)();
            if (x < minimum || x > maximum || y < minimum || y > maximum) {
                Fail(FailureKind::ArithmeticRange, 0);
                return false;
            }
            pendingX_ = x;
            pendingY_ = y;
        }
        baseline_ = point;
        baselineValid_ = true;
        return true;
    }

    static void Entry(void *context) {
        static_cast<Poller *>(context)->Run();
    }

    void Run() {
        for (;;) {
            std::uint32_t timeout = InfiniteWait;
            {
                Guard guard(backend_);
                if (stopping_ || first_.kind != FailureKind::None) {
                    return;
                }
                const CallResult reset = backend_.ResetControl();
                if (!reset.ok) {
                    Fail(FailureKind::ResetControl, reset.error);
                    return;
                }
                timeout = enabled_ ? timeout_ : InfiniteWait;
            }
            const WaitResult waited = backend_.WaitControl(timeout);
            if (waited.kind == WaitKind::Failed) {
                Guard guard(backend_);
                Fail(FailureKind::WaitControl, waited.error);
                return;
            }
            if (waited.kind == WaitKind::Signaled) {
                continue;
            }
            std::uint64_t generation = 0;
            {
                Guard guard(backend_);
                if (stopping_ || !enabled_ || !baselineValid_ || first_.kind != FailureKind::None) {
                    continue;
                }
                generation = generation_;
                inFlight_ = true;
            }
            Point point;
            const CallResult read = backend_.ReadCursor(point, SampleReason::Poll);
            {
                Guard guard(backend_);
                inFlight_ = false;
                if (!read.ok) {
                    Fail(FailureKind::ReadCursor, read.error);
                } else if (!stopping_ && enabled_ && generation == generation_) {
                    Add(point);
                }
                backend_.WakeCondition();
                if (first_.kind != FailureKind::None) {
                    return;
                }
            }
        }
    }

    Backend &backend_;
    bool started_ = false;
    bool stopping_ = false;
    bool enabled_ = false;
    bool baselineValid_ = false;
    bool inFlight_ = false;
    std::uint64_t generation_ = 0;
    std::uint32_t timeout_ = LegacyDefaultIntervalMs;
    Point baseline_;
    std::int64_t pendingX_ = 0;
    std::int64_t pendingY_ = 0;
    Failure first_;
    Failure last_;
};

} } // namespace cse::mouse
