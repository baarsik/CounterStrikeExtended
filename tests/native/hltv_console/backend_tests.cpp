#include "TextConsolePosix.h"
#include "PosixSystem.h"
#include "common/IBaseSystem.h"
#include "common/ObjectList.h"

#include <sys/resource.h>
#include <termios.h>
#include <unistd.h>
#include <fcntl.h>

#include <cerrno>
#include <chrono>
#include <csignal>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

namespace {

void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "FAIL %s errno=%d\n", message, errno);
        std::exit(EXIT_FAILURE);
    }
}

bool SameFileFlags(int before, int after) {
    // Darwin's Kernel.framework/sys/fcntl.h defines FWASWRITTEN; the isolated write control proves it is added.
    constexpr int KernelWrittenFlag = 0x10000;
    return after == before || after == (before | KernelWrittenFlag);
}

void EmitMode(const char* stage, const termios& mode) {
    std::fprintf(stderr, "MODE %s iflag=%lx oflag=%lx cflag=%lx lflag=%lx ispeed=%lu ospeed=%lu cc=", stage,
        static_cast<unsigned long>(mode.c_iflag), static_cast<unsigned long>(mode.c_oflag),
        static_cast<unsigned long>(mode.c_cflag), static_cast<unsigned long>(mode.c_lflag),
        static_cast<unsigned long>(cfgetispeed(&mode)), static_cast<unsigned long>(cfgetospeed(&mode)));
    for (unsigned char character : mode.c_cc) {
        std::fprintf(stderr, "%02x", character);
    }
    std::fputc('\n', stderr);
}

int duplicateCalls = 0;
int failedDuplicate = 0;
int interruptedReads = 0;
int interruptedWrites = 0;
bool shortWrites = false;
CTextConsolePosix* activeConsole = nullptr;
int readCalls = 0;
int failedRead = 0;

class ControlledSystem : public IBaseSystem {
public:
    bool Init(IBaseSystem*, int, char*) override {
        return true;
    }

    void RunFrame(double) override {}

    void ReceiveSignal(ISystemModule*, unsigned int, void*) override {}

    void ExecuteCommand(int, char*) override {}

    void RegisterListener(ISystemModule*) override {}

    void RemoveListener(ISystemModule*) override {}

    IBaseSystem* GetSystem() override {
        return this;
    }

    int GetSerial() override {
        return 1;
    }

    char* GetStatusLine() override {
        return name;
    }

    char* GetType() override {
        return name;
    }

    char* GetName() override {
        return name;
    }

    int GetState() override {
        return 3;
    }

    int GetVersion() override {
        return 1;
    }

    void ShutDown() override {}

    void* QueryInterface(const char*) override {
        return nullptr;
    }

    double GetTime() override {
        return 0;
    }

    unsigned int GetTick() override {
        return 0;
    }

    void SetFPS(float) override {}

    void Printf(char*, ...) override {
        ++warnings;
    }

    void DPrintf(char*, ...) override {}

    void RedirectOutput(char*, int) override {}

    IFileSystem* GetFileSystem() override {
        return nullptr;
    }

    unsigned char* LoadFile(const char*, int*) override {
        return nullptr;
    }

    void FreeFile(unsigned char*) override {}

    void SetTitle(char*) override {}

    void SetStatusLine(char*) override {}

    void ShowConsole(bool) override {}

    void LogConsole(char*) override {}

    bool InitVGUI(IVGuiModule*) override {
        return false;
    }

    bool RegisterCommand(char*, ISystemModule*, int) override {
        return true;
    }

    void GetCommandMatches(char* prefix, ObjectList* matches) override {
        ++completionCalls;
        if (std::strncmp(command, prefix, std::strlen(prefix)) == 0) {
            matches->AddTail(command);
        }
    }

    void ExecuteString(char*) override {}

    void ExecuteFile(char*) override {}

    void Errorf(char*, ...) override {
        ++errors;
    }

    char* CheckParam(char*) override {
        return nullptr;
    }

    bool AddModule(ISystemModule*, char*) override {
        return false;
    }

    ISystemModule* GetModule(char*, char*, char*) override {
        return nullptr;
    }

    bool RemoveModule(ISystemModule*) override {
        return false;
    }

    void Stop() override {
        ++stops;
    }

    char* COM_GetBaseDir() override {
        return name;
    }


    int completionCalls = 0;
    int stops = 0;
    int errors = 0;
    int warnings = 0;
    char name[8] = "fixture";
    char command[7] = "status";
};

int DescriptorCount() {
    int count = 0;
    constexpr int CheckedDescriptors = 256;
    for (int descriptor = 0; descriptor < CheckedDescriptors; ++descriptor) {
        if (fcntl(descriptor, F_GETFD) >= 0) {
            ++count;
        }
    }
    return count;
}

void EmitLine(const char* line) {
    std::fputs("LINE ", stderr);
    for (const unsigned char* byte = reinterpret_cast<const unsigned char*>(line); *byte; ++byte) {
        std::fprintf(stderr, "%02x", *byte);
    }
    std::fputc('\n', stderr);
    std::fflush(stderr);
}

}

// Only allocation for the actual retained ObjectList nodes is controlled; no module/provider is loaded.
extern "C" void* Mem_ZeroMalloc(unsigned int size) {
    return std::calloc(1, size);
}

void System_Printf(const char* format, ...) {
    char text[8192] = {};
    va_list arguments;
    va_start(arguments, format);
    std::vsnprintf(text, sizeof(text), format, arguments);
    va_end(arguments);
    Require(activeConsole != nullptr, "controlled frontend output has the real active console");
    activeConsole->Print(text);
}

extern "C" int CseConsoleDuplicate(int descriptor) {
    ++duplicateCalls;
    if (duplicateCalls == failedDuplicate) {
        errno = EMFILE;
        return -1;
    }
    return dup(descriptor);
}

extern "C" ssize_t CseConsoleRead(int descriptor, void* buffer, std::size_t size) {
    ++readCalls;
    if (readCalls == failedRead) {
        errno = EIO;
        return -1;
    }
    if (interruptedReads > 0) {
        --interruptedReads;
        errno = EINTR;
        return -1;
    }
    return read(descriptor, buffer, size);
}

extern "C" ssize_t CseConsoleWrite(int descriptor, const void* buffer, std::size_t size) {
    if (interruptedWrites > 0) {
        --interruptedWrites;
        errno = EINTR;
        return -1;
    }
    return write(descriptor, buffer, shortWrites && size > 1 ? 1 : size);
}

int main(int argc, char** argv) {
    Require(argc == 3, "case and expected line count");
    hltv_posix::SignalScope signals;
    Require(signals.Install(), "install fixture-main signal lifetime");
    const int expectedLines = std::atoi(argv[2]);
    const int originalInputFlags = fcntl(STDIN_FILENO, F_GETFL);
    const int originalOutputFlags = fcntl(STDOUT_FILENO, F_GETFL);
    std::fprintf(stderr, "FLAGS before stdin=%x stdout=%x\n", originalInputFlags, originalOutputFlags);
    const int originalDescriptors = DescriptorCount();
    const bool terminal = isatty(STDIN_FILENO) != 0;
    termios originalMode = {};
    if (terminal) {
        Require(tcgetattr(STDIN_FILENO, &originalMode) == 0, "capture owned terminal mode");
        EmitMode("before", originalMode);
    }
    ControlledSystem system;
    CTextConsolePosix console;
    activeConsole = &console;
    if (std::strcmp(argv[1], "dup-input-error") == 0 || std::strcmp(argv[1], "dup-output-error") == 0) {
        failedDuplicate = std::strcmp(argv[1], "dup-input-error") == 0 ? 1 : 2;
        Require(!console.Init(&system) && errno == EMFILE, "descriptor failure rejects and preserves errno");
        console.ShutDown();
        Require(DescriptorCount() == originalDescriptors, "partial initialization releases only owned descriptors");
        return EXIT_SUCCESS;
    }
    interruptedReads = std::strcmp(argv[1], "interrupted") == 0 ? 1 : 0;
    interruptedWrites = interruptedReads;
    shortWrites = interruptedReads != 0;
    failedRead = std::strcmp(argv[1], "read-error") == 0 ? 4 : 0;
    Require(console.Init(&system), "initialize actual POSIX console");
    Require(SameFileFlags(originalInputFlags, fcntl(STDIN_FILENO, F_GETFL)),
        "init leaves shared stdin status flags unchanged");
    if (terminal) {
        termios activeMode = {};
        Require(tcgetattr(STDIN_FILENO, &activeMode) == 0, "read active terminal mode");
        EmitMode("active", activeMode);
        Require(!(activeMode.c_lflag & (ICANON | ECHO)), "owned terminal uses manual line editing/echo");
    }
    char title[] = "HLTV fixture";
    char status[] = "server status";
    console.SetTitle(title);
    console.SetStatusLine(status);
    if (std::strcmp(argv[1], "hidden") == 0) {
        console.SetVisible(false);
    }
    char printed[] = "backend message\n";
    console.Print(printed);
    std::fputs("READY\n", stderr);
    std::fflush(stderr);
    if (std::strcmp(argv[1], "blocked-output") == 0) {
        constexpr std::size_t BlockedOutputBytes = 1024 * 1024;
        std::string payload(BlockedOutputBytes, 'x');
        console.PrintRaw(&payload[0], static_cast<int>(payload.size()));
        Require(hltv_posix::ShutdownRequested != 0, "real signal unblocks output through its atomic flag");
    }
    int lines = 0;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
    while (lines < expectedLines && std::chrono::steady_clock::now() < deadline) {
        const auto before = std::chrono::steady_clock::now();
        char* line = console.GetLine();
        Require(std::chrono::steady_clock::now() - before < std::chrono::milliseconds(100), "GetLine never blocks");
        if (line != nullptr) {
            EmitLine(line);
            ++lines;
        } else {
            usleep(1000);
        }
    }
    Require(lines == expectedLines, "all queued lines are returned");
    if (std::strcmp(argv[1], "completion") == 0) {
        Require(system.completionCalls == 1, "tab uses the actual base completion callback");
    }
    if (std::strcmp(argv[1], "eof") == 0 || std::strcmp(argv[1], "no-input") == 0
        || std::strcmp(argv[1], "read-error") == 0) {
        const auto before = std::chrono::steady_clock::now();
        constexpr int IdleFrames = 1000;
        for (int frame = 0; frame < IdleFrames; ++frame) {
            Require(console.GetLine() == nullptr, "idle/EOF does not invent or repeat a command");
        }
        Require(std::chrono::steady_clock::now() - before < std::chrono::milliseconds(200), "idle/EOF does not spin");
    }
    if (failedRead != 0) {
        Require(system.warnings == 1 && readCalls == failedRead, "read error discards partial input and warns only once");
    }
    Require(system.stops == 0 && system.errors == 0, "console/EOF does not stop server or call fatal error");
    console.ShutDown();
    console.ShutDown();
    const int inputFlags = fcntl(STDIN_FILENO, F_GETFL);
    Require(inputFlags >= 0 && SameFileFlags(originalInputFlags, inputFlags),
        "shutdown leaves inherited stdin descriptor/status flags");
    // Compare every bit, permitting only the independently demonstrated kernel-maintained write flag.
    const int outputFlags = fcntl(STDOUT_FILENO, F_GETFL);
    Require(outputFlags >= 0 && SameFileFlags(originalOutputFlags, outputFlags),
        "shutdown leaves inherited stdout descriptor/status flags");
    std::fprintf(stderr, "FLAGS restored stdin=%x stdout=%x\n", inputFlags, outputFlags);
    Require(DescriptorCount() == originalDescriptors, "idempotent cleanup releases duplicated descriptors");
    if (terminal) {
        termios restoredMode = {};
        Require(tcgetattr(STDIN_FILENO, &restoredMode) == 0, "read restored terminal mode");
        EmitMode("restored", restoredMode);
        // Darwin adds PENDIN when canonical mode resumes, even in the isolated tcsetattr control fixture.
        const bool localFlagsRestored = restoredMode.c_lflag == originalMode.c_lflag
            || restoredMode.c_lflag == (originalMode.c_lflag | PENDIN);
        Require(originalMode.c_iflag == restoredMode.c_iflag && originalMode.c_oflag == restoredMode.c_oflag
            && originalMode.c_cflag == restoredMode.c_cflag && localFlagsRestored
            && cfgetispeed(&originalMode) == cfgetispeed(&restoredMode)
            && cfgetospeed(&originalMode) == cfgetospeed(&restoredMode)
            && std::memcmp(originalMode.c_cc, restoredMode.c_cc, sizeof(originalMode.c_cc)) == 0,
            "all terminal flags, speeds and control bytes restored");
    }
    std::fputs("DONE\n", stderr);
    return EXIT_SUCCESS;
}
