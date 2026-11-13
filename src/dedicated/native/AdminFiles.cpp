#include "AdminFiles.h"
#include <algorithm>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fnmatch.h>
#include <unistd.h>

namespace cse::dedicated::files {
namespace fs = std::filesystem;
namespace {
Result Failure(const std::string& path, const std::string& error) {
    Result result;
    result.path = path;
    result.error = error;
    return result;
}
} // namespace
Result PrepareConfigDirectory(const std::string& path) {
    if (path.empty() || !fs::u8path(path).is_absolute()) {
        return Failure(path, "Dedicated CONFIG requires an absolute writable settings directory");
    }
    std::error_code error;
    fs::create_directories(fs::u8path(path), error);
    if (error) {
        return Failure(path, "Cannot create dedicated settings directory: " + error.message());
    }
    std::string probe = (fs::u8path(path) / ".cse-config-write-XXXXXX").u8string();
    const int descriptor = mkstemp(probe.data());
    if (descriptor < 0) {
        return Failure(path, "Cannot write dedicated settings directory: " + std::string(std::strerror(errno)));
    }
    constexpr char ProbeByte = 0;
    const auto bytes = write(descriptor, &ProbeByte, sizeof(ProbeByte));
    const bool written = bytes == sizeof(ProbeByte);
    const int writeError = written ? 0 : (bytes < 0 ? errno : EIO);
    const bool closed = close(descriptor) == 0;
    const int closeError = closed ? 0 : errno;
    const bool removed = unlink(probe.c_str()) == 0;
    const int removeError = removed ? 0 : errno;
    if (!written || !closed || !removed) {
        const int failure = writeError ? writeError : (closeError ? closeError : removeError);
        return Failure(path, "Cannot verify dedicated settings storage: " + std::string(std::strerror(failure)));
    }
    Result result;
    result.ok = true;
    result.directory = true;
    result.path = path;
    return result;
}
Result Stat(const std::string& path) {
    if (path.empty()) {
        return Failure(path, "No path selected");
    }
    std::error_code error;
    const auto status = fs::status(fs::u8path(path), error);
    if (error || !fs::exists(status)) {
        return Failure(path, error ? error.message() : "Path does not exist");
    }
    Result result;
    result.ok = true;
    result.directory = fs::is_directory(status);
    result.path = path;
    return result;
}
Result List(const std::string& path, const std::string& filter) {
    auto result = Stat(path);
    if (!result.ok || !result.directory) {
        return Failure(path, result.error.empty() ? "Path is not a directory" : result.error);
    }
    std::error_code error;
    fs::directory_iterator iterator(fs::u8path(path), error);
    const fs::directory_iterator end;
    while (!error && iterator != end) {
        const auto& entry = *iterator;
        const bool directory = entry.is_directory(error); // follow directory links, never recurse
        if (error) {
            break;
        }
        const std::string name = entry.path().filename().u8string();
        const char* pattern = filter.empty() || filter == "*.*" ? "*" : filter.c_str();
        if (directory || fnmatch(pattern, name.c_str(), 0) == 0) {
            result.entries.push_back({name, directory});
        }
        iterator.increment(error);
    }
    if (error) {
        return Failure(path, error.message());
    }
    std::sort(result.entries.begin(), result.entries.end(),
              [](const Entry& left, const Entry& right) { return left.name < right.name; });
    return result;
}
Result Parent(const std::string& path) {
    if (path.empty()) {
        return Failure(path, "No path selected");
    }
    auto current = fs::u8path(path).lexically_normal();
    if (current.filename().empty() && current != current.root_path()) {
        current = current.parent_path();
    }
    auto parent = current.parent_path();
    if (parent.empty()) {
        parent = current.root_path().empty() ? fs::path(".") : current.root_path();
    }
    Result result;
    result.ok = true;
    result.path = parent.u8string();
    return result;
}
Result Join(const std::string& directory, const std::string& name) {
    if (directory.empty() || name.empty()) {
        return Failure(directory, "No path selected");
    }
    Result result;
    result.ok = true;
    result.path = (fs::u8path(directory) / fs::u8path(name)).lexically_normal().u8string();
    return result;
}
std::vector<std::string> ParseRoots(const char* roots, std::size_t bytes) {
    std::vector<std::string> result;
    while (roots && bytes && *roots) {
        const auto* end = static_cast<const char*>(std::memchr(roots, 0, bytes));
        if (!end) {
            return {};
        }
        result.emplace_back(roots, end);
        const auto consumed = static_cast<std::size_t>(end - roots) + 1;
        bytes -= consumed;
        roots += consumed;
    }
    return result;
}
} // namespace cse::dedicated::files
