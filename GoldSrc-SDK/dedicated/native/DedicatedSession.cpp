#include "DedicatedSession.h"
#include "../../common/icommandline.h"
#include "../../engine/interface.h"
#include "../../public/idedicatedexports.h"
#include "../../public/native_dedicated_abi.h"
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <mach-o/dyld.h>
#include <time.h>
#include <unistd.h>
#include <vector>

namespace {
CTextConsolePosix* logConsole = nullptr;
cse::dedicated::NativeGui* logGui = nullptr;
cse::dedicated::DedicatedSession* activeSession = nullptr;
std::uint64_t nextGeneration = 0;
class DedicatedExports final : public IDedicatedExports {
public:
    void Sys_Printf(char* text) override {
        if (logConsole) {
            logConsole->Print(text);
        } else if (logGui) {
            logGui->Print(text);
        } else if (text) {
            std::fputs(text, stderr);
        }
    }
};
EXPOSE_SINGLE_INTERFACE(DedicatedExports, IDedicatedExports, VENGINE_DEDICATEDEXPORTS_API_VERSION);
} // namespace

void Sys_Printf(char* format, ...) {
    constexpr std::size_t LogCapacity = 1024;
    char text[LogCapacity]{};
    va_list arguments;
    va_start(arguments, format);
    std::vsnprintf(text, sizeof(text), format, arguments);
    va_end(arguments);
    if (logConsole) {
        logConsole->Print(text);
    } else if (logGui) {
        logGui->Print(text);
    } else {
        std::fputs(text, stderr);
    }
}

extern "C" __attribute__((visibility("default"))) const cse::abi::Descriptor* CseGetDedicatedAbiV1() {
    return &cse::dedicated_abi::Host;
}

namespace cse::dedicated {
DedicatedSession::~DedicatedSession() noexcept {
    Close();
    if (state_ != State::Closed) {
        std::fputs("Dedicated shutdown incomplete; callback owner must remain alive\n", stderr);
        // Restore shared terminal state without destroying live callback receivers
        // or releasing any image whose retirement was rejected.
        console_.RestoreTerminalMode();
        std::_Exit(EXIT_FAILURE);
    }
}

bool DedicatedSession::Open(int argc, const char* const* argv, std::string& error) {
    if (state_ != State::Closed || activeSession) {
        error = "Another dedicated session is active";
        return false;
    }
    if (!command_.Parse(argc, argv, error)) {
        return false;
    }
    if (nextGeneration == (std::numeric_limits<std::uint64_t>::max)()) {
        error = "Dedicated session generation exhausted";
        return false;
    }
    generation_ = ++nextGeneration;
    state_ = State::Opening;
    activeSession = this;
    stop_ = false;
    restart_ = false;
    std::uint32_t bytes = 0;
    _NSGetExecutablePath(nullptr, &bytes);
    std::vector<char> path(bytes);
    if (_NSGetExecutablePath(path.data(), &bytes) != 0) {
        error = "Cannot resolve dedicated executable";
        Close();
        return false;
    }
    char* canonical = realpath(path.data(), nullptr);
    if (!canonical) {
        error = "Cannot canonicalize dedicated executable";
        Close();
        return false;
    }
    executable_ = canonical;
    root_ = canonical;
    std::free(canonical);
    const auto separator = root_.find_last_of('/');
    root_.resize(separator == 0 ? 1 : separator);
    cmdline->CreateCmdLine(command_.Encoded().c_str());
    if (!signals_.Install()) {
        error = "Cannot install dedicated signal handlers";
        Close();
        return false;
    }
    if (command_.Console()) {
        logConsole = &console_;
        if (!console_.Init(nullptr)) {
            error = "Cannot initialize dedicated console";
            Close();
            return false;
        }
        consoleOpen_ = true;
        if (console_.OutputFailed()) {
            error = "Dedicated stdout is unavailable";
            Close();
            return false;
        }
    }
    if (!modules_.Open(root_.c_str(), !command_.Console(), error)) {
        Close();
        return false;
    }
    modules_.Filesystem()->Mount();
    mounted_ = true;
    if (!command_.Console()) {
        logGui = &gui_;
        if (!gui_.Open(modules_, error)) {
            Close();
            return false;
        }
        guiOpen_ = true;
    }
    state_ = guiOpen_ ? State::Configuring : State::Ready;
    return true;
}

int DedicatedSession::Run() {
    if (state_ != State::Ready && state_ != State::Configuring) {
        return EXIT_FAILURE;
    }
    while (guiOpen_ && !gui_.ReadyToStart()) {
        if (stop_ || ShutdownRequested || !gui_.Pump()) {
            Close();
            return EXIT_SUCCESS;
        }
        const timespec delay{0, 1000000};
        nanosleep(&delay, nullptr);
    }
    state_ = State::Starting;
    engineEntered_ = true;
    std::string command = command_.Encoded();
    if (!modules_.Engine()->Init(root_.data(), command.data(), Sys_GetFactoryThis(), modules_.FilesystemFactory())) {
        Close();
        return EXIT_FAILURE;
    }
    if (guiOpen_) {
        std::string error;
        if (const char* commands = gui_.Commands()) {
            modules_.Engine()->AddConsoleText(const_cast<char*>(commands));
        }
        if (!gui_.Started(error)) {
            std::fprintf(stderr, "%s\n", error.c_str());
            Close();
            return EXIT_FAILURE;
        }
    }
    state_ = State::Running;
    constexpr long FrameDelayNanoseconds = 1000000;
    double lastStatus = 0;
    while (!stop_ && !ShutdownRequested) {
        if (guiOpen_ && !gui_.Pump()) {
            break;
        }
        if (consoleOpen_) {
            if (char* line = console_.GetLine()) {
                std::string input(line);
                input += '\n';
                modules_.Engine()->AddConsoleText(input.data());
            }
            if (console_.InputFailed() || console_.OutputFailed()) {
                Close();
                return EXIT_FAILURE;
            }
        }
        if (!modules_.Engine()->RunFrame()) {
            break;
        }
        if (consoleOpen_) {
            timespec now{};
            if (clock_gettime(CLOCK_MONOTONIC, &now) != 0) {
                Close();
                return EXIT_FAILURE;
            }
            constexpr double NanosecondsPerSecond = 1000000000.0;
            constexpr double StatusIntervalSeconds = 0.5;
            const double seconds = now.tv_sec + now.tv_nsec / NanosecondsPerSecond;
            if (seconds - lastStatus >= StatusIntervalSeconds) {
                float fps = 0;
                int active = 0;
                int maximum = 0;
                char map[cse::engine_bootstrap_abi::StatusMapBytes]{};
                modules_.Engine()->UpdateStatus(&fps, &active, &maximum, map);
                map[sizeof(map) - 1] = '\0';
                char status[MAX_CONSOLE_TEXTLEN]{};
                std::snprintf(status, sizeof(status), "%.1f fps %2i/%2i on %16s", fps, active, maximum, map);
                console_.SetStatusLine(status);
                lastStatus = seconds;
            }
        }
        const timespec delay{0, FrameDelayNanoseconds};
        nanosleep(&delay, nullptr);
    }
    // Provider sys_engine.cpp maps state 5 to QUIT_RESTART; 3 is DLL_CLOSE.
    constexpr int DedicatedRestartState = 5;
    const int result = modules_.Shutdown();
    engineEntered_ = false;
    restart_ = result == DedicatedRestartState && !stop_ && !ShutdownRequested;
    Close();
    return state_ == State::Closed ? EXIT_SUCCESS : EXIT_FAILURE;
}

void DedicatedSession::RequestStop() noexcept {
    stop_ = true;
}

void DedicatedSession::Close() noexcept {
    if (state_ == State::Closed) {
        return;
    }
    state_ = State::Stopping;
    stop_ = true;
    if (guiOpen_) {
        gui_.Stop();
    }
    if (engineEntered_) {
        modules_.Shutdown();
        engineEntered_ = false;
    }
    modules_.Shutdown();
    if (!modules_.Retire()) {
        return;
    }
    // The log receiver and callback factory survive every engine cleanup call.
    if (guiOpen_) {
        gui_.Close();
        guiOpen_ = false;
    }
    logGui = nullptr;
    if (mounted_) {
        modules_.Filesystem()->Unmount();
        mounted_ = false;
    }
    modules_.Close();
    if (!modules_.IsClosed()) {
        return;
    }
    if (consoleOpen_) {
        console_.ShutDown();
        consoleOpen_ = false;
    }
    logConsole = nullptr;
    signals_.Restore();
    activeSession = nullptr;
    state_ = State::Closed;
}
} // namespace cse::dedicated
