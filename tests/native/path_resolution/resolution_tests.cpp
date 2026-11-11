#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

namespace {
constexpr int GAME_DIRECTORY_BYTES = 260;
bool found = true;
std::string resolved;

const char* FS_GetLocalPath(const char*, char* destination, int capacity) {
    const char* value = found ? resolved.c_str() : "not-a-resolved-path";
    std::snprintf(destination, static_cast<std::size_t>(capacity), "%s", value);
    return found ? destination : nullptr;
}

void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "%s\n", message);
        std::exit(1);
    }
}
}
#include "actual_resolution.inc"

int main() {
    char path[GAME_DIRECTORY_BYTES] = "filesystem_stdio.dll";
    found = false;
    Require(COM_ExpandFilename(path) == 0, "failed VFS resolution must not report success");
    Require(path[0] == 0, "failed VFS resolution must not publish unresolved stack bytes");
    found = true;
    resolved = "/owned/runtime/filesystem_stdio.dylib";
    std::strcpy(path, "filesystem_stdio.dylib");
    Require(COM_ExpandFilename(path) == 1 && resolved == path, "successful VFS resolution");
    resolved.clear();
    Require(COM_ExpandFilename(path) == 0, "empty resolved path must not report success");
}
