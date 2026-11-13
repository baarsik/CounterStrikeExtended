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

} // namespace interface_language
} // namespace cse
