#include "dedicated/native/AdminFiles.h"
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sys/stat.h>
#include <unistd.h>
#define CHECK(expr)                                                                                                    \
    do {                                                                                                               \
        if (!(expr)) {                                                                                                 \
            std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr);                                       \
            std::exit(1);                                                                                              \
        }                                                                                                              \
    } while (false)
int main() {
    namespace files = cse::dedicated::files;
    namespace fs = std::filesystem;
    char pattern[] = "/tmp/cse-admin-files-XXXXXX";
    const char* root = mkdtemp(pattern);
    CHECK(root);
    const fs::path directory = fs::path(root) / "zażółć space";
    fs::create_directory(directory);
    fs::create_directory(directory / "empty");
    fs::create_directory(directory / "locked");
    std::ofstream(directory / "bans.CfG") << "banid 0 STEAM_0:1:123\n";
    std::ofstream(directory / "ignored.txt") << "ignore";
    fs::create_directory_symlink(directory / "empty", directory / "link");
    fs::create_symlink(directory / "missing", directory / "dangling.cfg");
    CHECK(files::Parent("/").path == "/");
    CHECK(!files::List("", "*").ok);
    CHECK(files::List((directory / "empty").u8string(), "*").ok);
    CHECK(files::List((directory / "empty").u8string(), "*").entries.empty());
    auto listing = files::List(directory.u8string(), "*.cfg");
    CHECK(listing.ok && listing.entries.size() == 5); // directories and dangling links retain their rows
    bool danglingLinkListed = false;
    for (const auto& entry : listing.entries) {
        if (entry.name == "dangling.cfg" && !entry.directory) {
            danglingLinkListed = true;
        }
    }
    CHECK(danglingLinkListed);
    CHECK(files::Stat((directory / "link").u8string()).directory);
    CHECK(files::Join(directory.u8string(), "bans.CfG").path == (directory / "bans.CfG").u8string());
    CHECK(!files::Stat((directory / "missing").u8string()).ok);
    CHECK(files::Parent(directory.u8string() + "/").path == fs::path(root).u8string());
    CHECK(files::PrepareConfigDirectory((directory / "new-config").u8string()).ok);
    CHECK(fs::is_empty(directory / "new-config"));
    CHECK(files::PrepareConfigDirectory((directory / "new-config").u8string()).ok);
    CHECK(fs::is_empty(directory / "new-config"));
    CHECK(!files::PrepareConfigDirectory("relative-config").ok);
    chmod((directory / "locked").c_str(), 0);
    if (geteuid() != 0) {
        CHECK(!files::List((directory / "locked").u8string(), "*").ok);
        CHECK(!files::PrepareConfigDirectory((directory / "locked").u8string()).ok);
    }
    chmod((directory / "locked").c_str(), 0700);
    const char drives[] = "/\0/Volumes/Extra disk/\0\0";
    auto roots = files::ParseRoots(drives, sizeof(drives));
    CHECK(roots.size() == 2 && roots[1] == "/Volumes/Extra disk/");
    CHECK(files::ParseRoots("bad", 3).empty());
    fs::remove_all(root);
}
