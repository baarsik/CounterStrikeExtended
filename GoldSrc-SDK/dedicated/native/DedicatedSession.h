#pragma once
#include "DedicatedCommandLine.h"
#include "DedicatedModuleSet.h"
#include "NativeGui.h"
#include "Signals.h"
#include "TextConsolePosix.h"
#include <cstdint>
#include <string>

namespace cse::dedicated {
class DedicatedSession {
public:
    enum class State { Closed, Opening, Configuring, Ready, Starting, Running, Stopping };
    DedicatedSession() = default;
    ~DedicatedSession() noexcept;
    DedicatedSession(const DedicatedSession&) = delete;
    DedicatedSession& operator=(const DedicatedSession&) = delete;
    bool Open(int argc, const char* const* argv, std::string& error);
    int Run();
    void RequestStop() noexcept;
    void Close() noexcept;
    State CurrentState() const noexcept {
        return state_;
    }
    std::uint64_t Generation() const noexcept {
        return generation_;
    }
    bool RestartRequested() const noexcept {
        return restart_;
    }
    const std::string& Executable() const noexcept {
        return executable_;
    }
    const DedicatedCommandLine& CommandLine() const noexcept {
        return command_;
    }

private:
    DedicatedCommandLine command_;
    DedicatedModuleSet modules_;
    SignalScope signals_;
    CTextConsolePosix console_;
    NativeGui gui_;
    bool guiOpen_ = false;
    std::string root_;
    std::string executable_;
    State state_ = State::Closed;
    std::uint64_t generation_ = 0;
    bool mounted_ = false;
    bool consoleOpen_ = false;
    bool engineEntered_ = false;
    bool stop_ = false;
    bool restart_ = false;
};
} // namespace cse::dedicated
