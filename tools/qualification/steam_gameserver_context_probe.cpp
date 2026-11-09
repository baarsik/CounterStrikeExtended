// One-shot real Steam game-server bootstrap diagnostic. No logon or advertisement.
#include <windows.h>
#include <shellapi.h>
#include <sddl.h>
#include <tlhelp32.h>

#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace {

constexpr DWORD MaximumPathCharacters = 32768;
constexpr DWORD MaximumRegistryBytes = 65536;
constexpr std::uint32_t EngineUnconfiguredIP = 0;
constexpr std::uint16_t LegacySteamPort = 0;
constexpr std::uint16_t FixtureGamePort = 27441;
constexpr std::uint16_t SharedQueryPort = 0xffff;
constexpr int NoAuthentication = 1;
constexpr int Authentication = 2;
constexpr const char *EngineVersion = "1.1.2.7";
static_assert(sizeof(int) == 4, "The retained Steam EServerMode ABI is 32-bit");

template<typename Function>
Function Symbol(HMODULE library, const char *name) {
    const FARPROC address = GetProcAddress(library, name);
    static_assert(sizeof(Function) == sizeof(address), "Windows native function pointer width");
    Function result = nullptr;
    std::memcpy(&result, &address, sizeof(result));
    return result;
}

std::string Utf8(const std::wstring &value) {
    if (value.empty()) {
        return {};
    }
    const int count = WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()),
        nullptr, 0, nullptr, nullptr);
    std::string result(static_cast<std::size_t>(count), '\0');
    WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()),
        result.data(), count, nullptr, nullptr);
    return result;
}

std::string Json(const std::wstring &value) {
    std::ostringstream result;
    result << '"';
    for (const unsigned char character : Utf8(value)) {
        if (character == '\\' || character == '"') {
            result << '\\';
        }
        if (character >= ' ') {
            result << static_cast<char>(character);
        }
    }
    result << '"';
    return result.str();
}

std::wstring Environment(const wchar_t *name) {
    std::vector<wchar_t> value(MaximumPathCharacters);
    const DWORD length = GetEnvironmentVariableW(name, value.data(), static_cast<DWORD>(value.size()));
    if (length == 0 || length >= value.size()) {
        return {};
    }
    return {value.data(), length};
}

std::wstring Sid() {
    HANDLE token = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) {
        return {};
    }
    DWORD bytes = 0;
    GetTokenInformation(token, TokenUser, nullptr, 0, &bytes);
    std::vector<unsigned char> storage(bytes);
    std::wstring result;
    if (GetTokenInformation(token, TokenUser, storage.data(), bytes, &bytes)) {
        const auto user = reinterpret_cast<const TOKEN_USER *>(storage.data());
        wchar_t *sid = nullptr;
        if (ConvertSidToStringSidW(user->User.Sid, &sid)) {
            result = sid;
            LocalFree(sid);
        }
    }
    CloseHandle(token);
    return result;
}

std::string RegistryPath(HKEY root, const wchar_t *valueName, REGSAM view) {
    HKEY key = nullptr;
    const LSTATUS opened = RegOpenKeyExW(root, L"Software\\Valve\\Steam", 0, KEY_QUERY_VALUE | view, &key);
    if (opened != ERROR_SUCCESS) {
        return "{\"error\":" + std::to_string(opened) + "}";
    }
    DWORD type = 0;
    DWORD bytes = 0;
    LSTATUS status = RegQueryValueExW(key, valueName, nullptr, &type, nullptr, &bytes);
    std::wstring value;
    if (status == ERROR_SUCCESS && bytes <= MaximumRegistryBytes
        && (type == REG_SZ || type == REG_EXPAND_SZ)) {
        std::vector<wchar_t> buffer(bytes / sizeof(wchar_t) + 1);
        status = RegQueryValueExW(key, valueName, nullptr, &type,
            reinterpret_cast<BYTE *>(buffer.data()), &bytes);
        if (status == ERROR_SUCCESS) {
            value.assign(buffer.data());
        }
    }
    RegCloseKey(key);
    return "{\"error\":" + std::to_string(status) + ",\"path\":" + Json(value) + "}";
}

std::string Modules() {
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, GetCurrentProcessId());
    if (snapshot == INVALID_HANDLE_VALUE) {
        return "[]";
    }
    MODULEENTRY32W module = {};
    module.dwSize = sizeof(module);
    std::ostringstream result;
    result << '[';
    bool first = true;
    for (BOOL more = Module32FirstW(snapshot, &module); more; more = Module32NextW(snapshot, &module)) {
        const std::wstring name = module.szModule;
        if (_wcsicmp(name.c_str(), L"steam_api64.dll") != 0
            && _wcsicmp(name.c_str(), L"steamclient64.dll") != 0
            && _wcsicmp(name.c_str(), L"steamclient.dll") != 0
            && _wcsicmp(name.c_str(), L"tier0_s64.dll") != 0
            && _wcsicmp(name.c_str(), L"vstdlib_s64.dll") != 0) {
            continue;
        }
        if (!first) {
            result << ',';
        }
        first = false;
        result << "{\"name\":" << Json(name) << ",\"path\":" << Json(module.szExePath)
               << ",\"base\":" << reinterpret_cast<std::uintptr_t>(module.modBaseAddr) << '}';
    }
    result << ']';
    CloseHandle(snapshot);
    return result.str();
}

} // namespace

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
    int count = 0;
    wchar_t **arguments = CommandLineToArgvW(GetCommandLineW(), &count);
    if (!arguments || (count != 3 && count != 4)) {
        return 2;
    }
    const std::filesystem::path provider(arguments[1]);
    const std::filesystem::path receipt(arguments[2]);
    const int mode = count == 4 ? _wtoi(arguments[3]) : NoAuthentication;
    LocalFree(arguments);
    if (mode != NoAuthentication && mode != Authentication) {
        return 2;
    }
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX);
    SetEnvironmentVariableW(L"SteamAppId", L"10");
    SetEnvironmentVariableW(L"SteamGameId", L"10");
    DWORD session = 0;
    ProcessIdToSessionId(GetCurrentProcessId(), &session);
    std::ostringstream result;
    result << "{\"pid\":" << GetCurrentProcessId() << ",\"session\":" << session
           << ",\"sid\":" << Json(Sid()) << ",\"profile\":" << Json(Environment(L"USERPROFILE"))
           << ",\"hkcu32\":" << RegistryPath(HKEY_CURRENT_USER, L"SteamPath", KEY_WOW64_32KEY)
           << ",\"hkcu64\":" << RegistryPath(HKEY_CURRENT_USER, L"SteamPath", KEY_WOW64_64KEY)
           << ",\"hklm32\":" << RegistryPath(HKEY_LOCAL_MACHINE, L"InstallPath", KEY_WOW64_32KEY)
           << ",\"hklm64\":" << RegistryPath(HKEY_LOCAL_MACHINE, L"InstallPath", KEY_WOW64_64KEY)
           << ",\"arguments\":{\"ip\":" << EngineUnconfiguredIP << ",\"steam_port\":" << LegacySteamPort
           << ",\"game_port\":" << FixtureGamePort << ",\"query_port\":" << SharedQueryPort << ",\"mode\":"
           << mode << ",\"version\":\"" << EngineVersion << "\",\"app_id\":10}";
    HMODULE library = LoadLibraryExW(provider.c_str(), nullptr,
        LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
    const DWORD loadError = library ? ERROR_SUCCESS : GetLastError();
    result << ",\"provider_loaded\":" << (library ? "true" : "false");
    bool initialized = false;
    bool shutdownCalled = false;
    if (library) {
        using Initialize = bool (__cdecl *)(
            std::uint32_t, std::uint16_t, std::uint16_t, std::uint16_t, int, const char *);
        using Shutdown = void (__cdecl *)();
        const auto initialize = Symbol<Initialize>(library, "SteamInternal_GameServer_Init");
        const auto shutdown = Symbol<Shutdown>(library, "SteamGameServer_Shutdown");
        result << ",\"exports_found\":" << (initialize && shutdown ? "true" : "false");
        if (initialize && shutdown) {
            result << ",\"init_called\":true";
            initialized = initialize(EngineUnconfiguredIP, LegacySteamPort, FixtureGamePort,
                SharedQueryPort, mode, EngineVersion);
            result << ",\"modules_after_init\":" << Modules();
            if (initialized) {
                shutdown();
                shutdownCalled = true;
            }
            result << ",\"modules_after_shutdown\":" << Modules();
        }
        FreeLibrary(library);
    } else {
        result << ",\"load_error\":" << loadError;
    }
    result << ",\"init_succeeded\":" << (initialized ? "true" : "false")
           << ",\"shutdown_called\":" << (shutdownCalled ? "true" : "false")
           << ",\"logon_or_advertisement_called\":false}";
    std::ofstream output(receipt, std::ios::binary);
    output << result.str() << '\n';
    return output.good() ? 0 : 3;
}
