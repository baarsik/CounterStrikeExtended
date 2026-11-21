#include "process_channel.h"

namespace cse::browser {

ProcessChannel::~ProcessChannel() {
    int exitCode = 0;
    Finish(exitCode);
}

bool ProcessChannel::Start(const std::vector<std::string>& arguments, const char* directory) {
    if (process_ || arguments.empty() || arguments.front().empty()) {
        return false;
    }
    decoder_.Reset();
    eof_ = false;
    failed_ = false;
    error_.clear();
    if (SDL_GetVersion() != SDL_VERSION) {
        error_ = "Browser process transport requires the pinned SDL3 provider";
        return false;
    }
    std::vector<const char*> argv;
    for (const auto& argument : arguments) {
        if (argument.find('\0') != std::string::npos) {
            error_ = "Browser process arguments contain an embedded terminator";
            return false;
        }
        argv.push_back(argument.c_str());
    }
    argv.push_back(nullptr);
    const SDL_PropertiesID properties = SDL_CreateProperties();
    const bool configured = properties &&
        SDL_SetPointerProperty(properties, SDL_PROP_PROCESS_CREATE_ARGS_POINTER, argv.data()) &&
        SDL_SetNumberProperty(properties, SDL_PROP_PROCESS_CREATE_STDIN_NUMBER, SDL_PROCESS_STDIO_APP) &&
        SDL_SetNumberProperty(properties, SDL_PROP_PROCESS_CREATE_STDOUT_NUMBER, SDL_PROCESS_STDIO_APP) &&
        SDL_SetNumberProperty(properties, SDL_PROP_PROCESS_CREATE_STDERR_NUMBER, SDL_PROCESS_STDIO_INHERITED) &&
        (!directory || SDL_SetStringProperty(properties, SDL_PROP_PROCESS_CREATE_WORKING_DIRECTORY_STRING, directory));
    if (configured) {
        process_ = SDL_CreateProcessWithProperties(properties);
    }
    SDL_DestroyProperties(properties);
    if (!process_) {
        error_ = SDL_GetError();
        return false;
    }
    input_ = SDL_GetProcessInput(process_);
    output_ = SDL_GetProcessOutput(process_);
    if (!input_ || !output_) {
        Fail("Browser process did not provide both pipes");
        int exitCode = 0;
        Finish(exitCode);
        return false;
    }
    error_.clear();
    return true;
}

bool ProcessChannel::Queue(Header header, const std::vector<std::uint8_t>& payload) {
    if (!process_ || !input_ || failed_ || payload.size() > std::numeric_limits<std::uint32_t>::max() ||
        payload.size() > std::numeric_limits<std::size_t>::max() - HeaderBytes) {
        return false;
    }
    header.payloadBytes = static_cast<std::uint32_t>(payload.size());
    outgoing_.push_back({EncodeHeader(header), payload, 0});
    return true;
}

ChannelStatus ProcessChannel::Poll(std::vector<Message>& messages) {
    if (!process_) {
        return ChannelStatus::End;
    }
    const bool readProgress = Read(messages);
    const bool writeProgress = Write();
    if (failed_) {
        return ChannelStatus::Failed;
    }
    if (eof_) {
        return ChannelStatus::End;
    }
    return readProgress || writeProgress ? ChannelStatus::Progress : ChannelStatus::Idle;
}

bool ProcessChannel::FlushOutgoing() {
    // A paint acknowledgement takes two writes. Bound longer command queues to preserve the game frame budget.
    constexpr std::size_t MaximumWritesPerFrame = 32;
    for (std::size_t count = 0; count < MaximumWritesPerFrame && !outgoing_.empty() && !failed_; ++count) {
        if (!Write()) {
            break;
        }
    }
    return !failed_;
}

bool ProcessChannel::CloseInput() {
    if (!outgoing_.empty() && !failed_ && !eof_) {
        return false;
    }
    outgoing_.clear();
    if (input_) {
        SDL_CloseIO(input_);
        input_ = nullptr;
    }
    return true;
}

bool ProcessChannel::Finish(int& exitCode, Uint64 gracefulTimeoutMilliseconds) {
    if (!process_) {
        return !failed_;
    }
    const Uint64 start = SDL_GetTicks();
    const auto expired = [start, gracefulTimeoutMilliseconds]() {
        return SDL_GetTicks() - start >= gracefulTimeoutMilliseconds;
    };
    bool timedOut = false;
    std::vector<Message> discarded;
    while (!outgoing_.empty() && input_ && !failed_ && !eof_) {
        if (expired()) {
            timedOut = true;
            break;
        }
        const bool readProgress = Read(discarded);
        const bool writeProgress = Write();
        discarded.clear();
        if (!readProgress && !writeProgress) {
            SDL_Delay(1);
        }
    }
    if (!timedOut && !outgoing_.empty() && eof_ && !failed_) {
        Fail("Browser process ended before queued commands were sent");
    }
    if (timedOut) {
        outgoing_.clear();
    }
    CloseInput();
    while (!timedOut && !eof_ && output_) {
        if (expired()) {
            timedOut = true;
            break;
        }
        if (!Read(discarded)) {
            SDL_Delay(1);
        }
        discarded.clear();
    }
    while (!timedOut && !SDL_WaitProcess(process_, false, &exitCode)) {
        if (expired()) {
            timedOut = true;
            break;
        }
        SDL_Delay(1);
    }
    if (timedOut) {
        Fail("Browser process shutdown timed out");
        if (!SDL_WaitProcess(process_, false, &exitCode)) {
            if (!SDL_KillProcess(process_, true)) {
                Fail("Browser process could not be stopped");
            } else {
                constexpr Uint64 ForcedReapTimeoutMilliseconds = 2000;
                const Uint64 killStart = SDL_GetTicks();
                while (!SDL_WaitProcess(process_, false, &exitCode) &&
                    SDL_GetTicks() - killStart < ForcedReapTimeoutMilliseconds) {
                    SDL_Delay(1);
                }
            }
        }
    }
    if (!SDL_WaitProcess(process_, false, &exitCode)) {
        Fail("Browser process could not be reaped");
    }
    SDL_DestroyProcess(process_);
    process_ = nullptr;
    output_ = nullptr;
    return !failed_;
}

bool ProcessChannel::Started() const {
    return process_ != nullptr;
}

bool ProcessChannel::HasPartialMessage() const {
    return !decoder_.Finish();
}

const std::string& ProcessChannel::Error() const {
    return error_;
}

void ProcessChannel::Fail(const char* message) {
    if (!failed_) {
        error_ = message;
    }
    failed_ = true;
}

bool ProcessChannel::Read(std::vector<Message>& messages) {
    if (!output_ || eof_) {
        return false;
    }
    constexpr std::size_t ChunkBytes = 64 * 1024;
    std::array<std::uint8_t, ChunkBytes> bytes;
    const std::size_t count = SDL_ReadIO(output_, bytes.data(), bytes.size());
    if (count != 0) {
        std::size_t offset = 0;
        while (!failed_ && offset < count) {
            std::size_t consumed = 0;
            Message message;
            const DecodeStatus status = decoder_.Feed(bytes.data() + offset, count - offset, consumed, message);
            offset += consumed;
            if (status == DecodeStatus::Invalid || consumed == 0) {
                Fail("Malformed browser process message");
            } else if (status == DecodeStatus::Complete) {
                messages.push_back(std::move(message));
            }
        }
        return true;
    }
    const SDL_IOStatus status = SDL_GetIOStatus(output_);
    if (status == SDL_IO_STATUS_EOF || status == SDL_IO_STATUS_ERROR) {
        eof_ = true;
        if (status == SDL_IO_STATUS_ERROR) {
            Fail("Browser process output pipe failed");
        } else if (!decoder_.Finish()) {
            Fail("Browser process ended inside a message");
        }
    }
    return false;
}

bool ProcessChannel::Write() {
    if (!input_ || outgoing_.empty() || failed_) {
        return false;
    }
    Packet& packet = outgoing_.front();
    const bool inHeader = packet.offset < HeaderBytes;
    const std::uint8_t* source = inHeader ? packet.header.data() + packet.offset :
        packet.payload.data() + (packet.offset - HeaderBytes);
    const std::size_t remaining = inHeader ? HeaderBytes - packet.offset :
        packet.payload.size() - (packet.offset - HeaderBytes);
    const std::size_t count = SDL_WriteIO(input_, source, remaining);
    packet.offset += count;
    if (packet.offset >= HeaderBytes && packet.offset - HeaderBytes == packet.payload.size()) {
        outgoing_.pop_front();
    }
    if (count == 0 && SDL_GetIOStatus(input_) == SDL_IO_STATUS_ERROR) {
        Fail("Browser process input pipe failed");
    }
    return count != 0;
}
}
