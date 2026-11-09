#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <array>
#include <cstdio>
#include <cstring>
#include <string>

#define REGISTRY_NAME_CAPACITY 256
namespace vgui {
class App {
public:
    bool setRegistryString(const char*, const char*);
    bool getRegistryString(const char*, char*, int);
    bool setRegistryInteger(const char*, int);
    bool getRegistryInteger(const char*, int&);
};
void vgui_strcpy(char*, int, const char*);
}
#include "app_registry.inc"

int main() {
    const std::string path = "Software\\CounterStrikeExtendedFixture\\MacSettingsStrings-" +
        std::to_string(GetCurrentProcessId()) + "-" + std::to_string(GetTickCount64());
    HKEY handle = nullptr;
    if (RegCreateKeyExA(HKEY_CURRENT_USER, path.c_str(), 0, nullptr, REG_OPTION_NON_VOLATILE,
        KEY_READ | KEY_WRITE, nullptr, &handle, nullptr) != ERROR_SUCCESS) {
        return 1;
    }
    std::printf("ACP=%u\n", GetACP());
    vgui::App app;
    const std::string key = path + "\\value";
    const std::array<const char*, 10> strings{{"", "A", "AB", "ABC", "ABCD", "ABCDE",
        "\xc3\xa9", "\xd0\x96", "\xf0\x9f\x99\x82", "A\xc3\xa9Z"}};
    for (std::size_t index = 0; index < strings.size(); ++index) {
        if (!app.setRegistryString(key.c_str(), strings[index])) {
            RegCloseKey(handle);
            RegDeleteTreeA(HKEY_CURRENT_USER, path.c_str());
            return 2;
        }
        std::array<BYTE, 64> wide{{}};
        DWORD wideBytes = static_cast<DWORD>(wide.size());
        if (RegQueryValueExW(handle, L"value", nullptr, nullptr, wide.data(), &wideBytes) != ERROR_SUCCESS) {
            return 3;
        }
        std::printf("case=%zu input_bytes=%zu wide_bytes=%lu wide=", index, std::strlen(strings[index]) + 1, wideBytes);
        for (DWORD byte = 0; byte < wideBytes; ++byte) {
            std::printf("%02x", static_cast<unsigned int>(wide[byte]));
        }
        std::puts("");
        for (DWORD capacity = 0; capacity <= 16; ++capacity) {
            std::array<BYTE, 20> output;
            output.fill(0xa5);
            DWORD required = capacity;
            const LSTATUS code = RegQueryValueExA(handle, "value", nullptr, nullptr, output.data(), &required);
            std::printf("case=%zu capacity=%lu code=%ld required=%lu bytes=", index, capacity, code, required);
            for (BYTE value : output) {
                std::printf("%02x", static_cast<unsigned int>(value));
            }
            std::puts("");
        }
        int integer = 0x12345678;
        const bool result = app.getRegistryInteger(key.c_str(), integer);
        std::printf("App_GetInteger case=%zu result=%d value=%08x\n", index, result ? 1 : 0,
            static_cast<unsigned int>(integer));
    }
    RegCloseKey(handle);
    const LSTATUS cleanup = RegDeleteTreeA(HKEY_CURRENT_USER, path.c_str());
    std::printf("owned_registry_cleanup=%ld\n", cleanup);
    return cleanup == ERROR_SUCCESS ? 0 : 1;
}
