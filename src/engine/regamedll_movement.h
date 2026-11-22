#pragma once

#include "../cstrike/pm_shared/movement_timing.h"

#include <charconv>
#include <string_view>

namespace cse {

class ReGameDLLMovement {
public:
    template <typename SendCommand>
    void Request(std::string_view advertisedVersion, SendCommand sendCommand) {
        unsigned int major = 0;
        if (!ReadVersion(advertisedVersion, major) || major < SupportedMajorVersion) {
            return;
        }

        state_ = State::AwaitingReply;

        // ReGameDLL replies only when it clamps a requested version. Probe with the highest wire
        // version, then pin the version we predict in the same reliable message.
        sendCommand("cl_pmove_version 255.255.255");
        sendCommand("cl_pmove_version " CSE_REGAMEDLL_MOVEMENT_VERSION);
    }

    bool ConsumeReply(std::string_view text) {
        constexpr std::string_view prefix = "cl_pmove_version ";
        if (text.size() <= prefix.size() || text.substr(0, prefix.size()) != prefix || text.back() != '\n') {
            return false;
        }

        text.remove_prefix(prefix.size());
        text.remove_suffix(1);
        unsigned int major = 0;
        if (!ReadVersion(text, major)) {
            return false;
        }

        if (state_ == State::AwaitingReply) {
            state_ = major >= SupportedMajorVersion ? State::Enabled : State::Idle;
        }
        return true;
    }

    void Reset() {
        state_ = State::Idle;
    }

    bool PredictionEnabled() const {
        return state_ == State::Enabled;
    }

private:
    static constexpr unsigned int SupportedMajorVersion = 1;

    static bool ReadVersion(std::string_view version, unsigned int& major) {
        if (version.empty()) {
            return false;
        }

        const char* cursor = version.data();
        const char* end = cursor + version.size();
        if (!ReadComponent(cursor, end, major)) {
            return false;
        }

        constexpr int maxSeparators = 2;
        int separators = 0;
        for (; cursor != end && separators < maxSeparators; ++separators) {
            unsigned int component = 0;
            if (*cursor++ != '.' || !ReadComponent(cursor, end, component)) {
                return false;
            }
        }
        return separators > 0 && cursor == end;
    }

    static bool ReadComponent(const char*& cursor, const char* end, unsigned int& value) {
        constexpr unsigned int maxComponent = 255;
        const auto result = std::from_chars(cursor, end, value);
        cursor = result.ptr;
        return result.ec == std::errc{} && value <= maxComponent;
    }

    enum class State { Idle, AwaitingReply, Enabled };
    State state_ = State::Idle;
};

} // namespace cse
