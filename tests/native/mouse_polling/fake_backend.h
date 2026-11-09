#pragma once

#include "native_mouse_poller.h"
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <mutex>
#include <thread>

void Check(bool condition, const char *expression, int line);
#define CHECK(expression) Check((expression), #expression, __LINE__)

class FakeBackend {
public:
    std::mutex stateMutex;
    std::condition_variable stateCondition;
    std::mutex eventMutex;
    std::condition_variable eventCondition;
    std::mutex providerMutex;
    std::condition_variable providerCondition;
    std::mutex doneMutex;
    std::condition_variable doneCondition;
    std::thread worker;
    bool eventCreated = false;
    bool eventSignaled = false;
    bool done = false;
    bool holdPoll = false;
    bool pollEntered = false;
    cse::mouse::Point cursor{};
    std::atomic<int> pollReads{0};
    std::atomic<int> mainReads{0};
    std::atomic<int> creates{0};
    std::atomic<int> closes{0};
    std::atomic<int> conditionWaits{0};
    std::atomic<bool> failEvent{false};
    std::atomic<bool> failThread{false};
    std::atomic<bool> failRead{false};
    std::atomic<bool> failSignal{false};
    std::atomic<bool> failReset{false};
    std::atomic<bool> failWait{false};
    std::atomic<bool> failJoin{false};
    std::atomic<bool> failClose{false};
    std::atomic<bool> failCondition{false};

    void Lock() {
        stateMutex.lock();
    }
    void Unlock() {
        stateMutex.unlock();
    }
    std::uint64_t NowMs() {
        return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count());
    }
    cse::mouse::CallResult CreateControl() {
        ++creates;
        if (failEvent.exchange(false)) {
            return {false, 101};
        }
        eventCreated = true;
        eventSignaled = false;
        return {true, 0};
    }
    cse::mouse::CallResult StartThread(void (*entry)(void *), void *context) {
        if (failThread.exchange(false)) {
            return {false, 102};
        }
        done = false;
        worker = std::thread([this, entry, context] {
            entry(context);
            std::lock_guard<std::mutex> guard(doneMutex);
            done = true;
            doneCondition.notify_all();
        });
        return {true, 0};
    }
    cse::mouse::CallResult SignalControl() {
        if (failSignal.exchange(false)) {
            return {false, 103};
        }
        std::lock_guard<std::mutex> guard(eventMutex);
        eventSignaled = true;
        eventCondition.notify_all();
        return {true, 0};
    }
    cse::mouse::CallResult ResetControl() {
        if (failReset.exchange(false)) {
            return {false, 104};
        }
        std::lock_guard<std::mutex> guard(eventMutex);
        eventSignaled = false;
        return {true, 0};
    }
    cse::mouse::WaitResult WaitControl(std::uint32_t milliseconds) {
        if (failWait.exchange(false)) {
            return {cse::mouse::WaitKind::Failed, 105};
        }
        std::unique_lock<std::mutex> lock(eventMutex);
        if (milliseconds == cse::mouse::InfiniteWait) {
            eventCondition.wait(lock, [this] { return eventSignaled; });
            return {cse::mouse::WaitKind::Signaled, 0};
        }
        const bool signaled = eventCondition.wait_for(lock, std::chrono::milliseconds(milliseconds),
            [this] { return eventSignaled; });
        return {signaled ? cse::mouse::WaitKind::Signaled : cse::mouse::WaitKind::Timeout, 0};
    }
    cse::mouse::WaitResult WaitCondition(std::uint32_t milliseconds) {
        ++conditionWaits;
        if (failCondition.exchange(false)) {
            return {cse::mouse::WaitKind::Failed, 109};
        }
        std::unique_lock<std::mutex> lock(stateMutex, std::adopt_lock);
        const auto result = stateCondition.wait_for(lock, std::chrono::milliseconds(milliseconds));
        lock.release();
        return {result == std::cv_status::timeout ? cse::mouse::WaitKind::Timeout :
            cse::mouse::WaitKind::Signaled, 0};
    }
    void WakeCondition() {
        stateCondition.notify_all();
    }
    cse::mouse::CallResult ReadCursor(cse::mouse::Point &point, cse::mouse::SampleReason reason) {
        std::unique_lock<std::mutex> lock(providerMutex);
        if (reason == cse::mouse::SampleReason::Poll) {
            ++pollReads;
            pollEntered = true;
            providerCondition.notify_all();
            providerCondition.wait(lock, [this] { return !holdPoll; });
        } else {
            ++mainReads;
        }
        point = cursor;
        return failRead.exchange(false) ? cse::mouse::CallResult{false, 106} : cse::mouse::CallResult{true, 0};
    }
    cse::mouse::WaitResult Join(std::uint32_t milliseconds) {
        if (failJoin.exchange(false)) {
            return {cse::mouse::WaitKind::Failed, 107};
        }
        if (!worker.joinable()) {
            return {cse::mouse::WaitKind::Signaled, 0};
        }
        std::unique_lock<std::mutex> lock(doneMutex);
        if (!doneCondition.wait_for(lock, std::chrono::milliseconds(milliseconds), [this] { return done; })) {
            return {cse::mouse::WaitKind::Timeout, 0};
        }
        lock.unlock();
        worker.join();
        return {cse::mouse::WaitKind::Signaled, 0};
    }
    cse::mouse::CallResult CloseHandles() {
        CHECK(!worker.joinable());
        if (failClose.exchange(false)) {
            return {false, 108};
        }
        if (eventCreated) {
            eventCreated = false;
            ++closes;
        }
        return {true, 0};
    }
    void SetCursor(std::int32_t x, std::int32_t y) {
        std::lock_guard<std::mutex> guard(providerMutex);
        cursor = {x, y};
    }
    void HoldPoll() {
        std::lock_guard<std::mutex> guard(providerMutex);
        holdPoll = true;
        pollEntered = false;
    }
    void AwaitPoll() {
        std::unique_lock<std::mutex> lock(providerMutex);
        CHECK(providerCondition.wait_for(lock, std::chrono::seconds(2), [this] { return pollEntered; }));
    }
    void ReleasePoll() {
        std::lock_guard<std::mutex> guard(providerMutex);
        holdPoll = false;
        providerCondition.notify_all();
    }
    void AwaitOwnerWait() {
        const auto start = std::chrono::steady_clock::now();
        while (conditionWaits == 0 && std::chrono::steady_clock::now() - start < std::chrono::seconds(2)) {
            std::this_thread::yield();
        }
        CHECK(conditionWaits != 0);
    }
    ~FakeBackend() {
        CHECK(!worker.joinable());
    }
};
