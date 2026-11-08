#include <cstdio>
#include <filesystem>
#include "../../../GoldSrc-SDK/public/cse_directories.h"

int main(int argc, char **argv) {
    if (argc != 2) {
        return 1;
    }
    const std::filesystem::path root(argv[1]);
    // CTest owns this fixed build-directory fixture; no arbitrary existing tree is removed.
    if (std::filesystem::exists(root)) {
        std::fputs("Directory fixture must start absent\n", stderr);
        return 1;
    }
    const auto child = root / "child";
    const auto file = root / "regular-file";
    if (cse::platform::CreateDirectoryPath(child.string().c_str())
        || !cse::platform::CreateDirectoryPath(root.string().c_str())
        || cse::platform::CreateDirectoryPath(root.string().c_str())
        || !cse::platform::CreateDirectoryPath(child.string().c_str())) {
        return 1;
    }
    auto *stream = std::fopen(file.string().c_str(), "wb");
    if (!stream || std::fclose(stream) != 0 || cse::platform::CreateDirectoryPath(file.string().c_str())) {
        return 1;
    }
    return std::filesystem::remove(file) && std::filesystem::remove(child) && std::filesystem::remove(root) ? 0 : 1;
}
