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
    const std::string path = "Software\\CounterStrikeExtendedFixture\\MacSettingsDiagnostic-" +
        std::to_string(GetCurrentProcessId()) + "-" + std::to_string(GetTickCount64());
    HKEY handle = nullptr;
    if (RegCreateKeyExA(HKEY_CURRENT_USER, path.c_str(), 0, nullptr, REG_OPTION_NON_VOLATILE,
        KEY_READ | KEY_WRITE, nullptr, &handle, nullptr) != ERROR_SUCCESS) {
        return 1;
    }
    vgui::App app;
    const std::string key = path + "\\value";
    const std::array<BYTE, 2> shortBytes{{0x41, 0}};
    for (DWORD type : {static_cast<DWORD>(REG_SZ), static_cast<DWORD>(REG_BINARY)}) {
        for (DWORD storedBytes : {static_cast<DWORD>(0), static_cast<DWORD>(1), static_cast<DWORD>(2)}) {
            if (RegSetValueExA(handle, "value", 0, type, shortBytes.data(), storedBytes) != ERROR_SUCCESS) {
                return 2;
            }
            for (DWORD capacity : {static_cast<DWORD>(0), static_cast<DWORD>(1), static_cast<DWORD>(2),
                static_cast<DWORD>(3), static_cast<DWORD>(4), static_cast<DWORD>(8)}) {
                std::array<BYTE, 8> output{{0x78, 0x56, 0x34, 0x12, 0xa5, 0xa5, 0xa5, 0xa5}};
                DWORD required = capacity;
                const LSTATUS code = RegQueryValueExA(handle, "value", nullptr, nullptr, output.data(), &required);
                std::printf("type=%lu stored=%lu capacity=%lu code=%ld required=%lu bytes=", type, storedBytes,
                    capacity, code, required);
                for (BYTE value : output) {
                    std::printf("%02x", static_cast<unsigned int>(value));
                }
                std::puts("");
            }
            int integer = 0x12345678;
            const bool result = app.getRegistryInteger(key.c_str(), integer);
            std::printf("App_GetInteger type=%lu stored=%lu result=%d value=%08x\n", type, storedBytes,
                result ? 1 : 0, static_cast<unsigned int>(integer));
        }
    }
    RegCloseKey(handle);
    const LSTATUS cleanup = RegDeleteTreeA(HKEY_CURRENT_USER, path.c_str());
    std::printf("owned_registry_cleanup=%ld\n", cleanup);
    return cleanup == ERROR_SUCCESS ? 0 : 1;
}
