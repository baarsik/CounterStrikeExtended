#pragma once

#if !defined(_WIN32)

#include <cstddef>
#include <signal.h>
#include <time.h>

class TokenLine;
class CSysModule;

namespace hltv_posix {

constexpr std::size_t PathCapacity = 4096;
constexpr std::size_t ManagedSignalCount = 3;

bool SetArguments(TokenLine& commandLine, int argc, char* const* argv, const char** error);
bool ExecutableDirectory(char* destination, std::size_t capacity);
using ModuleLoader = CSysModule* (*)(const char*);
CSysModule* LoadRuntimeModule(const char* name, const char* executableDirectory, ModuleLoader loader);

extern volatile sig_atomic_t ShutdownRequested;
using SignalInstaller = decltype(&::sigaction);

class SignalScope {
public:
    explicit SignalScope(SignalInstaller installer = ::sigaction);
    ~SignalScope();
    bool Install();

private:
    SignalScope(const SignalScope&) = delete;
    SignalScope& operator=(const SignalScope&) = delete;
    void Restore();

    SignalInstaller m_Installer;
    struct sigaction m_Previous[ManagedSignalCount] = {};
    int m_Installed = 0;
    sig_atomic_t m_PreviousRequest = 0;
    bool m_Active = false;
};

using ClockReader = decltype(&::clock_gettime);

class MonotonicClock {
public:
    explicit MonotonicClock(ClockReader reader = ::clock_gettime);
    bool Reset();
    bool Read(double& elapsed);

private:
    ClockReader m_Reader;
    timespec m_Previous = {};
    double m_Elapsed = 0;
    bool m_Started = false;
};

bool SleepMilliseconds(unsigned int milliseconds, decltype(&::nanosleep) sleeper = ::nanosleep);

}

#endif
