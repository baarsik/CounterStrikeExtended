#include "interface_language_preference.h"
#include "interface_language.h"

#include <cstring>

#if defined(_WIN32)
#include <windows.h>
#elif defined(__APPLE__)
#include "mac_settings_store.h"
#endif

namespace cse {
namespace interface_language {

std::string ReadSavedPreference() {
    char saved[PreferenceBufferBytes]{};
    std::size_t bytes = 0;
#if defined(_WIN32)
    // Keep the early engine read independent of VGUI and avoid creating keys.
    constexpr char RegistryPath[] = "Software\\CounterStrikeExtended\\Interface";
    constexpr char RegistryValue[] = "Language";
    HKEY key = nullptr;
    if (RegOpenKeyExA(HKEY_CURRENT_USER, RegistryPath, 0, KEY_QUERY_VALUE, &key) != ERROR_SUCCESS) {
        return {};
    }
    DWORD type = 0;
    DWORD storedBytes = sizeof(saved);
    const LONG status = RegQueryValueExA(key, RegistryValue, nullptr, &type,
        reinterpret_cast<BYTE*>(saved), &storedBytes);
    RegCloseKey(key);
    if (status != ERROR_SUCCESS || type != REG_SZ) {
        return {};
    }
    bytes = storedBytes;
#elif defined(__APPLE__)
    if (!mac_settings::ReadBytes(PreferenceKey, saved, sizeof(saved), &bytes)) {
        return {};
    }
#else
    return {};
#endif
    if (bytes == 0 || bytes > sizeof(saved) || saved[bytes - 1] != '\0') {
        return {};
    }
    // Reject hidden suffixes rather than treating a malformed record as a valid prefix.
    if (std::strlen(saved) + 1 != bytes) {
        return {};
    }
    const std::string language = Normalize(saved);
    return IsBundled(language) ? language : std::string{};
}

bool WriteSavedPreference(const char* preference) {
    if (!preference) {
        return false;
    }
    const std::string language = Normalize(preference);
    if (preference[0] && !IsBundled(language)) {
        return false;
    }
#if defined(_WIN32)
    constexpr char RegistryPath[] = "Software\\CounterStrikeExtended\\Interface";
    constexpr char RegistryValue[] = "Language";
    HKEY key = nullptr;
    if (RegCreateKeyExA(HKEY_CURRENT_USER, RegistryPath, 0, nullptr, 0, KEY_SET_VALUE, nullptr,
        &key, nullptr) != ERROR_SUCCESS) {
        return false;
    }
    const LONG status = RegSetValueExA(key, RegistryValue, 0, REG_SZ,
        reinterpret_cast<const BYTE*>(language.c_str()), static_cast<DWORD>(language.size() + 1));
    RegCloseKey(key);
    return status == ERROR_SUCCESS;
#elif defined(__APPLE__)
    return mac_settings::WriteString(PreferenceKey, language.c_str());
#else
    return false;
#endif
}

} // namespace interface_language
} // namespace cse
