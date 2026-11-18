#pragma once

#include <cstring>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>

// Anchors a relative name at game_dir and makes it absolute: the process working
// directory is not game_dir under Steam or the launcher.
inline std::filesystem::path RadarUtil_AnchorPath(const char* game_dir, const char* name) {
    std::filesystem::path path(name);

    if (!path.is_absolute() && game_dir && game_dir[0]) {
        path = std::filesystem::path(game_dir) / path;
    }

    std::error_code ec;
    std::filesystem::path abs = std::filesystem::absolute(path, ec);

    return ec ? path : abs;
}

// "maps/de_dust2.bsp" -> "de_dust2"; writes at most out.size() - 1 characters plus a null.
inline void RadarUtil_ExtractMapName(const char* level_name, std::span<char> out) {
    if (out.empty()) {
        return;
    }

    std::string_view filename(level_name);
    const auto separator = filename.find_last_of("/\\");
    if (separator != std::string_view::npos) {
        filename.remove_prefix(separator + 1);
    }
    const auto basename = filename.substr(0, filename.rfind('.'));
    const auto copied = basename.copy(out.data(), out.size() - 1);
    out[copied] = '\0';
}

inline char RadarUtil_ToSiteLetter(char c) {
    if (c == 'a' || c == 'A') {
        return 'A';
    }

    if (c == 'b' || c == 'B') {
        return 'B';
    }

    return 0;
}

inline bool RadarUtil_IsAsciiAlpha(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

inline char RadarUtil_ToLowerAscii(char c) {
    return (c >= 'A' && c <= 'Z') ? (char)(c - 'A' + 'a') : c;
}

inline bool RadarUtil_EqualsIgnoringAsciiCase(const char* a, const char* b) {
    while (*a && RadarUtil_ToLowerAscii(*a) == RadarUtil_ToLowerAscii(*b)) {
        a++;
        b++;
    }

    return RadarUtil_ToLowerAscii(*a) == RadarUtil_ToLowerAscii(*b);
}

// map_models are the models the map's hostage entities name: a stock hostage can wear
// models/scientist.mdl, which no name rule tells from a prop. The engine looks models up ignoring
// case and keeps the first spelling it loaded, so the comparison ignores case too.
inline bool RadarUtil_IsHostageModel(const char* path, std::span<const std::string> map_models) {
    for (const std::string& model : map_models) {
        if (RadarUtil_EqualsIgnoringAsciiCase(path, model.c_str())) {
            return true;
        }
    }

    // Substring, not exact match, so custom hostage model paths still classify.
    return std::strstr(path, "hostage") != nullptr;
}

// Bomb-target names encode the site letter as "tgt_a", "a_crates", "targeta" or "a".
// Returns 'A'/'B', or 0 when none of those boundaries match.
inline char RadarUtil_DetectSiteLetter(const char* s) {
    int n = (int)std::strlen(s);
    if (n == 0) {
        return 0;
    }

    if (n >= 2 && s[n - 2] == '_') {
        if (char r = RadarUtil_ToSiteLetter(s[n - 1])) {
            return r;
        }
    }

    if (n >= 2 && s[1] == '_') {
        if (char r = RadarUtil_ToSiteLetter(s[0])) {
            return r;
        }
    }

    if (char r = RadarUtil_ToSiteLetter(s[n - 1])) {
        return r;
    }

    if (n == 1 || !RadarUtil_IsAsciiAlpha(s[1])) // a leading 'a'/'b' only counts when it is not a word
    {
        if (char r = RadarUtil_ToSiteLetter(s[0])) {
            return r;
        }
    }

    return 0;
}
