#include "PosixSystem.h"
#include "common/TokenLine.h"

#include <sys/stat.h>
#include <unistd.h>

#include <array>
#include <cerrno>
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace {

void Require(bool condition, const char* description) {
    if (!condition) {
        std::fprintf(stderr, "FAIL %s errno=%d\n", description, errno);
        std::exit(EXIT_FAILURE);
    }
}

timespec sample = {};
bool failClock = false;
int ClockSample(clockid_t clock, timespec* destination) {
    Require(clock == CLOCK_MONOTONIC, "use the monotonic clock");
    if (failClock) {
        errno = EIO;
        return -1;
    }
    *destination = sample;
    return 0;
}

int sleepCalls = 0;
int SleepSample(const timespec* request, timespec* remaining) {
    ++sleepCalls;
    if (sleepCalls == 1) {
        Require(request->tv_sec == 1 && request->tv_nsec == 250000000, "milliseconds retain full native value");
        remaining->tv_sec = 0;
        remaining->tv_nsec = 500000000;
        errno = EINTR;
        return -1;
    }
    Require(request->tv_sec == 0 && request->tv_nsec == 500000000, "resume only the interrupted remainder");
    return 0;
}

int signalCalls = 0;
int failingSignalCall = 0;
int SignalInstall(int signal, const struct sigaction* action, struct sigaction* previous) {
    ++signalCalls;
    if (signalCalls == failingSignalCall) {
        errno = EINVAL;
        return -1;
    }
    return sigaction(signal, action, previous);
}

void PreviousHandler(int) {
}

std::vector<std::string> moduleAttempts;
std::string successfulModule;
int moduleIdentity = 0;
CSysModule* ModuleLoad(const char* path) {
    moduleAttempts.emplace_back(path);
    struct stat information = {};
    if (successfulModule == path && stat(path, &information) == 0 && S_ISREG(information.st_mode)) {
        return reinterpret_cast<CSysModule*>(&moduleIdentity);
    }
    return nullptr;
}

void Arguments() {
    TokenLine tokens;
    char executable[] = "excluded executable";
    char option[] = "-name";
    char unicode[] = "Za\xc5\xbc\xc3\xb3\xc5\x82\xc4\x87 path";
    char quotes[] = "\"a b\\c\"";
    char empty[] = "";
    char* arguments[] = {executable, option, unicode, quotes, empty};
    const char* error = nullptr;
    Require(hltv_posix::SetArguments(tokens, 5, arguments, &error), "copy exact native argv");
    Require(tokens.CountToken() == 4 && std::strcmp(tokens.GetToken(0), option) == 0, "exclude argv0");
    Require(std::strcmp(tokens.CheckToken(option), unicode) == 0, "UTF8 and spaces remain one token");
    Require(std::strcmp(tokens.GetToken(2), quotes) == 0 && tokens.GetToken(3)[0] == 0, "literal quotes and empty arg");
    Require(tokens.GetToken(4) == nullptr, "token count remains bounded");
    std::array<char, sizeof(tokens.m_tokenBuffer)> saved = {};
    std::memcpy(saved.data(), tokens.m_tokenBuffer, saved.size());
    const int savedCount = tokens.m_tokenNumber;
    std::string tooLong(sizeof(tokens.m_tokenBuffer), 'x');
    char* oversized[] = {executable, &tooLong[0]};
    Require(!hltv_posix::SetArguments(tokens, 2, oversized, &error) && errno == E2BIG, "oversize argv fails");
    Require(tokens.m_tokenNumber == savedCount && std::memcmp(saved.data(), tokens.m_tokenBuffer, saved.size()) == 0,
        "failed argv assembly leaves previous state intact");
    Require(std::strcmp(tokens.GetToken(2), quotes) == 0, "failed argv assembly preserves token pointers");
    char* aliased[] = {executable, tokens.GetToken(1), tokens.GetToken(2)};
    Require(hltv_posix::SetArguments(tokens, 3, aliased, &error), "input may alias previous token storage");
    Require(std::strcmp(tokens.GetToken(0), unicode) == 0 && std::strcmp(tokens.GetToken(1), quotes) == 0,
        "aliased input is copied before mutation");
    std::vector<char*> maximum(128, empty);
    maximum[0] = executable;
    Require(hltv_posix::SetArguments(tokens, static_cast<int>(maximum.size()), maximum.data(), &error)
        && tokens.CountToken() == 127, "127 native tokens fit");
    maximum.push_back(empty);
    Require(!hltv_posix::SetArguments(tokens, static_cast<int>(maximum.size()), maximum.data(), &error)
        && errno == E2BIG && tokens.CountToken() == 127, "128th token rejects without partial mutation");
    std::string displayHeavy(15, 'x');
    maximum.assign(128, &displayHeavy[0]);
    maximum[0] = executable;
    Require(hltv_posix::SetArguments(tokens, static_cast<int>(maximum.size()), maximum.data(), &error),
        "diagnostic quoting cannot truncate or reject exact tokens which fit storage");
    Require(tokens.CountToken() == 127 && std::strcmp(tokens.GetToken(126), displayHeavy.c_str()) == 0,
        "all exact tokens survive diagnostic overflow");
    Require(std::strstr(tokens.GetLine(), "display omitted") != nullptr, "diagnostic elision is explicit");
    std::string maximumArgument(sizeof(tokens.m_tokenBuffer) - 2, 'x');
    char* boundary[] = {executable, &maximumArgument[0]};
    Require(hltv_posix::SetArguments(tokens, 2, boundary, &error)
        && std::strlen(tokens.GetToken(0)) == maximumArgument.size(), "exact legacy storage boundary");
    maximumArgument.push_back('x');
    boundary[1] = &maximumArgument[0];
    Require(!hltv_posix::SetArguments(tokens, 2, boundary, &error) && errno == E2BIG,
        "one byte beyond payload capacity rejects");
    Require(hltv_posix::SetArguments(tokens, 0, nullptr, &error) && tokens.CountToken() == 0, "empty argv is valid");
    Require(!hltv_posix::SetArguments(tokens, 2, nullptr, &error) && errno == EINVAL, "invalid argv pointer fails");
}

void Clock() {
    hltv_posix::MonotonicClock clock(ClockSample);
    constexpr time_t HighEpoch = time_t{1} << 40;
    sample = {HighEpoch, 500000000};
    Require(clock.Reset(), "initialize a high native epoch");
    sample = {HighEpoch + 2, 750000000};
    double elapsed = -1;
    Require(clock.Read(elapsed) && elapsed == 2.25, "subtract epoch before floating conversion");
    failClock = true;
    Require(!clock.Read(elapsed) && elapsed == 2.25 && errno == EIO, "failed clock leaves output/state intact");
    failClock = false;
    sample = {HighEpoch + 3, 250000000};
    Require(clock.Read(elapsed) && elapsed == 2.75, "clock resumes from last successful sample");
    sample = {HighEpoch + 2, 0};
    Require(!clock.Read(elapsed) && elapsed == 2.75 && errno == EIO, "backward monotonic sample rejects");
    sample = {HighEpoch + 4, 1000000000};
    Require(!clock.Read(elapsed) && elapsed == 2.75, "invalid nanoseconds reject");
    sample = {HighEpoch + 4, 250000000};
    Require(clock.Read(elapsed) && elapsed == 3.75, "invalid sample did not mutate the valid clock base");
    Require(hltv_posix::SleepMilliseconds(1250, SleepSample) && sleepCalls == 2, "interrupted bounded sleep resumes");
    hltv_posix::ShutdownRequested = 1;
    Require(hltv_posix::SleepMilliseconds(UINT_MAX, SleepSample) && sleepCalls == 2,
        "shutdown request prevents entering a long sleep");
    hltv_posix::ShutdownRequested = 0;
}

void Signals() {
    hltv_posix::ShutdownRequested = 1;
    {
        hltv_posix::SignalScope unused;
    }
    Require(hltv_posix::ShutdownRequested == 1, "an uninstalled scope cannot modify caller signal state");
    hltv_posix::ShutdownRequested = 0;
    const int signals[] = {SIGINT, SIGTERM, SIGPIPE};
    struct sigaction original[3] = {};
    struct sigaction expected = {};
    expected.sa_handler = PreviousHandler;
    sigemptyset(&expected.sa_mask);
    sigaddset(&expected.sa_mask, SIGUSR1);
    expected.sa_flags = SA_RESTART;
    for (std::size_t index = 0; index < 3; ++index) {
        Require(sigaction(signals[index], &expected, &original[index]) == 0, "install owned prior handlers");
    }
    {
        hltv_posix::SignalScope handlers;
        Require(handlers.Install(), "install main-lifetime signal scope");
        raise(SIGTERM);
        Require(hltv_posix::ShutdownRequested == 1, "handler only records shutdown request");
        struct sigaction pipe = {};
        Require(sigaction(SIGPIPE, nullptr, &pipe) == 0 && pipe.sa_handler == SIG_IGN, "closed stdout is nonfatal");
    }
    Require(hltv_posix::ShutdownRequested == 0, "signal scope restores previous flag state");
    for (int signal : signals) {
        struct sigaction restored = {};
        Require(sigaction(signal, nullptr, &restored) == 0 && restored.sa_handler == PreviousHandler
            && (restored.sa_flags & SA_RESTART) && sigismember(&restored.sa_mask, SIGUSR1),
            "main lifetime restores prior handlers");
    }
    failingSignalCall = 2;
    {
        hltv_posix::SignalScope handlers(SignalInstall);
        Require(!handlers.Install() && errno == EINVAL, "partial signal install fails deterministically");
    }
    for (std::size_t index = 0; index < 3; ++index) {
        struct sigaction restored = {};
        Require(sigaction(signals[index], nullptr, &restored) == 0 && restored.sa_handler == PreviousHandler
            && (restored.sa_flags & SA_RESTART) && sigismember(&restored.sa_mask, SIGUSR1),
            "partial installation rolls back every installed handler");
        Require(sigaction(signals[index], &original[index], nullptr) == 0, "restore fixture's original handlers");
    }
}

void Paths(const char* root) {
    std::array<char, hltv_posix::PathCapacity> originalDirectory = {};
    Require(getcwd(originalDirectory.data(), originalDirectory.size()) != nullptr, "capture inherited cwd");
    std::array<char, hltv_posix::PathCapacity> directory = {};
    Require(hltv_posix::ExecutableDirectory(directory.data(), directory.size()), "resolve actual executable directory");
    std::fprintf(stdout, "EXEDIR %s\n", directory.data());
    char tiny[] = "old";
    Require(!hltv_posix::ExecutableDirectory(tiny, sizeof(tiny)) && errno == ENAMETOOLONG
        && std::strcmp(tiny, "old") == 0, "path overflow is explicit and atomic");
    const std::string expected = std::string(root) + "/proxy.dll";
    successfulModule = expected;
    CSysModule* result = hltv_posix::LoadRuntimeModule("proxy.dll", root, ModuleLoad);
    Require(result == reinterpret_cast<CSysModule*>(&moduleIdentity) && moduleAttempts.size() == 1
        && moduleAttempts[0] == expected, "bare module prefers executable root without changing cwd");
    moduleAttempts.clear();
    successfulModule = "proxy.dll";
    Require(hltv_posix::LoadRuntimeModule("proxy.dll", root, ModuleLoad) == result && moduleAttempts.size() == 2,
        "failed executable-root candidate falls back to the existing loader");
    moduleAttempts.clear();
    successfulModule = "relative/path.dll";
    Require(hltv_posix::LoadRuntimeModule(successfulModule.c_str(), root, ModuleLoad) == result
        && moduleAttempts.size() == 1 && moduleAttempts[0] == successfulModule,
        "relative paths retain caller semantics");
    moduleAttempts.clear();
    successfulModule.clear();
    std::string excessive(hltv_posix::PathCapacity, 'x');
    Require(hltv_posix::LoadRuntimeModule("proxy.dll", excessive.c_str(), ModuleLoad) == nullptr
        && errno == ENAMETOOLONG && moduleAttempts.empty(), "oversized path cannot truncate or load another module");
    moduleAttempts.clear();
    successfulModule = "relative/alias.dll";
    Require(hltv_posix::LoadRuntimeModule(successfulModule.c_str(), root, ModuleLoad) == result,
        "caller-relative aliases retain their exact path");
    moduleAttempts.clear();
    successfulModule = std::string(root) + "/absolute.dll";
    Require(hltv_posix::LoadRuntimeModule(successfulModule.c_str(), root, ModuleLoad) == result
        && moduleAttempts.size() == 1, "absolute module path is passed through exactly");
    moduleAttempts.clear();
    successfulModule.clear();
    Require(hltv_posix::LoadRuntimeModule("missing.dll", root, ModuleLoad) == nullptr && moduleAttempts.size() == 2,
        "missing module reports failure after both candidates");
    std::array<char, hltv_posix::PathCapacity> after = {};
    Require(getcwd(after.data(), after.size()) != nullptr && std::strcmp(after.data(), originalDirectory.data()) == 0,
        "executable discovery and module lookup preserve cwd");
}

}

int main(int argc, char** argv) {
    Require(argc >= 2, "helper case");
    if (std::strcmp(argv[1], "argv") == 0) {
        Arguments();
    } else if (std::strcmp(argv[1], "clock") == 0) {
        Clock();
    } else if (std::strcmp(argv[1], "signals") == 0) {
        Signals();
    } else if (std::strcmp(argv[1], "paths") == 0) {
        Require(argc == 3, "path fixture root");
        Paths(argv[2]);
    } else {
        Require(false, "known helper case");
    }
    std::fprintf(stdout, "PASS %s\n", argv[1]);
    return EXIT_SUCCESS;
}
