#pragma once
#include <cstddef>
#include <string>
#include <vector>

namespace cse::dedicated::files {
struct Entry {
    std::string name;
    bool directory = false;
};
struct Result {
    bool ok = false;
    bool directory = false;
    std::string path;
    std::string error;
    std::vector<Entry> entries;
};
Result List(const std::string& path, const std::string& filter);
Result Stat(const std::string& path);
Result Parent(const std::string& path);
Result Join(const std::string& directory, const std::string& name);
std::vector<std::string> ParseRoots(const char* roots, std::size_t bytes);
} // namespace cse::dedicated::files
