#pragma once

#include "browser_wire.h"

#include <SDL3/SDL.h>
#include <deque>
#include <string>
#include <vector>

namespace cse::browser {

enum class ChannelStatus { Idle, Progress, End, Failed };

// One owner pumps both nonblocking pipes. Shutdown drains and reaps the child within a bounded grace period.
class ProcessChannel {
public:
    // Allow normal CEF cookie flushing while bounding a stalled host during game quit.
    static constexpr Uint64 ShutdownTimeoutMilliseconds = 15000;

    explicit ProcessChannel(std::size_t payloadLimit) : decoder_(payloadLimit) {}
    ~ProcessChannel();
    ProcessChannel(const ProcessChannel&) = delete;
    ProcessChannel& operator=(const ProcessChannel&) = delete;

    bool Start(const std::vector<std::string>& arguments, const char* directory = nullptr);
    bool Queue(Header header, const std::vector<std::uint8_t>& payload);
    ChannelStatus Poll(std::vector<Message>& messages);
    bool CloseInput();
    bool Finish(int& exitCode, Uint64 gracefulTimeoutMilliseconds = ShutdownTimeoutMilliseconds);
    bool Started() const;
    const std::string& Error() const;

private:
    struct Packet {
        std::array<std::uint8_t, HeaderBytes> header;
        std::vector<std::uint8_t> payload;
        std::size_t offset = 0;
    };

    void Fail(const char* message);
    bool Read(std::vector<Message>& messages);
    bool Write();

    SDL_Process* process_ = nullptr;
    SDL_IOStream* input_ = nullptr;
    SDL_IOStream* output_ = nullptr;
    Decoder decoder_;
    std::deque<Packet> outgoing_;
    bool eof_ = false;
    bool failed_ = false;
    std::string error_;
};
}
