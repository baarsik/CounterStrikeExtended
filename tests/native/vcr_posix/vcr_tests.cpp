#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <cerrno>
#include <climits>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <limits>
#include <string>
#include <vector>

namespace {

struct SocketReply {
    ssize_t result = 0;
    int error = 0;
    const char* data = "abc";
};

SocketReply reply;
int socketCalls = 0;
int helperCalls = 0;
int fatalCalls = 0;
std::string helperMessage;
bool poisonWriteErrno = false;
bool failSeek = false;
bool failTell = false;
bool failWrite = false;
bool shortRead = false;
int writeCalls = 0;
int failWriteOnCall = 0;
bool failFlush = false;
bool failRead = false;

void Require(bool condition, const char* description) {
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s (errno=%d, helpers=%d, message=%s)\n",
            description, errno, helperCalls, helperMessage.c_str());
        std::exit(EXIT_FAILURE);
    }
}

}

extern "C" ssize_t CseSend(int, const void*, std::size_t, int) {
    ++socketCalls;
    errno = reply.error;
    return reply.result;
}

extern "C" ssize_t CseRecv(int, void* output, std::size_t capacity, int) {
    ++socketCalls;
    if (reply.result > 0 && static_cast<std::uint64_t>(reply.result) <= capacity) {
        std::memcpy(output, reply.data, static_cast<std::size_t>(reply.result));
    }
    errno = reply.error;
    return reply.result;
}

extern "C" ssize_t CseRecvFrom(int, void* output, std::size_t capacity, int, sockaddr* from, socklen_t* length) {
    const ssize_t result = CseRecv(0, output, capacity, 0);
    if (result >= 0 && from != nullptr && length != nullptr) {
        constexpr unsigned char Address[] = {1, 2, 3, 4};
        Require(*length >= sizeof(Address), "controlled address destination fits");
        std::memcpy(from, Address, sizeof(Address));
        *length = sizeof(Address);
    }
    return result;
}

extern "C" std::size_t CseWrite(const void* data, std::size_t size, std::size_t count, FILE* file) {
    ++writeCalls;
    if (failWrite || writeCalls == failWriteOnCall) {
        errno = ENOSPC;
        return 0;
    }
    const std::size_t written = std::fwrite(data, size, count, file);
    if (poisonWriteErrno) {
        errno = ENOMEM;
    }
    return written;
}

extern "C" int CseFlush(FILE* file) {
    if (failFlush) {
        errno = ENOSPC;
        return EOF;
    }
    return std::fflush(file);
}

extern "C" std::size_t CseRead(void* data, std::size_t size, std::size_t count, FILE* file) {
    if (failRead) {
        errno = EACCES;
        return 0;
    }
    if (shortRead && count > 0) {
        return std::fread(data, size, count - 1, file);
    }
    return std::fread(data, size, count, file);
}

extern "C" int CseFileError(FILE* file) {
    return failRead ? 1 : std::ferror(file);
}

extern "C" int CseSeek(FILE* file, off_t offset, int origin) {
    if (failSeek) {
        errno = ESPIPE;
        return -1;
    }
    return fseeko(file, offset, origin);
}

extern "C" off_t CseTell(FILE* file) {
    if (failTell) {
        errno = EOVERFLOW;
        return -1;
    }
    return ftello(file);
}

extern "C" int CseLegacySeek(FILE* file, long offset, int origin) {
    return CseSeek(file, offset, origin);
}

extern "C" long CseLegacyTell(FILE* file) {
    return static_cast<long>(CseTell(file));
}

// Include the complete actual production TU. Only synchronous OS/stdio bindings are controlled.
#define send CseSend
#define recv CseRecv
#define recvfrom CseRecvFrom
#define fwrite CseWrite
#define fread CseRead
#define fflush CseFlush
#define ferror CseFileError
#define fseeko CseSeek
#define ftello CseTell
#define fseek CseLegacySeek
#define ftell CseLegacyTell
#include CSE_VCR_SOURCE
#undef send
#undef recv
#undef recvfrom
#undef fwrite
#undef fread
#undef fflush
#undef ferror
#undef fseeko
#undef ftello
#undef fseek
#undef ftell

extern "C" void Error(const char* format, ...) {
    ++fatalCalls;
    char message[512] = {};
    va_list arguments;
    va_start(arguments, format);
    std::vsnprintf(message, sizeof(message), format, arguments);
    va_end(arguments);
    helperMessage = message;
}

namespace {

class ReturningHelpers : public IVCRHelpers {
public:
    void ErrorMessage(char* message) override {
        ++helperCalls;
        helperMessage = message;
        errno = ERANGE;
    }
};

ReturningHelpers helpers;

void WriteBytes(const char* path, const std::vector<unsigned char>& bytes) {
    std::ofstream output(path, std::ios::binary);
    output.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    Require(output.good(), "write independent malformed input");
}

void ExpectFailure(const char* description) {
    Require(helperCalls == 1, description);
    Require(g_pVCR->GetMode() == VCR_Disabled && g_pVCRFile == nullptr, "failure closes and disables recorder");
    Require(socketCalls == 0, "malformed replay cannot fall through to a live socket binding");
    Require(errno == EIO, "malformed replay reports EIO after a returning helper");
    Require(helperMessage.compare(0, 4, "VCR ") == 0, "va_list error formatting preserves the actual reason");
}

void SetReply(ssize_t result, int error, const char* data = "abc") {
    reply.result = result;
    reply.error = error;
    reply.data = data;
}

void PacketCase(const char* path, bool playback) {
    Require(g_pVCR->Start(path, !playback, &helpers) == 1, "start packet recording/replay");
    Require(g_pVCR->Hook_Sys_FloatTime(1.25) == 1.25, "binary64 time event");
    constexpr unsigned char Data[] = {0, 255, 126};
    unsigned char data[sizeof(Data)] = {};
    if (playback) {
        Require(g_pVCR->GenericPlayback("event", data, sizeof(data), true) == sizeof(data), "generic replay size");
        Require(std::memcmp(data, Data, sizeof(data)) == 0, "generic replay payload");
    } else {
        g_pVCR->GenericRecord("event", Data, sizeof(Data));
    }
    SetReply(3, EINTR);
    Require(g_pVCR->Hook_send(17, "abc", 3, 2) == 3, "send result");
    SetReply(-1, EAGAIN);
    Require(g_pVCR->Hook_send(17, "abc", 3, 2) == -1 && errno == EAGAIN, "send error result and errno");
    char received[8] = {};
    SetReply(3, EINTR, "r\0x");
    Require(g_pVCR->Hook_recv(17, received, sizeof(received), 2) == 3, "recv result");
    Require(std::memcmp(received, "r\0x", 3) == 0, "recv bytes including zero");
    SetReply(-1, ECONNRESET);
    Require(g_pVCR->Hook_recv(17, received, sizeof(received), 2) == -1 && errno == ECONNRESET, "recv error and errno");
    sockaddr address = {};
    int addressLength = 4;
    SetReply(2, EINTR, "rx");
    Require(g_pVCR->Hook_recvfrom(17, received, sizeof(received), 2, &address, &addressLength) == 2, "recvfrom result");
    constexpr unsigned char Address[] = {1, 2, 3, 4};
    Require(std::memcmp(&address, Address, sizeof(Address)) == 0, "recvfrom opaque address bytes");
    SetReply(INT_MAX, EINTR);
    Require(g_pVCR->Hook_send(17, "x", INT_MAX, 0) == INT_MAX, "maximum serialized socket result");
    if (playback) {
        Require(g_pVCR->GenericPlayback("end", nullptr, 0, true) == 0, "zero-byte final generic event");
        Require(socketCalls == 0, "playback never calls socket bindings");
        Require(g_pVCR->GetMode() == VCR_Disabled, "complete final payload ends playback");
    } else {
        g_pVCR->GenericRecord("end", nullptr, 0);
    }
    Require(helperCalls == 0 && fatalCalls == 0, "valid packet does not report failure");
    g_pVCR->End();
}

void ControlPacketCase(const char* path, bool playback) {
    Require(g_pVCR->Start(path, !playback, &helpers) == 1, "start control event packet");
    const VCRMode activeMode = g_pVCR->GetMode();
    g_pVCR->SetEnabled(0);
    int untouched = 17;
    g_pVCR->GenericValue("disabled", &untouched, sizeof(untouched));
    Require(untouched == 17 && g_pVCR->Hook_Sys_FloatTime(2.5) == 2.5, "disabled hooks do not consume events");
    g_pVCR->SetEnabled(1);
    Require(g_pVCR->GetMode() == activeMode, "resume actual prior mode");
    g_pVCR->SyncToken("sync");
    OutputDebugStringFormat("%s", "dbg");
    BuildCmdLine(0, nullptr);
    char* commandLine = g_pVCR->Hook_GetCommandLine();
    Require(commandLine != nullptr && std::strcmp(commandLine, " -steam") == 0, "command line payload");
    if (playback) {
        delete[] commandLine;
    }
    char command[] = "exec";
    char* text = command;
    g_pVCR->Hook_Cmd_Exec(&text);
    Require(text != nullptr && std::strcmp(text, "exec") == 0, "command text payload");
    if (playback) {
        std::free(text);
    }
    text = nullptr;
    g_pVCR->Hook_Cmd_Exec(&text);
    Require(text == nullptr, "null command sentinel");
    constexpr unsigned int EventCount = 0xdeadbeef;
    unsigned int eventCount = playback ? 0 : EventCount;
    Require(g_pVCR->Hook_GetNumberOfConsoleInputEvents(nullptr, &eventCount) == 1
        && eventCount == EventCount, "retained console-event stub's recorded fields");
    constexpr unsigned int Message = 0xfedcba98;
    constexpr unsigned int WParam = 0x80000000;
    constexpr int LParam = INT_MIN;
    if (playback) {
        unsigned int message = 0;
        unsigned int wParam = 0;
        int lParam = 0;
        Require(g_pVCR->Hook_PlaybackGameMsg(&message, &wParam, &lParam), "game message payload");
        Require(message == Message && wParam == WParam && lParam == LParam, "fixed-width game message fields");
        Require(!g_pVCR->Hook_PlaybackGameMsg(&message, &wParam, &lParam), "end-game-message marker");
        Require(g_pVCR->GetMode() == VCR_Disabled, "control packet ends exactly at final marker");
    } else {
        g_pVCR->Hook_RecordGameMsg(Message, WParam, LParam);
        g_pVCR->Hook_RecordEndGameMsg();
    }
    Require(helperCalls == 0 && fatalCalls == 0 && socketCalls == 0, "valid control packet uses no live bindings");
    g_pVCR->End();
}

void MalformedCase(const char* name, const char* path) {
    std::vector<unsigned char> bytes = {2, 0, 0, 0};
    if (std::strcmp(name, "truncated-header") == 0) {
        bytes.resize(3);
    } else if (std::strcmp(name, "truncated-event") == 0) {
        bytes.push_back(19);
    } else if (std::strcmp(name, "truncated-send-error") == 0) {
        bytes.insert(bytes.end(), {19, 255, 255, 255, 255});
    } else if (std::strcmp(name, "truncated-recv") == 0) {
        bytes.insert(bytes.end(), {18, 3, 0, 0, 0, 'a', 'b'});
    } else if (std::strcmp(name, "recv-over-capacity") == 0) {
        bytes.insert(bytes.end(), {18, 9, 0, 0, 0, 'a'});
    } else if (std::strcmp(name, "negative-generic") == 0) {
        bytes.insert(bytes.end(), {20, 255, 255, 255, 255, 255});
    } else if (std::strcmp(name, "unterminated-name") == 0) {
        bytes.insert(bytes.end(), {20, 2, 'n', 'x', 0, 0, 0, 0});
    } else if (std::strcmp(name, "negative-command") == 0) {
        bytes.insert(bytes.end(), {7, 255, 255, 255, 255});
    } else if (std::strcmp(name, "truncated-command") == 0) {
        bytes.insert(bytes.end(), {7, 255, 255, 255, 127, 0});
    } else if (std::strcmp(name, "wrong-event") == 0) {
        bytes.insert(bytes.end(), {0, 0, 0, 0, 0, 0, 0, 0, 0});
    } else if (std::strcmp(name, "invalid-event") == 0) {
        bytes.insert(bytes.end(), {255, 0});
    } else if (std::strcmp(name, "short-read") == 0) {
        bytes.insert(bytes.end(), {19, 3, 0, 0, 0});
    } else if (std::strcmp(name, "truncated-game") == 0) {
        bytes.insert(bytes.end(), {14, 1, 1, 0, 0, 0});
    } else if (std::strcmp(name, "game-marker") == 0) {
        bytes.insert(bytes.end(), {14, 2});
    } else if (std::strcmp(name, "debug-length") == 0) {
        bytes.insert(bytes.end(), {0, 16, 0, 0, 'x'});
    } else if (std::strcmp(name, "negative-cmd-exec") == 0) {
        bytes.insert(bytes.end(), {6, 254, 255, 255, 255});
    } else if (std::strcmp(name, "unterminated-command") == 0) {
        bytes.insert(bytes.end(), {7, 2, 0, 0, 0, 'x', 'y'});
    } else if (std::strcmp(name, "truncated-address") == 0) {
        bytes.insert(bytes.end(), {1, 0, 0, 0, 0, 1, 'x', 'y', 'z'});
    } else if (std::strcmp(name, "address-marker") == 0) {
        bytes.insert(bytes.end(), {1, 0, 0, 0, 0, 2});
    } else if (std::strcmp(name, "unknown-version") == 0) {
        bytes = {3, 0, 0, 0, 0};
    } else {
        Require(false, "known malformed case");
    }
    WriteBytes(path, bytes);
    const int started = g_pVCR->Start(path, false, &helpers);
    if (std::strcmp(name, "truncated-header") == 0 || std::strcmp(name, "unknown-version") == 0) {
        Require(started == 0, "invalid recording header is rejected");
    } else {
        Require(started == 1, "valid envelope starts before malformed event");
        if (std::strstr(name, "generic") || std::strcmp(name, "unterminated-name") == 0) {
            unsigned char output[8];
            std::memset(output, 0x5a, sizeof(output));
            Require(g_pVCR->GenericPlayback("n", output, sizeof(output), false) == 0, "malformed generic fails");
            Require(output[0] == 0x5a, "invalid generic metadata does not touch destination");
        } else if (std::strstr(name, "command")) {
            Require(g_pVCR->Hook_GetCommandLine() == nullptr, "malformed command rejects before allocation");
        } else if (std::strcmp(name, "negative-cmd-exec") == 0) {
            char value[] = "original";
            char* command = value;
            g_pVCR->Hook_Cmd_Exec(&command);
            Require(command == nullptr, "invalid command text length returns null");
        } else if (std::strstr(name, "game")) {
            unsigned int message = 17;
            unsigned int wParam = 23;
            int lParam = 29;
            Require(!g_pVCR->Hook_PlaybackGameMsg(&message, &wParam, &lParam), "malformed game message fails");
            Require(message == 17 && wParam == 23 && lParam == 29, "game-message outputs remain atomic on failure");
        } else if (std::strcmp(name, "debug-length") == 0) {
            g_bTraceRead = true;
            OutputDebugStringFormat("%s", "x");
        } else if (std::strstr(name, "address")) {
            char output[8] = {};
            sockaddr address;
            std::memset(&address, 0x5a, sizeof(address));
            int addressLength = 4;
            Require(g_pVCR->Hook_recvfrom(17, output, sizeof(output), 0, &address, &addressLength) == -1,
                "malformed address fails");
            const unsigned char* data = reinterpret_cast<const unsigned char*>(&address);
            if (std::strcmp(name, "truncated-address") == 0) {
                Require(data[0] == 0 && data[3] == 0 && data[4] == 0x5a, "truncated address region zeroed");
            }
        } else if (std::strstr(name, "recv")) {
            char output[8];
            std::memset(output, 0x5a, sizeof(output));
            Require(g_pVCR->Hook_recv(17, output, sizeof(output), 0) == -1, "malformed receive fails");
            if (std::strcmp(name, "truncated-recv") == 0) {
                Require(output[0] == 0 && output[1] == 0 && output[2] == 0 && output[3] == 0x5a,
                    "truncated requested region is zeroed; remaining destination stays unchanged");
            } else {
                Require(output[0] == 0x5a, "over-capacity metadata leaves destination unchanged");
            }
        } else {
            shortRead = std::strcmp(name, "short-read") == 0;
            Require(g_pVCR->Hook_send(17, "abc", 3, 0) == -1, "malformed send fails");
        }
    }
    ExpectFailure(name);
}

void BoundaryCase(const char* name, const char* path) {
    if (std::strcmp(name, "empty") == 0) {
        WriteBytes(path, {2, 0, 0, 0});
        Require(g_pVCR->Start(path, false, &helpers) == 1, "header-only v2 is a valid empty recording");
        Require(g_pVCR->GetMode() == VCR_Disabled && g_pVCRFile == nullptr, "empty replay is already ended");
    } else if (std::strcmp(name, "high-offset") == 0) {
        constexpr off_t HighOffset = (off_t{1} << 33) + 17;
        volatile off_t highPosition = HighOffset;
        FILE* file = std::fopen(path, "wb");
        const unsigned char header[] = {2, 0, 0, 0};
        Require(file != nullptr && std::fwrite(header, 1, sizeof(header), file) == sizeof(header), "sparse header");
        Require(fseeko(file, HighOffset, SEEK_SET) == 0 && std::fputc(0x5a, file) != EOF, "sparse tail above 4 GiB");
        std::fclose(file);
        Require(g_pVCR->Start(path, false, &helpers) == 1, "large recording starts without length truncation");
        Require(static_cast<off_t>(g_FileLen) == highPosition + 1, "native file extent retains high bits");
        // Controlled positioning isolates the actual cursor/read arithmetic without reading an 8-GiB hole.
        Require(fseeko(g_pVCRFile, HighOffset, SEEK_SET) == 0, "position controlled high-offset trace read");
        g_CurFilePos = static_cast<decltype(g_CurFilePos)>(highPosition);
        unsigned char tail = 0;
        g_pVCR->GetVCRTraceInterface()->Read(&tail, 1);
        Require(tail == 0x5a && static_cast<off_t>(g_CurFilePos) == highPosition + 1,
            "high cursor advances without wrapping");
        Require(g_pVCR->GetMode() == VCR_Disabled, "high-offset final read ends");
    } else if (std::strcmp(name, "seek-error") == 0 || std::strcmp(name, "tell-error") == 0) {
        WriteBytes(path, {2, 0, 0, 0, 0});
        failSeek = std::strcmp(name, "seek-error") == 0;
        failTell = !failSeek;
        Require(g_pVCR->Start(path, false, &helpers) == 0, "native position error rejects playback");
        Require(helperCalls == 1 && g_pVCRFile == nullptr, "native position failure reports and closes");
        Require(errno == (failSeek ? ESPIPE : EOVERFLOW), "native position errno survives helper and close");
    } else if (std::strcmp(name, "write-error") == 0) {
        failWrite = true;
        Require(g_pVCR->Start(path, true, &helpers) == 0, "failed header write rejects recording");
        Require(helperCalls == 1 && errno == ENOSPC && g_pVCRFile == nullptr, "write failure reports actual errno");
    } else if (std::strcmp(name, "read-error") == 0) {
        WriteBytes(path, {2, 0, 0, 0, 19, 1, 0, 0, 0});
        Require(g_pVCR->Start(path, false, &helpers) == 1, "start before native read error");
        failRead = true;
        Require(g_pVCR->Hook_send(17, "x", 1, 0) == -1, "native read failure is explicit");
        Require(helperCalls == 1 && errno == EACCES && g_pVCRFile == nullptr && socketCalls == 0,
            "native read errno survives and no socket binding is called");
    } else if (std::strcmp(name, "open-error") == 0) {
        std::remove(path);
        Require(g_pVCR->Start(path, false, &helpers) == 0 && errno == ENOENT, "native open error is unchanged");
        Require(helperCalls == 0 && g_pVCRFile == nullptr, "open failure does not fabricate a format error");
    } else if (std::strcmp(name, "flush-error") == 0) {
        failFlush = true;
        Require(g_pVCR->Start(path, true, &helpers) == 0, "failed header flush rejects recording");
        Require(helperCalls == 1 && errno == ENOSPC && g_pVCRFile == nullptr, "flush failure preserves errno");
    } else if (std::strcmp(name, "event-write-error") == 0 || std::strcmp(name, "result-write-error") == 0) {
        Require(g_pVCR->Start(path, true, &helpers) == 1, "start failure-boundary recording");
        const bool failBeforeSocket = std::strcmp(name, "event-write-error") == 0;
        failWriteOnCall = failBeforeSocket ? 2 : 3;
        SetReply(1, EINTR);
        Require(g_pVCR->Hook_send(17, "x", 1, 0) == -1, "event/result recording error is explicit");
        Require(helperCalls == 1 && errno == ENOSPC && g_pVCRFile == nullptr, "recording failure reports and ends");
        Require(socketCalls == (failBeforeSocket ? 0 : 1), "failure preserves event-before-socket call order");
    } else if (std::strcmp(name, "errno") == 0) {
        Require(g_pVCR->Start(path, true, &helpers) == 1, "start errno recording");
        poisonWriteErrno = true;
        SetReply(-1, EAGAIN);
        Require(g_pVCR->Hook_send(17, "x", 1, 0) == -1 && errno == EAGAIN, "socket errno survives record writes");
        poisonWriteErrno = false;
        g_pVCR->End();
        Require(g_pVCR->Start(path, false, &helpers) == 1, "replay captured socket errno");
        Require(g_pVCR->Hook_send(17, "x", 1, 0) == -1 && errno == EAGAIN, "immediate native errno was serialized");
    } else if (std::strcmp(name, "native-result") == 0) {
        struct NativeCase {
            ssize_t native;
            int expected;
            int expectedError;
        };
        const NativeCase cases[] = {
            {-2, -1, EOVERFLOW}, {-1, -1, EAGAIN}, {0, 0, EINTR}, {1, 1, EINTR}, {INT_MAX, INT_MAX, EINTR},
            {static_cast<ssize_t>(INT_MAX) + 1, -1, EOVERFLOW},
            {std::numeric_limits<ssize_t>::max(), -1, EOVERFLOW}
        };
        for (const NativeCase& test : cases) {
            SetReply(test.native, test.native == -1 ? EAGAIN : EINTR);
            Require(g_pVCR->Hook_send(17, "x", INT_MAX, 0) == test.expected && errno == test.expectedError,
                "checked native send result");
            char data = 0;
            Require(g_pVCR->Hook_recv(17, &data, 0, 0) == test.expected && errno == test.expectedError,
                "checked native recv result without copying controlled payload");
            Require(g_pVCR->Hook_recvfrom(17, &data, 0, 0, nullptr, nullptr) == test.expected
                && errno == test.expectedError, "checked native recvfrom result");
        }
    } else if (std::strcmp(name, "null-generic") == 0) {
        Require(g_pVCR->Start(path, true, &helpers) == 1, "record unnamed generic event");
        const unsigned char value = 0x5a;
        g_pVCR->GenericRecord(nullptr, &value, 1);
        g_pVCR->End();
        Require(g_pVCR->Start(path, false, &helpers) == 1, "replay unnamed generic event");
        unsigned char output = 0;
        Require(g_pVCR->GenericPlayback(nullptr, &output, 1, true) == 1 && output == value, "unnamed event payload");
    } else if (std::strcmp(name, "disabled") == 0) {
        SetReply(2, EINTR, "rx");
        char output[8] = {};
        Require(g_pVCR->Hook_recv(17, output, sizeof(output), 0) == 2 && errno == EINTR, "disabled live recv binding");
        Require(std::memcmp(output, "rx", 2) == 0 && socketCalls == 1, "disabled recv bytes and exactly one call");
        g_pVCR->GetVCRTraceInterface()->Read(output, sizeof(output));
        Require(output[0] == 0 && output[7] == 0 && errno == EINTR, "disabled trace retains zero-fill behavior");
    } else {
        Require(false, "known boundary case");
    }
    g_pVCR->End();
}

}

int main(int argc, char** argv) {
    Require(argc == 3, "case and temporary file arguments");
    if (std::strcmp(argv[1], "record") == 0 || std::strcmp(argv[1], "replay") == 0) {
        PacketCase(argv[2], std::strcmp(argv[1], "replay") == 0);
    } else if (std::strcmp(argv[1], "record-controls") == 0 || std::strcmp(argv[1], "replay-controls") == 0) {
        ControlPacketCase(argv[2], std::strcmp(argv[1], "replay-controls") == 0);
    } else if (std::strncmp(argv[1], "bad-", 4) == 0) {
        MalformedCase(argv[1] + 4, argv[2]);
    } else {
        BoundaryCase(argv[1], argv[2]);
    }
    std::printf("PASS %s\n", argv[1]);
    return EXIT_SUCCESS;
}
