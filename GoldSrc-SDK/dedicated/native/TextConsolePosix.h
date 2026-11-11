#pragma once

#if !defined(_WIN32)

#include "../../common/textconsole.h"

#include <cstddef>
#include <string>
#include <termios.h>

class CTextConsolePosix : public CTextConsole {
public:
    ~CTextConsolePosix() override;
    bool Init(IBaseSystem* system) override;
    void ShutDown() override;
    void RestoreTerminalMode() noexcept;
    void Print(char* text) override;
    void SetTitle(char* title) override;
    void SetStatusLine(char* status) override;
    void UpdateStatus() override;
    void PrintRaw(char* text, int length = 0) override;
    void Echo(char* text, int length = 0) override;
    char* GetLine() override;
    int GetWidth() override;
    bool InputEnded() const noexcept {
        return m_InputEnded;
    }
    bool InputFailed() const noexcept {
        return m_InputFailed;
    }
    bool OutputFailed() const noexcept {
        return m_OutputFailed;
    }
    void SetVisible(bool visible) override;

private:
    static constexpr int InvalidDescriptor = -1;
    static constexpr std::size_t StatusLineCapacity = 80;

    enum class EscapeState { None, Escape, Sequence };

    bool WriteBytes(const char* text, std::size_t length);
    void RenderTitle();
    char* FinishInput(bool completeLine);

    int m_Input = InvalidDescriptor;
    int m_Output = InvalidDescriptor;
    bool m_InputTerminal = false;
    bool m_OutputTerminal = false;
    bool m_SavedTerminalMode = false;
    bool m_InputEnded = false;
    bool m_InputFailed = false;
    bool m_OutputFailed = false;
    bool m_Initialized = false;
    bool m_SkipLineFeed = false;
    bool m_SequenceParameters = false;
    EscapeState m_EscapeState = EscapeState::None;
    termios m_OriginalMode = {};
    std::string m_Title;
    char m_StatusLine[StatusLineCapacity] = {};
};

#endif
