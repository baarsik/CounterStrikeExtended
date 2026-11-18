#if !defined(_WIN32)

#include "TextConsolePosix.h"
#include "../../common/IBaseSystem.h"
#include "../../common/posix_console_input.h"
#include "Signals.h"

#include <poll.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <cstring>

CTextConsolePosix::~CTextConsolePosix() {
    ShutDown();
}

bool CTextConsolePosix::Init(IBaseSystem* system) {
    ShutDown();
    m_ConsoleVisible = true;
    m_Input = dup(STDIN_FILENO);
    if (m_Input < 0 && errno != EBADF) {
        return false;
    }
    m_Output = dup(STDOUT_FILENO);
    if (m_Output < 0 && errno != EBADF) {
        const int error = errno;
        ShutDown();
        errno = error;
        return false;
    }
    m_InputTerminal = m_Input >= 0 && isatty(m_Input);
    m_InputEnded = m_Input < 0 || (!m_InputTerminal && cse::console::IsNullDevice(m_Input));
    m_OutputTerminal = m_Output >= 0 && isatty(m_Output);
    m_OutputFailed = m_Output < 0;
    m_InputFailed = false;
    m_SkipLineFeed = false;
    m_EscapeState = EscapeState::None;
    if (m_InputTerminal) {
        if (tcgetattr(m_Input, &m_OriginalMode) != 0) {
            const int error = errno;
            ShutDown();
            errno = error;
            return false;
        }
        termios mode = m_OriginalMode;
        m_SavedTerminalMode = true;
        mode.c_lflag &= ~(ICANON | ECHO);
        mode.c_iflag &= ~(ICRNL | INLCR | IGNCR | ISTRIP | IXON);
        mode.c_cc[VMIN] = 1;
        mode.c_cc[VTIME] = 0;
        if (tcsetattr(m_Input, TCSANOW, &mode) != 0) {
            const int error = errno;
            ShutDown();
            errno = error;
            return false;
        }
    }
    m_Initialized = true;
    return CTextConsole::Init(system);
}

void CTextConsolePosix::RestoreTerminalMode() noexcept {
    const int savedError = errno;
    if (m_SavedTerminalMode) {
        while (tcsetattr(m_Input, TCSANOW, &m_OriginalMode) != 0 && errno == EINTR) {
        }
        m_SavedTerminalMode = false;
    }
    errno = savedError;
}

void CTextConsolePosix::ShutDown() {
    const int savedError = errno;
    RestoreTerminalMode();
    if (m_Input >= 0) {
        close(m_Input);
        m_Input = InvalidDescriptor;
    }
    if (m_Output >= 0) {
        close(m_Output);
        m_Output = InvalidDescriptor;
    }
    m_InputEnded = true;
    m_Initialized = false;
    CTextConsole::ShutDown();
    errno = savedError;
}

bool CTextConsolePosix::WriteBytes(const char* text, std::size_t length) {
    if (m_Output < 0 || m_OutputFailed || (m_OutputTerminal && !m_ConsoleVisible)) {
        return false;
    }
    const int savedError = errno;
    std::size_t written = 0;
    while (written < length) {
        if (cse::dedicated::ShutdownRequested) {
            // Preserve final engine diagnostics when the real output is ready,
            // but do not begin a blocked write after a stop signal.
            pollfd output{m_Output, POLLOUT, 0};
            if (poll(&output, 1, 0) <= 0 || !(output.revents & POLLOUT)) {
                m_OutputFailed = true;
                return false;
            }
        }
        const ssize_t count = write(m_Output, text + written, length - written);
        if (count > 0) {
            written += static_cast<std::size_t>(count);
        } else if (count < 0 && errno == EINTR) {
            if (cse::dedicated::ShutdownRequested) {
                m_OutputFailed = true;
                return false;
            }
            continue;
        } else {
            m_OutputFailed = true;
            return false;
        }
    }
    errno = savedError;
    return true;
}

void CTextConsolePosix::Print(char* text) {
    if (m_InputTerminal && m_OutputTerminal) {
        CTextConsole::Print(text);
    } else {
        PrintRaw(text);
    }
}

void CTextConsolePosix::PrintRaw(char* text, int length) {
    if (text == nullptr || length < 0) {
        return;
    }
    WriteBytes(text, length != 0 ? static_cast<std::size_t>(length) : std::strlen(text));
}

void CTextConsolePosix::Echo(char* text, int length) {
    if (m_InputTerminal) {
        PrintRaw(text, length);
    }
}

void CTextConsolePosix::SetTitle(char* title) {
    m_Title = title != nullptr ? title : "";
    RenderTitle();
}

void CTextConsolePosix::RenderTitle() {
    if (!m_OutputTerminal || !m_ConsoleVisible) {
        return;
    }
    constexpr char TitlePrefix[] = "\033]0;";
    constexpr char TitleEnd[] = "\007";
    if (!WriteBytes(TitlePrefix, sizeof(TitlePrefix) - 1)) {
        return;
    }
    for (char character : m_Title) {
        // Keep terminal framing bytes out of the title while retaining UTF8 bytes.
        if (static_cast<unsigned char>(character) >= ' ' && character != '\177') {
            if (!WriteBytes(&character, 1)) {
                return;
            }
        }
    }
    WriteBytes(TitleEnd, sizeof(TitleEnd) - 1);
}

void CTextConsolePosix::SetStatusLine(char* status) {
    const std::size_t length = status != nullptr ? std::strlen(status) : 0;
    const std::size_t copied = (std::min)(length, sizeof(m_StatusLine) - 1);
    if (copied > 0) {
        for (std::size_t index = 0; index < copied; ++index) {
            const unsigned char character = static_cast<unsigned char>(status[index]);
            m_StatusLine[index] = character >= ' ' && character != '\177' ? status[index] : ' ';
        }
    }
    m_StatusLine[copied] = 0;
    UpdateStatus();
}

void CTextConsolePosix::UpdateStatus() {
    if (!m_OutputTerminal || !m_ConsoleVisible) {
        return;
    }
    constexpr char SaveAndPosition[] = "\0337\033[1;1H";
    constexpr char ClearAndRestore[] = "\033[K\0338";
    const std::size_t width = static_cast<std::size_t>(GetWidth() - 1);
    const std::size_t length = (std::min)(std::strlen(m_StatusLine), width);
    if (WriteBytes(SaveAndPosition, sizeof(SaveAndPosition) - 1) && WriteBytes(m_StatusLine, length)) {
        WriteBytes(ClearAndRestore, sizeof(ClearAndRestore) - 1);
    }
}

int CTextConsolePosix::GetWidth() {
    constexpr int DefaultConsoleWidth = 80;
    winsize size = {};
    if (m_Output >= 0 && ioctl(m_Output, TIOCGWINSZ, &size) == 0 && size.ws_col > 1) {
        return size.ws_col;
    }
    return DefaultConsoleWidth;
}

void CTextConsolePosix::SetVisible(bool visible) {
    m_ConsoleVisible = visible;
    if (visible) {
        RenderTitle();
        UpdateStatus();
    }
}

char* CTextConsolePosix::FinishInput(bool completeLine) {
    m_InputEnded = true;
    if (completeLine) {
        return m_nConsoleTextLen > 0 && ReceiveNewline() > 0 ? m_szConsoleText : nullptr;
    }
    m_InputFailed = true;
    const int error = errno;
    m_nConsoleTextLen = 0;
    m_nCursorPosition = 0;
    if (m_System != nullptr) {
        char format[] = "WARNING! Console input disabled: %s\n";
        m_System->Printf(format, std::strerror(error));
    }
    errno = error;
    return nullptr;
}

char* CTextConsolePosix::GetLine() {
    if (!m_Initialized || m_InputEnded || cse::dedicated::ShutdownRequested) {
        return nullptr;
    }
    // The console is the sole input consumer while active. poll/read never alter shared file status flags.
    constexpr int MaximumInputBytesPerFrame = 4096;
    constexpr unsigned char Escape = 27;
    constexpr unsigned char Delete = 127;
    constexpr unsigned char EndOfInput = 4;
    for (int processed = 0; processed < MaximumInputBytesPerFrame; ++processed) {
        pollfd input = {m_Input, POLLIN, 0};
        const int ready = poll(&input, 1, 0);
        if (ready == 0) {
            return nullptr;
        }
        if (ready < 0) {
            if (errno == EINTR) {
                if (cse::dedicated::ShutdownRequested) {
                    return nullptr;
                }
                continue;
            }
            return FinishInput(false);
        }
        if (input.revents & (POLLERR | POLLNVAL)) {
            errno = input.revents & POLLNVAL ? EBADF : EIO;
            return FinishInput(false);
        }
        if (!(input.revents & (POLLIN | POLLHUP))) {
            return nullptr;
        }
        unsigned char character = 0;
        const ssize_t count = read(m_Input, &character, sizeof(character));
        if (count == 0) {
            return FinishInput(true);
        }
        if (count < 0) {
            if (errno == EINTR) {
                if (cse::dedicated::ShutdownRequested) {
                    return nullptr;
                }
                continue;
            }
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                return nullptr;
            }
            return FinishInput(false);
        }
        if (m_SkipLineFeed) {
            m_SkipLineFeed = false;
            if (character == '\n') {
                continue;
            }
        }
        if (m_EscapeState == EscapeState::Escape) {
            m_EscapeState = EscapeState::None;
            if (character == '[' || character == 'O') {
                m_EscapeState = EscapeState::Sequence;
                m_SequenceParameters = false;
                continue;
            }
        } else if (m_EscapeState == EscapeState::Sequence) {
            constexpr unsigned char FirstFinalByte = 0x40;
            constexpr unsigned char LastFinalByte = 0x7e;
            if (character < FirstFinalByte || character > LastFinalByte) {
                m_SequenceParameters = true;
                continue;
            }
            m_EscapeState = EscapeState::None;
            if (!m_SequenceParameters) {
                switch (character) {
                case 'A':
                    ReceiveUpArrow();
                    break;
                case 'B':
                    ReceiveDownArrow();
                    break;
                case 'C':
                    ReceiveRightArrow();
                    break;
                case 'D':
                    ReceiveLeftArrow();
                    break;
                }
            }
            continue;
        }
        if (character == Escape) {
            m_EscapeState = EscapeState::Escape;
        } else if (character == EndOfInput) {
            return FinishInput(true);
        } else if (character == '\b' || character == Delete) {
            ReceiveBackspace();
        } else if (character == '\t') {
            ReceiveTab();
        } else if (character == '\r' || character == '\n') {
            m_SkipLineFeed = character == '\r';
            if (ReceiveNewline() > 0) {
                return m_szConsoleText;
            }
        } else if (character >= ' ' && character != Delete) {
            m_SkipLineFeed = false;
            ReceiveStandardChar(static_cast<char>(character));
        }
    }
    return nullptr;
}

#endif
