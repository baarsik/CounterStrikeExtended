#pragma once

#include <atomic>
#include <memory>

// Native cancellation shared by the imported radar algorithms and their worker.
namespace taskcoro {
class CancellationToken {
  private:
    std::atomic<bool> canceled_{false};

  public:
    virtual ~CancellationToken() = default;

    static std::shared_ptr<CancellationToken> Create() {
        return std::make_shared<CancellationToken>();
    }

    virtual bool IsCanceled() const {
        return canceled_.load(std::memory_order_relaxed);
    }

    void SetCanceled() {
        canceled_.store(true, std::memory_order_relaxed);
    }
};
} // namespace taskcoro
