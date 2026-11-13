#if !defined(_WIN32)

#include "PosixSystem.h"
#include "common/TokenLine.h"

#include <sched.h>
#include <unistd.h>
#if defined(__APPLE__)
#include <mach-o/dyld.h>
#endif

#include <array>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace hltv_posix {

volatile sig_atomic_t ShutdownRequested = 0;

namespace {

constexpr long NanosecondsPerSecond = 1000000000;
constexpr int ManagedSignals[ManagedSignalCount] = {SIGINT, SIGTERM, SIGPIPE};

void RequestShutdown(int) {
    ShutdownRequested = 1;
}

bool ArgumentFailure(const char* description, int code, const char** error) {
    if (error != nullptr) {
        *error = description;
    }
    errno = code;
    return false;
}

bool ValidTime(const timespec& value) {
    return value.tv_sec >= 0 && value.tv_nsec >= 0 && value.tv_nsec < NanosecondsPerSecond;
}

}

bool SetArguments(TokenLine& commandLine, int argc, char* const* argv, const char** error) {
    constexpr std::size_t TokenCapacity = sizeof(commandLine.m_token) / sizeof(commandLine.m_token[0]);
    constexpr std::size_t StorageCapacity = sizeof(commandLine.m_tokenBuffer);
    constexpr std::size_t DisplayCapacity = sizeof(commandLine.m_fullLine);
    // TokenLine::SetLine reserves two terminating bytes and rejects its final (128th) token slot.
    constexpr std::size_t PayloadCapacity = StorageCapacity - 1;
    if (argc < 0 || (argc > 1 && argv == nullptr)) {
        return ArgumentFailure("invalid native argument vector", EINVAL, error);
    }
    const int argumentCount = argc > 0 ? argc - 1 : 0;
    if (static_cast<std::size_t>(argumentCount) >= TokenCapacity) {
        return ArgumentFailure("too many native arguments", E2BIG, error);
    }
    std::array<char, StorageCapacity> tokens = {};
    std::array<char, DisplayCapacity> display = {};
    std::array<std::size_t, TokenCapacity> offsets = {};
    std::size_t used = 0;
    std::size_t displayed = 0;
    bool displayFits = true;
    const auto appendDisplay = [&](char character) {
        if (displayed >= display.size() - 1) {
            displayFits = false;
            return;
        }
        display[displayed++] = character;
    };
    for (int index = 0; index < argumentCount; ++index) {
        const char* argument = argv[index + 1];
        if (argument == nullptr) {
            return ArgumentFailure("null native argument", EINVAL, error);
        }
        const std::size_t remaining = PayloadCapacity - used;
        const std::size_t length = strnlen(argument, remaining + 1);
        if (length >= remaining) {
            return ArgumentFailure("native arguments exceed TokenLine storage", E2BIG, error);
        }
        offsets[static_cast<std::size_t>(index)] = used;
        std::memcpy(tokens.data() + used, argument, length + 1);
        used += length + 1;
        if (index != 0) {
            appendDisplay(' ');
        }
        appendDisplay('"');
        for (std::size_t byte = 0; byte < length; ++byte) {
            if (argument[byte] == '"' || argument[byte] == '\\') {
                appendDisplay('\\');
            }
            appendDisplay(argument[byte]);
        }
        appendDisplay('"');
    }
    if (!displayFits) {
        // This buffer is diagnostic only. Never reject/shorten exact tokens merely to render added quotes.
        constexpr char Omitted[] = "[argument display omitted: exact tokens exceed diagnostic representation]";
        display.fill(0);
        std::memcpy(display.data(), Omitted, sizeof(Omitted));
    }
    std::memcpy(commandLine.m_tokenBuffer, tokens.data(), tokens.size());
    std::memcpy(commandLine.m_fullLine, display.data(), display.size());
    for (std::size_t index = 0; index < TokenCapacity; ++index) {
        commandLine.m_token[index] = index < static_cast<std::size_t>(argumentCount)
            ? commandLine.m_tokenBuffer + offsets[index] : nullptr;
    }
    commandLine.m_tokenNumber = argumentCount;
    if (error != nullptr) {
        *error = nullptr;
    }
    return true;
}

bool ExecutableDirectory(char* destination, std::size_t capacity) {
    if (destination == nullptr || capacity == 0) {
        errno = EINVAL;
        return false;
    }
#if defined(__APPLE__)
    std::uint32_t bytes = 0;
    _NSGetExecutablePath(nullptr, &bytes);
    if (bytes == 0) {
        errno = ENOENT;
        return false;
    }
    char* requested = static_cast<char*>(std::malloc(bytes));
    if (requested == nullptr) {
        errno = ENOMEM;
        return false;
    }
    if (_NSGetExecutablePath(requested, &bytes) != 0) {
        std::free(requested);
        errno = ENAMETOOLONG;
        return false;
    }
    char* canonical = realpath(requested, nullptr);
    const int pathError = errno;
    std::free(requested);
    if (canonical == nullptr) {
        errno = pathError;
        return false;
    }
    char* separator = std::strrchr(canonical, '/');
    if (separator == nullptr) {
        std::free(canonical);
        errno = ENOENT;
        return false;
    }
    const std::size_t length = separator == canonical ? 1 : static_cast<std::size_t>(separator - canonical);
    if (length >= capacity) {
        std::free(canonical);
        errno = ENAMETOOLONG;
        return false;
    }
    std::memcpy(destination, canonical, length);
    destination[length] = 0;
    std::free(canonical);
    return true;
#else
#error Native HLTV executable discovery requires qualification before selecting another POSIX target
#endif
}

CSysModule* LoadRuntimeModule(const char* name, const char* executableDirectory, ModuleLoader loader) {
    if (name == nullptr || !*name || executableDirectory == nullptr || loader == nullptr) {
        errno = EINVAL;
        return nullptr;
    }
    const std::size_t nameLength = strnlen(name, PathCapacity);
    if (nameLength >= PathCapacity) {
        errno = ENAMETOOLONG;
        return nullptr;
    }
    if (std::strchr(name, '/') != nullptr) {
        return loader(name);
    }
    const std::size_t rootLength = strnlen(executableDirectory, PathCapacity);
    const bool separator = rootLength != 0 && executableDirectory[rootLength - 1] != '/';
    constexpr std::size_t TerminatorBytes = 1;
    if (rootLength == 0 || rootLength >= PathCapacity || nameLength >= PathCapacity
        || rootLength + separator + nameLength + TerminatorBytes > PathCapacity) {
        errno = rootLength == 0 ? EINVAL : ENAMETOOLONG;
        return nullptr;
    }
    char path[PathCapacity];
    std::memcpy(path, executableDirectory, rootLength);
    std::size_t offset = rootLength;
    if (separator) {
        path[offset++] = '/';
    }
    std::memcpy(path + offset, name, nameLength + 1);
    if (CSysModule* module = loader(path)) {
        return module;
    }
    return loader(name);
}

SignalScope::SignalScope(SignalInstaller installer) : m_Installer(installer) {
}

SignalScope::~SignalScope() {
    Restore();
}

bool SignalScope::Install() {
    if (m_Active) {
        return true;
    }
    if (m_Installer == nullptr) {
        errno = EINVAL;
        return false;
    }
    m_Active = true;
    m_PreviousRequest = ShutdownRequested;
    ShutdownRequested = 0;
    for (int signal : ManagedSignals) {
        struct sigaction action = {};
        action.sa_handler = signal == SIGPIPE ? SIG_IGN : RequestShutdown;
        sigemptyset(&action.sa_mask);
        if (m_Installer(signal, &action, &m_Previous[m_Installed]) != 0) {
            const int error = errno;
            Restore();
            errno = error;
            return false;
        }
        ++m_Installed;
    }
    return true;
}

void SignalScope::Restore() {
    if (!m_Active) {
        return;
    }
    const int savedError = errno;
    while (m_Installed > 0) {
        --m_Installed;
        m_Installer(ManagedSignals[m_Installed], &m_Previous[m_Installed], nullptr);
    }
    ShutdownRequested = m_PreviousRequest;
    m_Active = false;
    errno = savedError;
}

MonotonicClock::MonotonicClock(ClockReader reader) : m_Reader(reader) {
}

bool MonotonicClock::Reset() {
    timespec sample = {};
    if (m_Reader(CLOCK_MONOTONIC, &sample) != 0) {
        return false;
    }
    if (!ValidTime(sample)) {
        errno = EIO;
        return false;
    }
    m_Previous = sample;
    m_Elapsed = 0;
    m_Started = true;
    return true;
}

bool MonotonicClock::Read(double& elapsed) {
    timespec sample = {};
    if (m_Reader(CLOCK_MONOTONIC, &sample) != 0) {
        return false;
    }
    if (!ValidTime(sample) || (m_Started && (sample.tv_sec < m_Previous.tv_sec
        || (sample.tv_sec == m_Previous.tv_sec && sample.tv_nsec < m_Previous.tv_nsec)))) {
        errno = EIO;
        return false;
    }
    if (m_Started) {
        m_Elapsed += static_cast<double>(sample.tv_sec - m_Previous.tv_sec)
            + static_cast<double>(sample.tv_nsec - m_Previous.tv_nsec) / NanosecondsPerSecond;
    }
    m_Previous = sample;
    m_Started = true;
    elapsed = m_Elapsed;
    return true;
}

bool SleepMilliseconds(unsigned int milliseconds, decltype(&::nanosleep) sleeper) {
    if (ShutdownRequested) {
        return true;
    }
    if (milliseconds == 0) {
        return sched_yield() == 0;
    }
    constexpr unsigned int MillisecondsPerSecond = 1000;
    constexpr long NanosecondsPerMillisecond = NanosecondsPerSecond / MillisecondsPerSecond;
    timespec remaining = {static_cast<time_t>(milliseconds / MillisecondsPerSecond),
        static_cast<long>(milliseconds % MillisecondsPerSecond) * NanosecondsPerMillisecond};
    while (!ShutdownRequested) {
        timespec interrupted = {};
        if (sleeper(&remaining, &interrupted) == 0) {
            return true;
        }
        if (errno != EINTR) {
            return false;
        }
        remaining = interrupted;
    }
    return true;
}

}

#endif
