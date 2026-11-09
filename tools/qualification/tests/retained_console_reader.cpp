// GUI fixture for exact extracted retained GetLine, ReceiveNewline and character bodies.
#include <windows.h>
#include <shellapi.h>

#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>

constexpr int MAX_CONSOLE_TEXTLEN = 256;
constexpr int MAX_BUFFER_LINES = 30;
constexpr DWORD MAX_CONSOLE_EVENTS = 1024;

class IBaseSystem {
public:
    void Errorf(char *, ...) {
        std::abort();
    }
};

void Sys_Printf(char *, ...) {
}

class CTextConsole {
public:
    bool Init(IBaseSystem *system);
    int ReceiveNewline();
    void ReceiveStandardChar(char character);
    void ReceiveUpArrow() {
        std::abort();
    }
    void ReceiveDownArrow() {
        std::abort();
    }
    void ReceiveLeftArrow() {
        std::abort();
    }
    void ReceiveRightArrow() {
        std::abort();
    }
    void ReceiveBackspace() {
        std::abort();
    }
    void ReceiveTab() {
        std::abort();
    }
    void Echo(char *text, int length = 0) {
        DWORD written = 0;
        const DWORD count = length ? static_cast<DWORD>(length) : static_cast<DWORD>(std::strlen(text));
        WriteConsoleA(GetStdHandle(STD_OUTPUT_HANDLE), text, count, &written, nullptr);
    }

protected:
    char m_szConsoleText[MAX_CONSOLE_TEXTLEN];
    int m_nConsoleTextLen;
    int m_nCursorPosition;
    char m_szSavedConsoleText[MAX_CONSOLE_TEXTLEN];
    int m_nSavedConsoleTextLen;
    char m_aszLineBuffer[MAX_BUFFER_LINES][MAX_CONSOLE_TEXTLEN];
    int m_nInputLine;
    int m_nBrowseLine;
    int m_nTotalLines;
    bool m_ConsoleVisible;
    IBaseSystem *m_System;
};

class CTextConsoleWin32 : public CTextConsole {
public:
    CTextConsoleWin32() : m_hInput(GetStdHandle(STD_INPUT_HANDLE)) {
    }
    char *GetLine();

private:
    HANDLE m_hInput;
};

#include "retained_reader_bodies.inc"

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
    int count = 0;
    wchar_t **arguments = CommandLineToArgvW(GetCommandLineW(), &count);
    if (!arguments || count != 3) {
        return 2;
    }
    const std::filesystem::path receipt(arguments[1]);
    const bool gated = std::wstring(arguments[2]) == L"gated";
    LocalFree(arguments);
    if (!AllocConsole()) {
        return 3;
    }
    CTextConsoleWin32 reader;
    reader.Init(nullptr);
    reader.Echo(const_cast<char *>("CSE_RETAINED_READER_READY\r\n"));
    const std::filesystem::path gate = receipt.wstring() + L".go";
    while (gated && !std::filesystem::exists(gate)) {
        Sleep(1);
    }
    for (;;) {
        if (char *line = reader.GetLine()) {
            std::ofstream file(receipt, std::ios::app | std::ios::binary);
            file << line << '\n';
            file.close();
            reader.Echo(const_cast<char *>("CSE_RETAINED_LINE:"));
            reader.Echo(line);
            reader.Echo(const_cast<char *>("\r\n"));
            if (std::strcmp(line, "quit") == 0) {
                FreeConsole();
                return 0;
            }
        }
        Sleep(1);
    }
}
