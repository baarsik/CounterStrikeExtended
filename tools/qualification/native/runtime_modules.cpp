// Build in the x86 MSVC environment: cl /nologo /W4 /WX /EHsc runtime_modules.cpp
// Usage: runtime_modules.exe PID [UTF8-output-file]
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <tlhelp32.h>

#include <cerrno>
#include <cstdio>
#include <cwchar>
#include <limits>
#include <string>

int wmain(int argc, wchar_t* argv[]) {
    constexpr int requiredArguments = 2;
    constexpr int outputFileArguments = 3;
    constexpr int decimalBase = 10;
    if (argc != requiredArguments && argc != outputFileArguments) {
        std::fwprintf(stderr, L"Usage: runtime_modules.exe PID [UTF8-output-file]\n");
        return 1;
    }
    wchar_t* end = nullptr;
    errno = 0;
    unsigned long parsedPid = std::wcstoul(argv[1], &end, decimalBase);
    if (errno != 0 || end == argv[1] || *end != L'\0' || argv[1][0] == L'-'
        || parsedPid == 0 || parsedPid > std::numeric_limits<DWORD>::max()) {
        std::fwprintf(stderr, L"Invalid PID\n");
        return 1;
    }
    DWORD processId = static_cast<DWORD>(parsedPid);
    HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, processId);
    DWORD exitCode = 0;
    if (!process) {
        std::fwprintf(stderr, L"OpenProcess failed: %lu\n", GetLastError());
        return 1;
    }
    bool running = GetExitCodeProcess(process, &exitCode) && exitCode == STILL_ACTIVE;
    CloseHandle(process);
    if (!running) {
        std::fwprintf(stderr, L"PID is not running\n");
        return 1;
    }
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, processId);
    if (snapshot == INVALID_HANDLE_VALUE) {
        std::fwprintf(stderr, L"CreateToolhelp32Snapshot failed: %lu\n", GetLastError());
        return 1;
    }
    FILE* output = stdout;
    if (argc == outputFileArguments && _wfopen_s(&output, argv[2], L"wb") != 0) {
        std::fwprintf(stderr, L"Cannot create module output file\n");
        CloseHandle(snapshot);
        return 1;
    }
    MODULEENTRY32W entry = {};
    entry.dwSize = sizeof(entry);
    bool succeeded = Module32FirstW(snapshot, &entry) != FALSE;
    if (!succeeded) {
        std::fwprintf(stderr, L"Module32First failed: %lu\n", GetLastError());
    }
    while (succeeded) {
        int bytes = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, entry.szExePath, -1,
            nullptr, 0, nullptr, nullptr);
        if (bytes <= 0) {
            succeeded = false;
            break;
        }
        std::string path(static_cast<size_t>(bytes), '\0');
        if (!WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, entry.szExePath, -1,
            &path[0], bytes, nullptr, nullptr)
            || std::fwrite(path.data(), 1, path.size() - 1, output) != path.size() - 1
            || std::fputc('\n', output) == EOF) {
            succeeded = false;
            break;
        }
        if (!Module32NextW(snapshot, &entry)) {
            succeeded = GetLastError() == ERROR_NO_MORE_FILES;
            break;
        }
    }
    CloseHandle(snapshot);
    if (output != stdout && std::fclose(output) != 0) {
        succeeded = false;
    }
    return succeeded ? 0 : 1;
}
