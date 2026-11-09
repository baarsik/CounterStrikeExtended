#include "native_path_prefix.h"

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>

int main(int argc, char** argv) {
    if (argc != 2) {
        return EXIT_FAILURE;
    }
    const auto home = std::filesystem::path(argv[1]);
    const auto existing = home / "owned-steam-content";
    const auto absent = home / "absent-steam-link";
    std::filesystem::remove_all(home);
    std::filesystem::create_directories(existing);
    constexpr std::size_t pathCapacity = 4096;
    char link[pathCapacity] = {};
    char* missingPrefix = cse::filesystem::ResolveSteamContentPrefixForHome(home.c_str(), link, sizeof(link));
    if (!missingPrefix || missingPrefix[0] != '\0') {
        std::fprintf(stderr, "FAIL: an absent owned Steam link must resolve to an empty prefix\n");
        return EXIT_FAILURE;
    }
    std::free(missingPrefix);
    std::filesystem::create_directories(home / ".steam");
    std::filesystem::create_symlink(existing, home / ".steam/steam");
    const std::string prefix = std::filesystem::canonical(existing).string() + "/";
    char* validPrefix = cse::filesystem::ResolveSteamContentPrefixForHome(home.c_str(), link, sizeof(link));
    if (!validPrefix || prefix != validPrefix) {
        std::fprintf(stderr, "FAIL: a valid owned Steam link must resolve to its canonical directory prefix\n");
        return EXIT_FAILURE;
    }
    std::free(validPrefix);
    const std::string path = prefix + "game/maps/test.bsp";
    if (cse::filesystem::MatchesSteamContentPrefix(path.c_str(), "", 0)) {
        std::fprintf(stderr, "FAIL: an unresolved empty Steam prefix must use ordinary path descent\n");
        return EXIT_FAILURE;
    }
    if (!cse::filesystem::MatchesSteamContentPrefix(path.c_str(), prefix.c_str(), prefix.size())) {
        std::fprintf(stderr, "FAIL: a valid owned Steam prefix must retain the prefix fast path\n");
        return EXIT_FAILURE;
    }
    if (std::filesystem::exists(absent)) {
        return EXIT_FAILURE;
    }
    const std::string ordinary = home.string() + "/ordinary/missing.txt";
    if (cse::filesystem::MatchesSteamContentPrefix(ordinary.c_str(), prefix.c_str(), prefix.size())) {
        std::fprintf(stderr, "FAIL: ordinary paths must not match an unrelated nonempty prefix\n");
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
