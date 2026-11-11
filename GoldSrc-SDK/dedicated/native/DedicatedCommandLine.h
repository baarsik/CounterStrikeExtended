#pragma once
#include "../../public/native_engine_arguments.h"
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
        encoded_ = std::move(encoded);
        arguments_ = std::move(arguments);
        console_ = console;
        error.clear();
        return true;
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
