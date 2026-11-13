#include "ServerBrowserConfig.h"
#include "filesystem_fixture.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace cse::server_browser;
#define CHECK(value) do { if (!(value)) { throw std::runtime_error(#value); } } while (false)
struct MemoryStorage final : IConfigStorage {
    std::string bytes;
    bool missing = true;
    bool unreadable = false;
    bool failReplace = false;
    ReadResult Read(std::string& output, std::string& error) override {
        if (unreadable) {
            error = "unreadable";
            return ReadResult::Error;
        }
        output = bytes;
        return missing ? ReadResult::Missing : ReadResult::Ok;
    }
    bool AtomicReplace(std::string_view input, std::string& error) override {
        if (failReplace) {
            error = "replace failed";
            return false;
        }
        missing = false;
        bytes = input;
        return true;
    }
};
void MissingAndRoundTrip() {
    MemoryStorage storage;
    ServerBrowserConfig config(storage);
    CHECK(config.Load());
    auto settings = config.Settings();
    CHECK(!settings.savedPosition);
    settings.x = -280;
    settings.y = 140;
    settings.savedPosition = true;
    settings.filters.app = 70;
    settings.filters.map = "a=b\n\303\251%";
    settings.kind = ListKind::History;
    settings.sort = {Column::Ping, false};
    config.SetSettings(settings);
    CHECK(config.Status().dirty && config.Save() && !config.Status().dirty);
    ServerBrowserConfig second(storage);
    CHECK(second.Load());
    CHECK(second.Settings().savedPosition && second.Settings().x == -280 && second.Settings().y == 140);
    CHECK(second.Settings().filters.map == "a=b\n\303\251%");
    CHECK(second.Settings().filters.app == 70 && second.Settings().kind == ListKind::History);
    CHECK(second.Settings().sort.column == Column::Ping && !second.Settings().sort.ascending);
}
void PositionValidation() {
    MemoryStorage storage;
    storage.missing = false;
    storage.bytes = "x=not-a-coordinate\ny=24\nsavedposition=1\n";
    ServerBrowserConfig config(storage);
    CHECK(!config.Load());
    CHECK(!config.Save());
    for (const auto *value : {"2147483648", "-2147483649", "12x"}) {
        storage.bytes = std::string("savedposition=1\nx=") + value + "\ny=24\n";
        CHECK(!config.Load() && !config.Save());
    }
    storage.bytes = "originalui=1\nwidth=800\nheight=500\nsavedsize=1\n";
    CHECK(config.Load() && !config.Settings().savedPosition);
    auto settings = config.Settings();
    settings.savedPosition = true;
    settings.x = std::numeric_limits<int>::max();
    settings.y = std::numeric_limits<int>::min();
    config.SetSettings(settings);
    CHECK(config.Save());
    ServerBrowserConfig restored(storage);
    CHECK(restored.Load());
    CHECK(restored.Settings().savedPosition && restored.Settings().x == settings.x && restored.Settings().y == settings.y);
}
void UnknownAndFailures() {
    MemoryStorage storage;
    storage.missing = false;
    storage.bytes = "#future comment\nfuture-key=%zz opaque\r\nwidth=800\n";
    ServerBrowserConfig config(storage);
    CHECK(config.Load());
    auto settings = config.Settings();
    settings.width = 900;
    config.SetSettings(settings);
    const auto old = storage.bytes;
    storage.failReplace = true;
    CHECK(!config.Save() && storage.bytes == old && config.Status().dirty);
    storage.failReplace = false;
    CHECK(config.Save());
    CHECK(storage.bytes.find("#future comment\nfuture-key=%zz opaque\r\n") == 0);
    CHECK(storage.bytes.find("width=900\n") != std::string::npos);
    storage.bytes = "width=bad\n";
    CHECK(!config.Load());
    CHECK(!config.Save() && storage.bytes == "width=bad\n");
    storage.bytes = "name=%zz\n";
    CHECK(!config.Load() && !config.Save());
    storage.unreadable = true;
    CHECK(!config.Load() && !config.Save());
    storage.unreadable = false;
    storage.bytes = "width=800\n";
    CHECK(config.Load() && config.Save());
}
void RejectKnownStringNul() {
    MemoryStorage storage;
    storage.missing = false;
    storage.bytes = "future-key=" + std::string("x\0y", 3) + "\nname=valid\n";
    ServerBrowserConfig config(storage);
    CHECK(config.Load());
    const auto valid = config.Settings();
    CHECK(config.Save());
    const auto old = storage.bytes;
    CHECK(old.find(std::string("future-key=x\0y\n", 15)) != std::string::npos);
    for (auto member : {&Filters::gameDirectory, &Filters::map, &Filters::name}) {
        auto invalid = valid;
        invalid.filters.*member = std::string("a\0b", 3);
        config.SetSettings(invalid);
        CHECK(!config.Save());
        CHECK(storage.bytes == old && config.Status().dirty && !config.Status().error.empty());
        ServerBrowserConfig preserved(storage);
        CHECK(preserved.Load() && preserved.Settings().filters.name.empty());
    }
    config.SetSettings(valid);
    CHECK(config.Save());
    CHECK(storage.bytes == old);
    for (const char* key : {"game", "map", "name"}) {
        for (const auto& value : {std::string("a\0b", 3), std::string("a%00b")}) {
            storage.bytes = std::string(key) + '=' + value + '\n';
            const auto malformed = storage.bytes;
            CHECK(!config.Load());
            CHECK(!config.Status().available && !config.Save());
            CHECK(storage.bytes == malformed);
        }
    }
    storage.bytes = old;
    CHECK(config.Load() && config.Save() && storage.bytes == old);
}
void OriginalSettingsAndStableTabs() {
    MemoryStorage storage;
    storage.missing = false;
    storage.bytes = "kind=3\nwidth=640\nheight=480\nname=hidden\nregion=4\npassword=1\nmaxping=87\n";
    ServerBrowserConfig config(storage);
    CHECK(config.Load());
    auto settings = config.Settings();
    CHECK(settings.kind == ListKind::LAN);
    CHECK(BrowserTabKinds[3] == ListKind::Spectate && BrowserTabKinds[4] == ListKind::LAN);
    CHECK(settings.filters.name.empty() && settings.filters.region == -1);
    CHECK(settings.filters.password == Requirement::Any && settings.filters.maxPing == 0);
    CHECK(settings.height == 384 && !settings.savedSize);
    settings.kind = ListKind::Spectate;
    settings.filtersExpanded = true;
    settings.filters.password = Requirement::No;
    settings.filters.maxPing = 150;
    settings.filters.validSteamAccount = true;
    settings.sort = {Column::LastPlayed, false};
    auto &history = settings.pages[static_cast<size_t>(ListKind::History)];
    history.initialized = true;
    history.filters.map = "De_";
    history.filters.validSteamAccount = true;
    history.filtersExpanded = true;
    history.hiddenColumns = 1U << 5;
    config.SetSettings(settings);
    CHECK(config.Save());
    ServerBrowserConfig reopened(storage);
    CHECK(reopened.Load());
    const auto &restored = reopened.Settings();
    CHECK(restored.kind == ListKind::Spectate && !restored.filtersExpanded);
    CHECK(restored.filters.password == Requirement::No && restored.filters.maxPing == 150);
    CHECK(restored.filters.validSteamAccount && restored.sort.column == Column::LastPlayed && !restored.sort.ascending);
    const auto &savedHistory = restored.pages[static_cast<size_t>(ListKind::History)];
    CHECK(savedHistory.initialized && savedHistory.filters.map == "De_" && savedHistory.filters.validSteamAccount);
    CHECK(!savedHistory.filtersExpanded && savedHistory.hiddenColumns == 1U << 5);
}
void NativeAtomic(const std::filesystem::path& parent) {
    std::filesystem::create_directories(parent);
    // Every invocation owns a newly created child. Retain prior failure fixtures and
    // never remove an existing settings path supplied by the caller.
    constexpr size_t MaximumFixtureAttempts = 1024;
    std::filesystem::path root;
    for (size_t attempt = 0; attempt < MaximumFixtureAttempts; ++attempt) {
        const auto candidate = parent / ("native-atomic-" + std::to_string(attempt));
        if (std::filesystem::create_directory(candidate)) {
            root = candidate;
            break;
        }
    }
    CHECK(!root.empty());
    MountFilesystem filesystem;
    {
        NativeConfigStorage storage(&filesystem, root.string());
        CHECK(storage.Available());
        CHECK(storage.Directory() == std::filesystem::weakly_canonical(root / "server-browser").string());
        CHECK(filesystem.additions == 1 && filesystem.mounts.at(storage.Directory()) == "SERVERBROWSER");
        {
            NativeConfigStorage duplicate(&filesystem, root.string());
            CHECK(!duplicate.Available());
        }
        CHECK(filesystem.removals == 0 && filesystem.mounts.at("platform/config") == "CONFIG");
        ServerBrowserConfig config(storage);
        CHECK(config.Load());
        auto settings = config.Settings();
        settings.width = 876;
        config.SetSettings(settings);
        CHECK(config.Save());
        CHECK(std::filesystem::exists(root / "server-browser/settings.cfg"));
#if !defined(_WIN32)
        const auto directory = root / "server-browser";
        std::filesystem::permissions(directory, std::filesystem::perms::owner_read |
            std::filesystem::perms::owner_exec);
        const bool readOnlySaved = config.Save();
        std::filesystem::permissions(directory, std::filesystem::perms::owner_all);
        CHECK(!readOnlySaved && config.Status().dirty);
        ServerBrowserConfig preserved(storage);
        CHECK(preserved.Load() && preserved.Settings().width == 876);
#endif
        // A failed atomic replacement must keep the prior directory target intact.
        std::filesystem::remove(root / "server-browser/settings.cfg");
        std::filesystem::create_directory(root / "server-browser/settings.cfg");
        CHECK(!config.Save());
        CHECK(std::filesystem::is_directory(root / "server-browser/settings.cfg"));
    }
    CHECK(filesystem.removals == 1 && filesystem.mounts.size() == 1);
    NativeConfigStorage next(&filesystem, root.string());
    CHECK(next.Available());
}
int main(int argc, char** argv) {
    try {
        CHECK(argc == 2);
        PositionValidation();
        MissingAndRoundTrip();
        OriginalSettingsAndStableTabs();
        UnknownAndFailures();
        RejectKnownStringNul();
        NativeAtomic(std::filesystem::absolute(argv[1]));
        std::cout << "6 config cases passed\n";
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
