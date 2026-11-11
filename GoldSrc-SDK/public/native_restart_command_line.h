#pragma once

#include <cctype>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

namespace cse_restart {
// Existing IEngineAPI::Run caller-owned generated restart buffer, including NUL.
constexpr std::size_t GeneratedCommandCapacity = 2048;
constexpr char ResumeCommandLineSwitch[] = "--cse-resume-command-line";
constexpr std::size_t MaximumCommandLineBytes = 4095;
constexpr int NativeArgumentsOffset = 3;

inline const char* CanonicalLine(int count, const char* const* arguments) {
    return count > NativeArgumentsOffset && arguments && arguments[1] && arguments[2] &&
        std::strcmp(arguments[1], ResumeCommandLineSwitch) == 0 &&
        std::strlen(arguments[2]) <= MaximumCommandLineBytes ? arguments[2] : nullptr;
}

// The legacy engine string is not a reversible encoding of arbitrary argv.
// Preserve a separate, lossless native argument list across process replacement.
inline std::vector<std::string> ProcessArguments(int count, const char* const* arguments) {
    std::vector<std::string> result;
    const int begin = CanonicalLine(count, arguments) ? NativeArgumentsOffset : 0;
    for (int index = begin; index < count; ++index) {
        result.emplace_back(arguments[index]);
    }
    return result;
}

// Only engine-generated replacement options use GoldSrc quoted-token syntax;
// original native arguments never pass through this parser or response expansion.
inline std::vector<std::string> GeneratedArguments(const char* line) {
    std::vector<std::string> result;
    if (!line) {
        return result;
    }
    while (*line) {
        while (*line && std::isspace(static_cast<unsigned char>(*line))) {
            ++line;
        }
        if (!*line) {
            break;
        }
        std::string argument;
        if (*line == '"') {
            ++line;
            while (*line && *line != '"') {
                argument += *line++;
            }
            if (*line == '"') {
                ++line;
            }
        } else {
            while (*line && !std::isspace(static_cast<unsigned char>(*line))) {
                argument += *line++;
            }
        }
        result.push_back(std::move(argument));
    }
    return result;
}
}
