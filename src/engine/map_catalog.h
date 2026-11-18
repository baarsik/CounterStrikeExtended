#pragma once

#include "../public/cse_posix_crt.h"

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <string>
#include <string_view>

namespace cse::maps {

inline std::filesystem::path NormalizedPath(std::string path) {
    std::replace(path.begin(), path.end(), '\\', '/');
    std::error_code error;
    auto absolute = std::filesystem::absolute(path, error);
    return error ? std::filesystem::path(path).lexically_normal() : absolute.lexically_normal();
}

inline bool IsSameMapDirectory(const std::filesystem::path& directory, const std::filesystem::path& expected) {
    if (directory == expected) {
        return true;
    }
    std::error_code error;
    if (std::filesystem::equivalent(directory, expected, error)) {
        return true;
    }
    // On a case-sensitive volume an exact game directory wins over a separate case variant.
    if (std::filesystem::exists(expected, error) || error) {
        return false;
    }

    const auto root = directory.parent_path();
    const auto expectedRoot = expected.parent_path();
    if (root == expectedRoot || std::filesystem::equivalent(root, expectedRoot, error)) {
        return true;
    }
    return !std::filesystem::exists(expectedRoot, error) && !error &&
        _stricmp(root.string().c_str(), expectedRoot.string().c_str()) == 0;
}

inline bool IsGameMapPath(
    const char* localPath, const char* baseDirectory, const char* gameDirectoryName, const std::string& language) {
    const auto mapsDirectory = NormalizedPath(localPath).parent_path();
    if (_stricmp(mapsDirectory.filename().string().c_str(), "maps") != 0) {
        return false;
    }
    const auto directory = mapsDirectory.parent_path();
    const auto gameDirectory = NormalizedPath(std::string(baseDirectory) + "/" + gameDirectoryName);
    const std::string directoryName = directory.filename().string();
    const std::string gameName = gameDirectory.filename().string();
    if (gameName.empty() || _strnicmp(directoryName.c_str(), gameName.c_str(), gameName.size()) != 0) {
        return false;
    }

    const auto* suffix = directoryName.c_str() + gameName.size();
    constexpr const char* OverlaySuffixes[] = {"", "_addon", "_hd", "_lv", "_downloads"};
    for (const auto* overlay : OverlaySuffixes) {
        if (_stricmp(suffix, overlay) == 0) {
            auto expected = gameDirectory;
            expected += overlay;
            return IsSameMapDirectory(directory, expected);
        }
    }

    const std::string languageSuffix = "_" + language;
    if (language.empty() || _stricmp(suffix, languageSuffix.c_str()) != 0) {
        return false;
    }
    auto expected = gameDirectory;
    expected += languageSuffix;
    if (IsSameMapDirectory(directory, expected)) {
        return true;
    }

    // FileSystem_SetGameDirectory also mounts localization next to the Windows game's base directory.
    const std::string_view base(baseDirectory);
    const auto gameRoot = base.find("\\game");
    if (gameRoot == std::string_view::npos) {
        return false;
    }
    const std::string external = std::string(base.substr(0, gameRoot)) + "/localization/" + gameDirectoryName;
    return IsSameMapDirectory(directory, NormalizedPath(external + languageSuffix));
}

} // namespace cse::maps
