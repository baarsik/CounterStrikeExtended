// Private GUI-subsystem control of the retained AllocConsole/GetStdHandle path.
#include <windows.h>
#include <shellapi.h>

#include <filesystem>
#include <fstream>
#include <string>

namespace {

constexpr DWORD InputRecordBatch = 32;
constexpr std::size_t MaximumInputCharacters = 128;

void Report(const std::filesystem::path &path, bool allocated, bool input, bool output, const std::string &line) {
    std::ofstream file(path, std::ios::binary);
    file << "{\"allocated_console\":" << (allocated ? "true" : "false")
         << ",\"input_console_mode\":" << (input ? "true" : "false")
         << ",\"output_console_mode\":" << (output ? "true" : "false")
         << ",\"received_line\":\"" << line << "\"}\n";
}

template<std::size_t Count>
void Output(const char (&text)[Count]) {
    static_assert(Count - 1 <= MAXDWORD, "Private console control literal fits DWORD");
    DWORD count = 0;
    WriteConsoleA(GetStdHandle(STD_OUTPUT_HANDLE), text, static_cast<DWORD>(Count - 1), &count, nullptr);
}

} // namespace

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
    int count = 0;
    wchar_t **arguments = CommandLineToArgvW(GetCommandLineW(), &count);
    if (!arguments || count != 2) {
        return 2;
    }
    const std::filesystem::path receipt(arguments[1]);
    LocalFree(arguments);
    const bool allocated = AllocConsole() != FALSE;
    DWORD mode = 0;
    const bool input = GetConsoleMode(GetStdHandle(STD_INPUT_HANDLE), &mode) != FALSE;
    const bool output = GetConsoleMode(GetStdHandle(STD_OUTPUT_HANDLE), &mode) != FALSE;
    Report(receipt, allocated, input, output, {});
    if (!input || !output) {
        FreeConsole();
        return 3;
    }
    Output("CSE_GUI_CONSOLE_READY\r\n");
    std::string line;
    for (;;) {
        INPUT_RECORD records[InputRecordBatch] = {};
        DWORD read = 0;
        if (!ReadConsoleInputA(GetStdHandle(STD_INPUT_HANDLE), records, InputRecordBatch, &read)) {
            FreeConsole();
            return 4;
        }
        for (DWORD index = 0; index < read; ++index) {
            if (records[index].EventType != KEY_EVENT || !records[index].Event.KeyEvent.bKeyDown) {
                continue;
            }
            const char character = records[index].Event.KeyEvent.uChar.AsciiChar;
            if (character == '\r') {
                Report(receipt, allocated, input, output, line);
                Output("CSE_GUI_CONSOLE_RECEIVED\r\n");
                FreeConsole();
                return line == "CSE_GUI_INPUT" || line == "quit" ? 0 : 5;
            }
            if (character >= ' ' && character <= '~' && line.size() < MaximumInputCharacters) {
                line.push_back(character);
            }
        }
    }
}
