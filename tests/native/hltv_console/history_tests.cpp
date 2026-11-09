#include "TextConsole.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <algorithm>
#include <string>

void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "FAIL %s\n", message);
        std::exit(EXIT_FAILURE);
    }
}

// This isolates the actual retained base handlers from every POSIX backend/helper implementation.
class ScriptedConsole : public CTextConsole {
public:
    void PrintRaw(char*, int = 0) override {}
    void Echo(char*, int = 0) override {}
    char* GetLine() override {
        return nullptr;
    }

    int GetWidth() override {
        constexpr int ConsoleWidth = 80;
        return ConsoleWidth;
    }

    void Type(const char* text) {
        for (; *text; ++text) {
            HandleKeyChar(*text);
        }
    }

    void Up() {
        HandleKeyUp();
    }

    void Down() {
        HandleKeyDown();
    }

    void Left() {
        HandleKeyLeft();
    }

    int Enter() {
        return HandleKeyEnter();
    }

    void Report() {
        std::fprintf(stderr, "browse=%d count=%d next=%d saved=%d current=%d\n", m_BrowseLine,
            m_HistoryCount, m_NextHistoryLine, m_SavedConsoleLineLen, m_ConsoleLineLen);
    }

    std::string Text() const {
        Require(m_ConsoleLineLen >= 0 && m_ConsoleLineLen < MAX_CONSOLE_LINE, "line length stays in storage");
        Require(m_BrowseLine >= 0 && m_BrowseLine < MAX_CONSOLE_HISTORY, "history browse index stays inside ring");
        return std::string(m_ConsoleLine, static_cast<std::size_t>(m_ConsoleLineLen));
    }

    int Caret() const {
        Require(m_CursorPosition >= 0 && m_CursorPosition <= m_ConsoleLineLen, "caret stays inside current line");
        return m_CursorPosition;
    }

    int Next() const {
        return m_NextHistoryLine;
    }
};

void System_Printf(const char*, ...) {}

extern "C" void* Mem_ZeroMalloc(unsigned int size) {
    return std::calloc(1, size);
}

void Fill(ScriptedConsole& console, int entries) {
    Require(console.Init(nullptr), "initialize actual base console");
    Require(console.Enter() == 0, "empty Enter creates no history");
    for (int index = 0; index < entries; ++index) {
        char line[32];
        std::snprintf(line, sizeof(line), "cmd%d", index);
        console.Type(line);
        console.Enter();
    }
}

void Draft(int entries) {
    ScriptedConsole console;
    Fill(console, entries);
    console.Type("saved");
    console.Left();
    console.Left();
    Require(console.Caret() == 3, "draft caret can start in the middle");
    console.Up();
    Require(console.Text() == "cmd" + std::to_string(entries - 1), "Up returns latest history entry");
    console.Down();
    Require(console.Text() == "saved", "Down restores saved draft");
    // Preserve the existing policy: history navigation restores the draft caret at its end.
    Require(console.Caret() == 5, "draft caret retains legacy end-of-line restoration");
    const int length = console.Enter();
    Require(length == static_cast<int>(sizeof("saved") - 1), "Enter returns the complete restored draft");
    const int next = console.Next();
    Require(console.Enter() == 0 && console.Next() == next && console.Caret() == 0,
        "empty Enter leaves history/caret stable");
}

void Traverse(int entries) {
    ScriptedConsole console;
    Fill(console, entries);
    console.Type("saved");
    console.Left();
    // The retained ring reserves the next slot as the draft sentinel, so a full ring exposes 29 entries.
    const int browsable = (std::min)(entries, MAX_CONSOLE_HISTORY - 1);
    for (int index = 1; index <= browsable; ++index) {
        console.Up();
        Require(console.Text() == "cmd" + std::to_string(entries - index), "Up traverses retained history order");
        Require(console.Caret() == static_cast<int>(console.Text().size()), "history caret stays at line end");
    }
    const std::string oldest = console.Text();
    console.Up();
    Require(console.Text() == oldest, "Up stops at the existing oldest browse boundary");
    for (int index = 1; index < browsable; ++index) {
        console.Down();
        Require(console.Text() == "cmd" + std::to_string(entries - browsable + index),
            "Down traverses retained history order across ring wrap");
    }
    console.Down();
    Require(console.Text() == "saved" && console.Caret() == 5, "full traversal restores draft and legacy caret");
    console.Down();
    Require(console.Text() == "saved", "Down stops at the draft sentinel");
    console.Left();
    console.Left();
    console.Type("X");
    Require(console.Text() == "savXed" && console.Caret() == 4, "editing after restoration retains insertion behavior");
}

int main(int argc, char** argv) {
    constexpr int EntryCounts[] = {29, 30, 31, 59, 60};
    for (int entries : EntryCounts) {
        if (argc == 2 && entries != std::atoi(argv[1])) {
            continue;
        }
        Draft(entries);
        Traverse(entries);
        std::printf("PASS history entries=%d\n", entries);
    }
    return EXIT_SUCCESS;
}
