#if !defined(_WIN32)
#error The conproc worker fixture requires actual Windows APIs
#endif
#include "dedicated/console/conproc.cpp"

#include <cstdio>
#include <string>

void Sys_Printf(char*, ...) {}

namespace {
constexpr DWORD MappingBytes = 4096;
constexpr DWORD AcknowledgementTimeoutMilliseconds = 5000;
constexpr int RoundtripIterations = 50;

std::string CommandForMapping(const std::string& mappingValue, HANDLE parent, HANDLE child) {
    return "hlds -HFILE " + mappingValue
        + " -HPARENT " + std::to_string(reinterpret_cast<std::uintptr_t>(parent))
        + " -HCHILD " + std::to_string(reinterpret_cast<std::uintptr_t>(child));
}

bool JoinedAndCleared() {
    HANDLE observed = nullptr;
    if (!workerThread || !DuplicateHandle(GetCurrentProcess(), workerThread, GetCurrentProcess(), &observed,
        SYNCHRONIZE, FALSE, 0)) {
        return false;
    }
    DeinitConProc();
    const bool joined = WaitForSingleObject(observed, 0) == WAIT_OBJECT_0;
    CloseHandle(observed);
    return joined && !workerThread && !heventDone && !hfileBuffer && !heventParentSend
        && !heventChildSend && !hStdout && !hStdin;
}

bool BorrowedHandlesRemainOpen(HANDLE mapping, HANDLE parent, HANDLE child) {
    DWORD flags = 0;
    return GetHandleInformation(mapping, &flags) && GetHandleInformation(parent, &flags)
        && GetHandleInformation(child, &flags);
}

int RunRequests(HANDLE mapping, HANDLE parent, HANDLE child, int* buffer) {
    const std::string command = CommandForMapping(
        std::to_string(reinterpret_cast<std::uintptr_t>(mapping)), parent, child);
    cmdline->CreateCmdLine(command.c_str());
    for (int iteration = 0; iteration < RoundtripIterations; ++iteration) {
        InitConProc();
        if (!workerThread) {
            return 2;
        }
        buffer[0] = CCOM_GET_SCR_LINES;
        buffer[1] = 0;
        if (!SetEvent(parent)
            || WaitForSingleObject(child, AcknowledgementTimeoutMilliseconds) != WAIT_OBJECT_0) {
            return 3;
        }
        // Acknowledgement must follow the worker's mapped status response, before shutdown.
        if (buffer[0] != TRUE || buffer[1] <= 0) {
            return 4;
        }
        if (!JoinedAndCleared()) {
            return 5;
        }
        if (!BorrowedHandlesRemainOpen(mapping, parent, child)) {
            return 6;
        }
    }

#if defined(_WIN64)
    // This unissued token exercises full-bit argument handoff, not valid high-valued mapping I/O.
    constexpr std::uintptr_t HighMappingToken = 0x100001234ULL;
    const std::string highCommand = CommandForMapping(std::to_string(HighMappingToken), parent, child);
    cmdline->CreateCmdLine(highCommand.c_str());
    if (!ResetEvent(parent) || !ResetEvent(child)) {
        return 7;
    }
    InitConProc();
    if (!workerThread || reinterpret_cast<std::uintptr_t>(hfileBuffer) != HighMappingToken
        || heventParentSend != parent || heventChildSend != child) {
        return 8;
    }
    // Deliberately dispatch no request: the worker only receives its real stop event.
    if (!JoinedAndCleared() || WaitForSingleObject(child, 0) != WAIT_TIMEOUT
        || !BorrowedHandlesRemainOpen(mapping, parent, child)) {
        return 9;
    }
#endif
    return 0;
}
}

int main() {
    const bool allocatedConsole = AllocConsole() != FALSE;
    const HANDLE previousOutput = GetStdHandle(STD_OUTPUT_HANDLE);
    // CTest may redirect stdout. Supply a real console buffer for the retained status command.
    HANDLE consoleOutput = CreateConsoleScreenBuffer(GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, CONSOLE_TEXTMODE_BUFFER, nullptr);
    HANDLE mapping = CreateFileMappingA(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0, MappingBytes, nullptr);
    HANDLE parent = CreateEventA(nullptr, FALSE, FALSE, nullptr);
    HANDLE child = CreateEventA(nullptr, FALSE, FALSE, nullptr);
    int* buffer = mapping ? static_cast<int*>(MapViewOfFile(mapping, FILE_MAP_READ | FILE_MAP_WRITE,
        0, 0, MappingBytes)) : nullptr;
    int result = 1;
    const bool outputInstalled = consoleOutput != INVALID_HANDLE_VALUE
        && SetStdHandle(STD_OUTPUT_HANDLE, consoleOutput) != FALSE;
    if (outputInstalled && mapping && parent && child && buffer) {
        result = RunRequests(mapping, parent, child, buffer);
    }

    // Every failure path also joins before releasing any worker-borrowed resource.
    DeinitConProc();
    if (buffer) {
        UnmapViewOfFile(buffer);
    }
    if (mapping) {
        CloseHandle(mapping);
    }
    if (parent) {
        CloseHandle(parent);
    }
    if (child) {
        CloseHandle(child);
    }
    if (outputInstalled) {
        SetStdHandle(STD_OUTPUT_HANDLE, previousOutput);
    }
    if (consoleOutput != INVALID_HANDLE_VALUE) {
        CloseHandle(consoleOutput);
    }
    if (allocatedConsole) {
        FreeConsole();
    }
    if (result != 0) {
        std::fprintf(stderr, "conproc Windows fixture failed at stage %d\n", result);
    }
    return result;
}
