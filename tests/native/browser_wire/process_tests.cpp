#include "process_channel.h"

#include <array>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>
#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <fcntl.h>
#include <io.h>
#else
#include <unistd.h>
#endif

using namespace cse::browser;
constexpr std::size_t PayloadLimit = 8 * 1024 * 1024;
constexpr int TransferCount = 6;
constexpr std::size_t TransferBytes = 1024 * 1024 + 37;
constexpr Uint64 DeadlineMilliseconds = 10000;
const std::string Argument = "CSE \xd0\x96 \xc5\x81 \"quoted\" $() &| trailing\\";

static void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "browser_process_failure=%s SDL=%s\n", message, SDL_GetError());
        std::exit(1);
    }
}

static int ReadInput(void* data, unsigned int bytes) {
#if defined(_WIN32)
    return _read(_fileno(stdin), data, bytes);
#else
    return static_cast<int>(read(STDIN_FILENO, data, bytes));
#endif
}

static bool WriteOutput(const std::uint8_t* data, std::size_t bytes) {
    while (bytes != 0) {
        constexpr std::size_t ChunkBytes = 16 * 1024;
        const unsigned int amount = static_cast<unsigned int>(std::min(bytes, ChunkBytes));
#if defined(_WIN32)
        const int written = _write(_fileno(stdout), data, amount);
#else
        const int written = static_cast<int>(write(STDOUT_FILENO, data, amount));
#endif
        if (written <= 0) {
            return false;
        }
        data += written;
        bytes -= static_cast<std::size_t>(written);
    }
    return true;
}

static bool Reply(Header header, const std::vector<std::uint8_t>& payload) {
    header.payloadBytes = static_cast<std::uint32_t>(payload.size());
    const auto encoded = EncodeHeader(header);
    return WriteOutput(encoded.data(), encoded.size()) && WriteOutput(payload.data(), payload.size());
}

static int Child(const std::vector<std::string>& arguments) {
#if defined(_WIN32)
    _setmode(_fileno(stdin), _O_BINARY);
    _setmode(_fileno(stdout), _O_BINARY);
#endif
    if (arguments[1] == "--exit") {
        return 17;
    }
    if (arguments[1] == "--truncated" || arguments[1] == "--bad-version") {
        auto bytes = EncodeHeader({Kind::Event, 1, 0, 0, 128});
        if (arguments[1] == "--bad-version") {
            bytes[4] = 2;
        }
        Require(WriteOutput(bytes.data(), bytes.size()) && WriteOutput(bytes.data(), 3), "invalid child output");
        return 37;
    }
    const bool draining = arguments[1] == "--drain";
    int replied = 0;
    Writer ready;
    constexpr std::size_t EchoArgumentCount = 3;
    constexpr std::size_t DrainArgumentCount = 4;
    Require(arguments.size() == (draining ? DrainArgumentCount : EchoArgumentCount) && ready.String(arguments[2]), "child argument count");
    Require(Reply({Kind::Control, 1, 0, 0, 0}, ready.Data()), "child ready");
    Decoder decoder(PayloadLimit);
    constexpr unsigned int InputBytes = 16 * 1024;
    std::array<std::uint8_t, InputBytes> bytes;
    for (;;) {
        const int count = ReadInput(bytes.data(), InputBytes);
        if (count == 0) {
            return decoder.Finish() && (!draining || replied == TransferCount) ? 0 : 39;
        }
        if (count < 0) {
            return 40;
        }
        std::size_t offset = 0;
        while (offset < static_cast<std::size_t>(count)) {
            Message message;
            std::size_t consumed = 0;
            const auto status = decoder.Feed(bytes.data() + offset, static_cast<std::size_t>(count) - offset,
                consumed, message);
            if (status == DecodeStatus::Invalid || consumed == 0) {
                return 41;
            }
            offset += consumed;
            if (status == DecodeStatus::Complete) {
                message.header.kind = Kind::Event;
                Require(Reply(message.header, message.payload), "child echo");
                ++replied;
            }
        }
    }
}

static std::vector<std::uint8_t> Payload(int sequence) {
    std::vector<std::uint8_t> result(TransferBytes);
    for (std::size_t index = 0; index < result.size(); ++index) {
        result[index] = static_cast<std::uint8_t>((index * 13 + static_cast<unsigned int>(sequence)) & 0xff);
    }
    return result;
}

static void Echo(ProcessChannel& channel, const std::string& child) {
    static int invocation = 0;
    std::fprintf(stderr, "browser_echo_invocation=%d\n", ++invocation);
    Require(channel.Start({child, "--echo", Argument}), "start owned child");
    Require(!channel.Start({child, "--exit"}) && channel.Started(), "duplicate start preserves owner");
    for (int sequence = 0; sequence < TransferCount; ++sequence) {
        Require(channel.Queue({Kind::Command, 7, 9, static_cast<std::uint32_t>(sequence + 1), 0}, Payload(sequence)),
            "queue ordered bulk payload");
    }
    bool ready = false;
    int received = 0;
    const Uint64 deadline = SDL_GetTicks() + DeadlineMilliseconds;
    while (received < TransferCount && SDL_GetTicks() < deadline) {
        std::vector<Message> messages;
        const auto status = channel.Poll(messages);
        if (status == ChannelStatus::Failed || status == ChannelStatus::End) {
            std::fprintf(stderr, "browser_channel_status=%d received=%d error=%s\n",
                static_cast<int>(status), received, channel.Error().c_str());
        }
        Require(status != ChannelStatus::Failed && status != ChannelStatus::End, "live duplex channel");
        for (const auto& message : messages) {
            if (message.header.kind == Kind::Control) {
                Reader reader(message.payload);
                std::string argument;
                Require(!ready && reader.String(argument) && reader.Finish() && argument == Argument,
                    "Unicode, spaces, quotes and shell metacharacters stay one argument");
                ready = true;
            } else {
                Require(ready && message.header.kind == Kind::Event && message.header.operation == 7 &&
                    message.header.browser == 9 && message.header.request == static_cast<std::uint32_t>(received + 1) &&
                    message.payload == Payload(received), "duplex payload bytes and ordering");
                ++received;
            }
        }
        if (status == ChannelStatus::Idle) {
            SDL_Delay(1);
        }
    }
    Require(received == TransferCount, "bulk duplex deadline");
    Require(channel.CloseInput(), "close an empty outgoing stream");
    Require(!channel.Queue({Kind::Command, 0, 0, 0, 0}, {}), "commands rejected after input close");
    int exitCode = -1;
    Require(channel.Finish(exitCode) && exitCode == 0 && !channel.Started(), "cooperative EOF and process reap");
    Require(channel.Finish(exitCode), "repeat finish");
}

static int Run(const std::vector<std::string>& arguments) {
    const bool peer = arguments.size() == 3 && arguments[1] == "--peer";
    if (arguments.size() > 1 && !peer) {
        return Child(arguments);
    }
    const std::string child = peer ? arguments[2] : arguments[0];
    Require(SDL_Init(0), "SDL fixture initialization");
    {
        ProcessChannel channel(PayloadLimit);
        Require(!channel.Start({child + ".missing"}) && !channel.Started() && !channel.Error().empty(),
            "missing executable leaves no owned process");
        Echo(channel, child);
        Echo(channel, child);
    }
    {
        ProcessChannel channel(PayloadLimit);
        Require(channel.Start({child, "--drain", Argument, "6"}), "start draining child");
        for (int sequence = 0; sequence < TransferCount; ++sequence) {
            Require(channel.Queue({Kind::Command, 7, 9, static_cast<std::uint32_t>(sequence + 1), 0}, Payload(sequence)),
                "queue before immediate shutdown");
        }
        Require(!channel.CloseInput(), "input close cannot truncate accepted messages");
        int exitCode = -1;
        Require(channel.Finish(exitCode) && exitCode == 0, "shutdown flushes queued commands while draining replies");
    }
    for (const char* mode : {"--exit", "--truncated", "--bad-version"}) {
        ProcessChannel channel(PayloadLimit);
        Require(channel.Start({child, mode}), "start failure-case child");
        const Uint64 deadline = SDL_GetTicks() + DeadlineMilliseconds;
        ChannelStatus status = ChannelStatus::Idle;
        while (SDL_GetTicks() < deadline) {
            std::vector<Message> messages;
            status = channel.Poll(messages);
            if (status == ChannelStatus::End || status == ChannelStatus::Failed) {
                break;
            }
            SDL_Delay(1);
        }
        int exitCode = 0;
        const bool finished = channel.Finish(exitCode);
        if (std::string(mode) == "--exit") {
            Require(status == ChannelStatus::End && finished && exitCode == 17, "nonzero child exit retained");
        } else {
            Require(status == ChannelStatus::Failed && !finished && exitCode == 37 && !channel.Error().empty(),
                "truncated child output is a failed channel");
            Echo(channel, child);
        }
    }
    SDL_Quit();
    std::puts("native_browser_process_passed=1");
    return 0;
}

#if defined(_WIN32)
int wmain(int count, wchar_t* values[]) {
    std::vector<std::string> arguments;
    for (int index = 0; index < count; ++index) {
        const int required = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, values[index], -1,
            nullptr, 0, nullptr, nullptr);
        Require(required > 0, "native Windows UTF-16 arguments");
        std::string value(static_cast<std::size_t>(required), '\0');
        Require(WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, values[index], -1,
            value.data(), required, nullptr, nullptr) == required, "Windows argument conversion");
        value.pop_back();
        arguments.push_back(std::move(value));
    }
    return Run(arguments);
}
#else
int main(int count, char* values[]) {
    return Run(std::vector<std::string>(values, values + count));
}
#endif
