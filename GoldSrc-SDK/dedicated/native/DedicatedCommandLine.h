#pragma once
#include "../../public/native_engine_arguments.h"
#include "AdminCommands.h"
#include <cstring>
#include <string>
#include <vector>

namespace cse::dedicated {
class DedicatedCommandLine {
  public:
    // sys_dll2's m_OrigCmd includes its terminating NUL.
    static constexpr std::size_t CommandCapacity = native_arguments::CommandCapacity;
    bool Parse(int argc, const char* const* argv, std::string& error) {
        std::string encoded;
        std::vector<std::string> arguments;
        bool console = false;
        if (argc <= 0 || !argv) {
            error = "Missing executable argument";
            return false;
        }
        if (argc > native_arguments::MaximumEncodedArguments) {
            error = "Dedicated command exceeds the engine argument count";
            return false;
        }
        for (int index = 0; index < argc; ++index) {
            if (!argv[index]) {
                error = "Null command argument";
                return false;
            }
            const std::string argument(argv[index]);
            for (const unsigned char byte : argument) {
                if (!native_arguments::TokenByte(byte)) {
                    error = "Control byte cannot round-trip through the engine command parser";
                    return false;
                }
            }
            if (argument.find_first_of("\"\r\n\t") != std::string::npos || argument.find('@') != std::string::npos) {
                error = "Argument cannot round-trip through the legacy command parser";
                return false;
            }
            if (argument == "-usegh" || argument == "-HFILE" || argument == "-HPARENT" || argument == "-HCHILD") {
                error = "Windows-only dedicated option: " + argument;
                return false;
            }
            console = console || argument == "-console";
            if (index != 0) {
                encoded += ' ';
            }
            const bool quote = argument.empty() || argument.find(' ') != std::string::npos;
            if (quote) {
                encoded += '"';
            }
            encoded += argument;
            if (quote) {
                encoded += '"';
            }
            if (encoded.size() >= CommandCapacity) {
                error = "Dedicated command exceeds 1023 encoded bytes";
                return false;
            }
            arguments.push_back(argument);
        }
        if (!native_arguments::Validate(encoded.c_str())) {
            error = "Dedicated startup command cannot round-trip through the engine command buffer";
            return false;
        }
        encoded_ = std::move(encoded);
        arguments_ = std::move(arguments);
        console_ = console;
        error.clear();
        return true;
    }
    bool ApplySetup(const StartupSettings& settings, std::string& error) {
        constexpr int MaximumPlayers = 32;
        constexpr int MaximumPort = 65535;
        if (arguments_.empty() || settings.game.empty() || settings.map.empty() || settings.maximumPlayers < 1 ||
            settings.maximumPlayers > MaximumPlayers || settings.port < 1 || settings.port > MaximumPort) {
            error = "Invalid dedicated game, map, player limit or port";
            return false;
        }
        std::vector<std::string> arguments{arguments_.front()};
        for (std::size_t index = 1; index < arguments_.size(); ++index) {
            const auto& argument = arguments_[index];
            if (argument == "-insecure") {
                continue;
            }
            if (argument == "-game" || argument == "-maxplayers" || argument == "+sv_lan" || argument == "+map" ||
                argument == "-port") {
                if (index + 1 < arguments_.size()) {
                    const auto& value = arguments_[index + 1];
                    const bool quoted = value.empty() || value.find(' ') != std::string::npos;
                    if (!native_arguments::StartupBoundary(value.c_str(), quoted)) {
                        ++index;
                    }
                }
                continue;
            }
            arguments.push_back(argument);
        }
        arguments.insert(arguments.end(),
                         {"-game", settings.game, "-maxplayers", std::to_string(settings.maximumPlayers), "+sv_lan",
                          settings.lan ? "1" : "0", "+map", settings.map, "-port", std::to_string(settings.port)});
        if (!settings.secure) {
            arguments.push_back("-insecure");
        }
        std::vector<const char*> values;
        for (const auto& argument : arguments) {
            values.push_back(argument.c_str());
        }
        // Parse commits atomically only after the final count, encoding and engine buffer checks pass.
        return Parse(static_cast<int>(values.size()), values.data(), error);
    }
    const std::string& Encoded() const noexcept {
        return encoded_;
    }
    const std::vector<std::string>& Arguments() const noexcept {
        return arguments_;
    }
    bool Console() const noexcept {
        return console_;
    }

  private:
    std::string encoded_;
    std::vector<std::string> arguments_;
    bool console_ = false;
};
} // namespace cse::dedicated
