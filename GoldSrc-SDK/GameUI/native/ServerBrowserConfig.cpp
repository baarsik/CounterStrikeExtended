#include "ServerBrowserConfig.h"
#include "../../public/FileSystem.h"
#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_stdinc.h>

#include <algorithm>
#include <charconv>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <map>
#include <mutex>
#include <set>
#include <thread>
#if defined(_WIN32)
#include <Windows.h>
#else
#include <unistd.h>
#endif

namespace cse::server_browser {
namespace {
constexpr char PreferenceOrganization[] = "CounterStrikeExtended";
#if defined(_DEBUG)
constexpr char PreferenceApplication[] = "CounterStrikeExtended Debug";
#else
constexpr char PreferenceApplication[] = "CounterStrikeExtended";
#endif
constexpr char BrowserDirectory[] = "server-browser";
constexpr char BrowserPathId[] = "SERVERBROWSER";
constexpr char SettingsFilename[] = "settings.cfg";
constexpr size_t MaximumConfigBytes = 1024 * 1024;
constexpr int MinimumDimension = 320;
constexpr int MaximumDimension = 16384;
constexpr int MaximumRegion = 7;
constexpr int NoRegion = -1;
std::mutex mountMutex;
std::set<std::string> ownedDirectories;

void CheckThread(std::thread::id thread) {
    if (thread != std::this_thread::get_id()) {
        std::abort();
    }
}
using Fields = std::map<std::string, std::string>;
constexpr const char *PageFilterFields[]{"app", "game", "map", "name", "nonempty", "notfull",
    "password", "secure", "maxping", "region", "validaccount"};
Fields EncodeFields(const BrowserSettings& settings, bool includePages = true) {
    Fields fields = {{"kind", std::to_string(static_cast<int>(settings.kind))},
        {"app", std::to_string(settings.filters.app)}, {"game", settings.filters.gameDirectory},
        {"map", settings.filters.map}, {"name", settings.filters.name},
        {"nonempty", settings.filters.nonempty ? "1" : "0"}, {"notfull", settings.filters.notFull ? "1" : "0"},
        {"password", std::to_string(static_cast<int>(settings.filters.password))},
        {"secure", std::to_string(static_cast<int>(settings.filters.secure))},
        {"maxping", std::to_string(settings.filters.maxPing)}, {"region", std::to_string(settings.filters.region)},
        {"sort", std::to_string(static_cast<int>(settings.sort.column))},
        {"ascending", settings.sort.ascending ? "1" : "0"},
        {"width", std::to_string(settings.width)}, {"height", std::to_string(settings.height)},
        {"originalui", "1"},
        {"savedsize", settings.savedSize ? "1" : "0"},
        {"validaccount", settings.filters.validSteamAccount ? "1" : "0"}};
    if (includePages) {
        for (size_t index = 0; index < settings.pages.size(); ++index) {
            BrowserSettings page;
            page.filters = settings.pages[index].filters;
            page.filtersExpanded = settings.pages[index].filtersExpanded;
            const std::string prefix = "page" + std::to_string(index) + ".";
            const auto pageFields = EncodeFields(page, false);
            for (const char *key : PageFilterFields) {
                fields[prefix + key] = pageFields.at(key);
            }
            fields[prefix + "initialized"] = settings.pages[index].initialized ? "1" : "0";
            fields[prefix + "hidden"] = std::to_string(settings.pages[index].hiddenColumns);
        }
    }
    return fields;
}
std::string Escape(std::string_view value) {
    constexpr char Hex[] = "0123456789ABCDEF";
    constexpr unsigned char FirstPrintable = 32;
    constexpr unsigned char LastPrintable = 126;
    constexpr unsigned char NibbleMask = 15;
    constexpr int NibbleBits = 4;
    std::string output;
    for (unsigned char character : value) {
        if (character < FirstPrintable || character > LastPrintable || character == '%' || character == '=') {
            output += '%';
            output += Hex[character >> NibbleBits];
            output += Hex[character & NibbleMask];
        } else {
            output += static_cast<char>(character);
        }
    }
    return output;
}
bool Unescape(std::string_view value, std::string& output) {
    const auto digit = [](char character) {
        if (character >= '0' && character <= '9') {
            return character - '0';
        }
        if (character >= 'A' && character <= 'F') {
            return character - 'A' + 10;
        }
        if (character >= 'a' && character <= 'f') {
            return character - 'a' + 10;
        }
        return -1;
    };
    output.clear();
    for (size_t index = 0; index < value.size(); ++index) {
        if (value[index] == '%') {
            if (index + 2 >= value.size()) {
                return false;
            }
            const int high = digit(value[index + 1]);
            const int low = digit(value[index + 2]);
            if (high < 0 || low < 0 || (high == 0 && low == 0)) {
                return false;
            }
            constexpr int HexRadix = 16;
            output += static_cast<char>(high * HexRadix + low);
            index += 2;
        } else {
            output += value[index];
        }
    }
    return true;
}
template<class Integer> bool Number(const std::string& text, Integer& value, Integer minimum, Integer maximum) {
    const auto parsed = std::from_chars(text.data(), text.data() + text.size(), value);
    return parsed.ec == std::errc{} && parsed.ptr == text.data() + text.size() && value >= minimum && value <= maximum;
}
bool DecodeFields(const Fields& fields, BrowserSettings& settings, bool includePages = true) {
    // Known strings share Unescape's NUL-free domain on both Load and Save.
    // Unknown raw fields remain opaque and are preserved separately.
    for (const char* key : {"game", "map", "name"}) {
        if (fields.at(key).find('\0') != std::string::npos) {
            return false;
        }
    }
    int kind = 0;
    int password = 0;
    int secure = 0;
    int column = 0;
    int ascending = 0;
    int nonempty = 0;
    int notFull = 0;
    int savedSize = 0;
    int validAccount = 0;
    constexpr int MaximumPing = 1000000;
    constexpr AppId_t MaximumApp = UINT32_MAX;
    if (!Number(fields.at("kind"), kind, 0, static_cast<int>(ListKind::Spectate)) ||
        !Number(fields.at("password"), password, 0, static_cast<int>(Requirement::No)) ||
        !Number(fields.at("secure"), secure, 0, static_cast<int>(Requirement::No)) ||
        !Number(fields.at("sort"), column, 0, static_cast<int>(Column::LastPlayed)) ||
        !Number(fields.at("ascending"), ascending, 0, 1) || !Number(fields.at("nonempty"), nonempty, 0, 1) ||
        !Number(fields.at("notfull"), notFull, 0, 1) ||
        !Number(fields.at("savedsize"), savedSize, 0, 1) ||
        !Number(fields.at("validaccount"), validAccount, 0, 1) ||
        !Number(fields.at("app"), settings.filters.app, AppId_t{0}, MaximumApp) ||
        !Number(fields.at("width"), settings.width, MinimumDimension, MaximumDimension) ||
        !Number(fields.at("height"), settings.height, MinimumDimension, MaximumDimension) ||
        !Number(fields.at("region"), settings.filters.region, NoRegion, MaximumRegion) ||
        !Number(fields.at("maxping"), settings.filters.maxPing, 0, MaximumPing)) {
        return false;
    }
    settings.kind = static_cast<ListKind>(kind);
    settings.filters.password = static_cast<Requirement>(password);
    settings.filters.secure = static_cast<Requirement>(secure);
    settings.sort = {static_cast<Column>(column), ascending != 0};
    settings.filters.nonempty = nonempty != 0;
    settings.filters.notFull = notFull != 0;
    settings.filters.gameDirectory = fields.at("game");
    settings.filters.map = fields.at("map");
    settings.filters.name = fields.at("name");
    settings.savedSize = savedSize != 0;
    settings.filters.validSteamAccount = validAccount != 0;
    if (includePages) {
        for (size_t index = 0; index < settings.pages.size(); ++index) {
            BrowserSettings page;
            auto pageFields = EncodeFields(page, false);
            const std::string prefix = "page" + std::to_string(index) + ".";
            for (const char *key : PageFilterFields) {
                pageFields[key] = fields.at(prefix + key);
            }
            int initialized = 0;
            constexpr uint32_t AllColumnBits = (1U << 10) - 1;
            if (!DecodeFields(pageFields, page, false) ||
                !Number(fields.at(prefix + "initialized"), initialized, 0, 1) ||
                !Number(fields.at(prefix + "hidden"), settings.pages[index].hiddenColumns, 0U, AllColumnBits)) {
                return false;
            }
            settings.pages[index].filters = page.filters;
            settings.pages[index].filtersExpanded = page.filtersExpanded;
            settings.pages[index].initialized = initialized != 0;
        }
    }
    return true;
}
}
struct ServerBrowserConfig::State {
    IConfigStorage& storage;
    std::thread::id thread = std::this_thread::get_id();
    BrowserSettings settings;
    ConfigStatus status;
    std::string unknown;
};
ServerBrowserConfig::ServerBrowserConfig(IConfigStorage& storage) : state_(new State{storage, {}, {}, {}, {}}) {
    state_->thread = std::this_thread::get_id();
}
ServerBrowserConfig::~ServerBrowserConfig() = default;
bool ServerBrowserConfig::Load() {
    auto& state = *state_;
    CheckThread(state.thread);
    state.status.available = false;
    std::string input;
    const ReadResult read = state.storage.Read(input, state.status.error);
    if (read == ReadResult::Error) {
        return false;
    }
    BrowserSettings settings;
    Fields fields = EncodeFields(settings);
    std::string unknown;
    std::set<std::string> knownSeen;
    size_t offset = 0;
    while (offset < input.size()) {
        const size_t end = input.find('\n', offset);
        const size_t next = end == std::string::npos ? input.size() : end + 1;
        std::string line = input.substr(offset, next - offset);
        while (!line.empty() && (line.back() == '\n' || line.back() == '\r')) {
            line.pop_back();
        }
        const size_t equal = line.find('=');
        const auto key = line.substr(0, equal);
        if (fields.count(key)) {
            std::string value;
            if (equal == std::string::npos || !knownSeen.insert(key).second ||
                !Unescape(std::string_view(line).substr(equal + 1), value)) {
                state.status.error = "Malformed or duplicate browser setting";
                return false;
            }
            fields[key] = std::move(value);
        } else {
            unknown.append(input, offset, next - offset);
        }
        offset = next;
    }
    if (!DecodeFields(fields, settings)) {
        state.status.error = "Invalid browser setting value";
        return false;
    }
    // Retire constraints the original controls cannot represent. Existing numeric tab IDs stay unchanged.
    const auto migrate = [](Filters &filters) {
        filters.name.clear();
        filters.region = NoRegion;
        if (filters.password == Requirement::Yes) {
            filters.password = Requirement::Any;
        }
        if (std::find(BrowserPingLimits.begin(), BrowserPingLimits.end(), filters.maxPing) == BrowserPingLimits.end()) {
            filters.maxPing = 0;
        }
    };
    migrate(settings.filters);
    for (auto &page : settings.pages) {
        migrate(page.filters);
    }
    if (!knownSeen.count("originalui")) {
        // The previous custom default was 640x480, not a user-chosen original-browser rectangle.
        settings.savedSize = knownSeen.count("width") && knownSeen.count("height") &&
            (settings.width != 640 || settings.height != 480);
        if (!settings.savedSize) {
            settings.width = 640;
            settings.height = 384;
        }
    }
    state.settings = std::move(settings);
    state.unknown = std::move(unknown);
    state.status = {true, false, {}};
    return true;
}
const BrowserSettings& ServerBrowserConfig::Settings() const {
    CheckThread(state_->thread);
    return state_->settings;
}
void ServerBrowserConfig::SetSettings(const BrowserSettings& settings) {
    CheckThread(state_->thread);
    state_->settings = settings;
    state_->status.dirty = true;
}
bool ServerBrowserConfig::Save() {
    auto& state = *state_;
    CheckThread(state.thread);
    state.status.dirty = true;
    if (!state.status.available) {
        return false;
    }
    const Fields fields = EncodeFields(state.settings);
    BrowserSettings checked;
    if (!DecodeFields(fields, checked)) {
        state.status.error = "Invalid browser setting value";
        return false;
    }
    std::string output = state.unknown;
    if (!output.empty() && output.back() != '\n') {
        output += '\n';
    }
    for (const auto& field : fields) {
        output += field.first + '=' + Escape(field.second) + '\n';
    }
    if (!state.storage.AtomicReplace(output, state.status.error)) {
        return false;
    }
    state.status.dirty = false;
    state.status.error.clear();
    return true;
}
ConfigStatus ServerBrowserConfig::Status() const {
    CheckThread(state_->thread);
    return state_->status;
}

struct NativeConfigStorage::State {
    IFileSystem* filesystem = nullptr;
    std::thread::id thread = std::this_thread::get_id();
    std::string directory;
    std::string error;
    bool ownsDirectory = false;
    bool mounted = false;
};
NativeConfigStorage::NativeConfigStorage(IFileSystem* filesystem, std::string preferenceRoot)
    : state_(new State) {
    state_->filesystem = filesystem;
    if (preferenceRoot.empty()) {
        char* path = SDL_GetPrefPath(PreferenceOrganization, PreferenceApplication);
        if (!path) {
            state_->error = "Could not resolve browser preferences";
            return;
        }
        preferenceRoot = path;
        SDL_free(path);
    }
    std::error_code error;
    const auto root = std::filesystem::u8path(preferenceRoot);
    if (!root.is_absolute()) {
        state_->error = "Browser preference root must be absolute";
        return;
    }
    const auto directory = std::filesystem::weakly_canonical(root / BrowserDirectory, error);
    if (error) {
        state_->error = error.message();
        return;
    }
    state_->directory = directory.u8string();
    {
        std::lock_guard<std::mutex> lock(mountMutex);
        if (!ownedDirectories.insert(state_->directory).second) {
            state_->error = "Browser preference directory already has an owner";
            return;
        }
        state_->ownsDirectory = true;
    }
    std::filesystem::create_directories(directory, error);
    if (error) {
        state_->error = error.message();
        return;
    }
    if (filesystem) {
        filesystem->AddSearchPath(state_->directory.c_str(), BrowserPathId);
        state_->mounted = true;
    }
}
NativeConfigStorage::~NativeConfigStorage() {
    CheckThread(state_->thread);
    if (state_->mounted) {
        state_->filesystem->RemoveSearchPath(state_->directory.c_str());
    }
    if (state_->ownsDirectory) {
        std::lock_guard<std::mutex> lock(mountMutex);
        ownedDirectories.erase(state_->directory);
    }
}
bool NativeConfigStorage::Available() const {
    CheckThread(state_->thread);
    return state_->ownsDirectory && state_->error.empty();
}
const std::string& NativeConfigStorage::Directory() const {
    CheckThread(state_->thread);
    return state_->directory;
}
ReadResult NativeConfigStorage::Read(std::string& output, std::string& error) {
    CheckThread(state_->thread);
    output.clear();
    error = state_->error;
    if (!Available()) {
        return ReadResult::Error;
    }
    const auto path = std::filesystem::u8path(state_->directory) / SettingsFilename;
    std::error_code code;
    const bool exists = std::filesystem::exists(path, code);
    if (code) {
        error = code.message();
        return ReadResult::Error;
    }
    if (!exists) {
        return ReadResult::Missing;
    }
    const auto size = std::filesystem::file_size(path, code);
    if (code || size > MaximumConfigBytes) {
        error = "Browser configuration is unreadable or oversized";
        return ReadResult::Error;
    }
    std::ifstream stream(path, std::ios::binary);
    output.resize(static_cast<size_t>(size));
    if (!stream || !stream.read(output.data(), static_cast<std::streamsize>(size))) {
        error = "Could not read browser configuration";
        return ReadResult::Error;
    }
    return ReadResult::Ok;
}
bool NativeConfigStorage::AtomicReplace(std::string_view bytes, std::string& error) {
    CheckThread(state_->thread);
    error = state_->error;
    if (!Available()) {
        return false;
    }
    const auto directory = std::filesystem::u8path(state_->directory);
    const auto target = directory / SettingsFilename;
#if defined(_WIN32)
    // CREATE_NEW reserves a same-directory temporary file without overwriting another writer.
    std::filesystem::path temporary;
    HANDLE file = INVALID_HANDLE_VALUE;
    constexpr unsigned int TemporaryAttempts = 128;
    for (unsigned int attempt = 0; attempt < TemporaryAttempts && file == INVALID_HANDLE_VALUE; ++attempt) {
        temporary = directory / ("settings.tmp-" + std::to_string(GetCurrentProcessId()) + '-' +
            std::to_string(GetTickCount64()) + '-' + std::to_string(attempt));
        file = CreateFileW(temporary.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (file == INVALID_HANDLE_VALUE && GetLastError() != ERROR_FILE_EXISTS) {
            break;
        }
    }
    if (file == INVALID_HANDLE_VALUE) {
        error = "Could not create browser configuration temporary file";
        return false;
    }
    DWORD written = 0;
    bool ok = bytes.size() <= MaximumConfigBytes && WriteFile(file, bytes.data(), static_cast<DWORD>(bytes.size()),
        &written, nullptr) && written == bytes.size() && FlushFileBuffers(file);
    ok = CloseHandle(file) && ok;
    if (ok) {
        ok = MoveFileExW(temporary.c_str(), target.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
    }
    if (!ok) {
        DeleteFileW(temporary.c_str());
    }
#else
    std::string temporary = (directory / "settings.tmp-XXXXXX").u8string();
    const int descriptor = mkstemp(temporary.data());
    if (descriptor < 0) {
        error = "Could not create browser configuration temporary file";
        return false;
    }
    FILE* file = fdopen(descriptor, "wb");
    bool ok = file != nullptr && bytes.size() <= MaximumConfigBytes;
    if (file) {
        ok = ok && std::fwrite(bytes.data(), 1, bytes.size(), file) == bytes.size();
        ok = std::fflush(file) == 0 && ok;
        ok = fsync(descriptor) == 0 && ok;
        ok = std::fclose(file) == 0 && ok;
    } else {
        close(descriptor);
    }
    if (ok) {
        ok = std::rename(temporary.c_str(), target.c_str()) == 0;
    }
    if (!ok) {
        std::remove(temporary.c_str());
    }
#endif
    if (!ok) {
        error = "Could not atomically replace browser configuration; previous file preserved";
    }
    return ok;
}
}
