#include "VGui1AppPlatform.h"

#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <limits>

namespace cse_vgui1_app {
namespace {
struct Data {
    State state;
    Failure failure;
    ServicesV1 services;
    std::uint32_t active;
    std::uint32_t borrowers;
    std::uint64_t generation;
    std::uint64_t nextIdentifier;
    std::uint64_t identifiers[MaximumBorrowers];
};

// Constant/zero initialization only. No late destructor may call an unloaded
// service. Root must explicitly Close and honor Busy/ReleaseFailed before unload.
Data data{};
std::atomic_flag locked = ATOMIC_FLAG_INIT;

class Lock {
public:
    Lock() noexcept {
        while (locked.test_and_set(std::memory_order_acquire)) {
        }
    }
    ~Lock() {
        locked.clear(std::memory_order_release);
    }
};

bool Complete(const ServicesV1& value) noexcept {
    return value.structBytes == sizeof(ServicesV1) && value.version == ServicesVersion && value.context
        && value.ownerThread && value.retainContext && value.releaseContext && value.pumpEvents
        && value.applyArena && value.acquireClipboard && value.releaseClipboard && value.writeClipboard
        && value.readSetting && value.writeString && value.writeInteger;
}

bool Pin(ServicesV1& output, bool allowClosing) noexcept {
    Lock guard;
    if (data.state != State::Bound && !(allowClosing && data.state == State::Closing)) {
        return false;
    }
    if (data.active == (std::numeric_limits<std::uint32_t>::max)()) {
        return false;
    }
    ++data.active;
    output = data.services;
    return true;
}

void Unpin() noexcept {
    Lock guard;
    --data.active;
}
}

[[noreturn]] void InvalidLegacyUse(const char* operation, const char* reason) noexcept {
    std::fprintf(stderr, "VGUI1 App platform %s: %s\n", operation, reason);
    std::fflush(stderr);
    std::abort();
}

Diagnostics Inspect() noexcept {
    Lock guard;
    return {data.state, data.failure, data.active, data.borrowers, data.generation};
}

void RecordFailure(Failure failure) noexcept {
    Lock guard;
    data.failure = failure;
}

Status Bind(const ServicesV1& services) noexcept {
    if (!Complete(services)) {
        return Status::InvalidArgument;
    }
    if (!services.ownerThread(services.context)) {
        return Status::WrongThread;
    }
    {
        Lock guard;
        if (data.state == State::Binding || data.state == State::Bound || data.state == State::Closing
            || data.active != 0) {
            return Status::AlreadyBound;
        }
        if (data.generation == (std::numeric_limits<std::uint64_t>::max)()) {
            return Status::Exhausted;
        }
        // Reserve before the external retain callback, which may reenter.
        data.state = State::Binding;
        data.active = 1;
    }
    const bool retained = services.retainContext(services.context);
    Lock guard;
    data.active = 0;
    if (!retained) {
        data.state = State::Closed;
        return Status::ReleaseFailed;
    }
    data.services = services;
    data.failure = Failure::None;
    data.borrowers = 0;
    data.nextIdentifier = 0;
    for (auto& identifier : data.identifiers) {
        identifier = 0;
    }
    ++data.generation;
    data.state = State::Bound;
    return Status::Ok;
}

Status Close() noexcept {
    ServicesV1 services{};
    if (!Pin(services, true)) {
        const State state = Inspect().state;
        if (state == State::Binding) {
            return Status::Busy;
        }
        return state == State::Bound || state == State::Closing ? Status::Exhausted : Status::Ok;
    }
    if (!services.ownerThread(services.context)) {
        Unpin();
        return Status::WrongThread;
    }
    {
        Lock guard;
        data.state = State::Closing;
        if (data.active != 1 || data.borrowers != 0) {
            --data.active;
            return Status::Busy;
        }
    }
    const bool released = services.releaseContext(services.context);
    Lock guard;
    --data.active;
    if (!released) {
        return Status::ReleaseFailed;
    }
    data.services = {};
    data.state = State::Closed;
    return Status::Ok;
}

Call::Call(const char* operation) noexcept : services_{} {
    if (!Pin(services_, false)) {
        const State state = Inspect().state;
        InvalidLegacyUse(operation, state == State::Closing ? "closing" : "not bound");
    }
    if (!services_.ownerThread(services_.context)) {
        Unpin();
        InvalidLegacyUse(operation, "requires owner thread");
    }
}

Call::~Call() {
    Unpin();
}

Status AcquireBorrower(Borrower& borrower) noexcept {
    Call call("retain borrower");
    Lock guard;
    if (data.nextIdentifier == (std::numeric_limits<std::uint64_t>::max)()) {
        return Status::Exhausted;
    }
    for (auto& identifier : data.identifiers) {
        if (identifier == 0) {
            identifier = ++data.nextIdentifier;
            ++data.borrowers;
            borrower = {data.generation, identifier};
            return Status::Ok;
        }
    }
    return Status::Exhausted;
}

Status ReleaseBorrower(const Borrower& borrower) noexcept {
    ServicesV1 services{};
    if (!Pin(services, true)) {
        return Status::InvalidBorrower;
    }
    if (!services.ownerThread(services.context)) {
        Unpin();
        return Status::WrongThread;
    }
    Status status = Status::InvalidBorrower;
    {
        Lock guard;
        if (borrower.generation == data.generation && borrower.identifier != 0) {
            for (auto& identifier : data.identifiers) {
                if (identifier == borrower.identifier) {
                    identifier = 0;
                    --data.borrowers;
                    status = Status::Ok;
                    break;
                }
            }
        }
    }
    Unpin();
    return status;
}

bool EncodeClockTicks(std::uint64_t ticks, std::uint32_t numerator, std::uint32_t denominator,
    std::uint32_t& encodedMillis) noexcept {
    if (numerator == 0 || denominator == 0) {
        return false;
    }
    constexpr unsigned int WordBits = 32;
    constexpr int ProductBits = 96; // uint64 ticks multiplied by uint32 timebase numerator.
    constexpr std::uint64_t NanosecondsPerMillisecond = 1000000;
    const std::uint64_t lower = static_cast<std::uint32_t>(ticks) * static_cast<std::uint64_t>(numerator);
    const std::uint64_t upper = (ticks >> WordBits) * numerator + (lower >> WordBits);
    const std::uint32_t words[] = {static_cast<std::uint32_t>(lower), static_cast<std::uint32_t>(upper),
        static_cast<std::uint32_t>(upper >> WordBits)};
    const std::uint64_t divisor = static_cast<std::uint64_t>(denominator) * NanosecondsPerMillisecond;
    std::uint64_t remainder = 0;
    std::uint32_t quotient = 0;
    for (int bit = ProductBits - 1; bit >= 0; --bit) {
        const unsigned int index = static_cast<unsigned int>(bit);
        remainder = (remainder << 1) | ((words[index / WordBits] >> (index % WordBits)) & 1u);
        if (remainder >= divisor) {
            remainder -= divisor;
            if (index < WordBits) {
                quotient |= std::uint32_t{1} << index;
            }
        }
    }
    encodedMillis = quotient;
    return true;
}
}
