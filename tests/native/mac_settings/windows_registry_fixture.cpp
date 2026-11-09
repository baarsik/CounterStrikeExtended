#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <array>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <string>

#define REGISTRY_NAME_CAPACITY 256
namespace vgui {
class App {
public:
    bool setRegistryString(const char* key, const char* value);
    bool getRegistryString(const char* key, char* buffer, int capacity);
    bool setRegistryInteger(const char* key, int value);
    bool getRegistryInteger(const char* key, int& value);
};
void vgui_strcpy(char* destination, int capacity, const char* source);
}
#include "app_registry.inc"

namespace {
void Require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}
}

int main() {
    static_assert(sizeof(int) == 4, "The retained App integer uses four bytes");
    const std::string path = "Software\\CounterStrikeExtendedFixture\\MacSettings-" +
        std::to_string(GetCurrentProcessId()) + "-" + std::to_string(GetTickCount64());
    HKEY handle = nullptr;
    int result = 1;
    try {
        Require(RegCreateKeyExA(HKEY_CURRENT_USER, path.c_str(), 0, nullptr, REG_OPTION_NON_VOLATILE,
            KEY_READ | KEY_WRITE, nullptr, &handle, nullptr) == ERROR_SUCCESS, "owned fixture key creation failed");
        vgui::App app;
        const std::string key = path + "\\value";
        Require(app.setRegistryInteger(key.c_str(), -1), "integer setter failed");
        std::array<unsigned char, 8> output;
        output.fill(0xa5);
        Require(app.getRegistryString(key.c_str(), reinterpret_cast<char*>(output.data()),
            static_cast<int>(output.size())), "integer-to-string raw getter failed");
        Require(output[0] == 0xff && output[3] == 0xff && output[4] == 0xa5,
            "raw integer bytes or suffix changed");
        Require(app.setRegistryString(key.c_str(), "A"), "string setter failed");
        int integer = 0x12345678;
        Require(app.getRegistryInteger(key.c_str(), integer) && integer == 0x00000041,
            "short String staging residue differs");
        const std::array<BYTE, 2> shortRaw{{0x41, 0}};
        Require(RegSetValueExA(handle, "value", 0, REG_BINARY, shortRaw.data(),
            static_cast<DWORD>(shortRaw.size())) == ERROR_SUCCESS, "short Binary setup failed");
        integer = 0x12345678;
        Require(app.getRegistryInteger(key.c_str(), integer) && integer == 0x12340041,
            "short Binary did not preserve caller suffix");
        Require(RegSetValueExA(handle, "value", 0, REG_BINARY, nullptr, 0) == ERROR_SUCCESS,
            "empty raw data setup failed");
        Require(app.getRegistryInteger(key.c_str(), integer) && integer == 0x12340041,
            "empty raw query changed caller");
        const std::array<BYTE, 5> raw{{0x80, 0, 0xff, 0xc0, 7}};
        Require(RegSetValueExA(handle, "value", 0, REG_BINARY, raw.data(), static_cast<DWORD>(raw.size())) ==
            ERROR_SUCCESS, "raw setup failed");
        DWORD required = 1;
        Require(RegQueryValueExA(handle, "value", nullptr, nullptr, nullptr, &required) == ERROR_SUCCESS &&
            required == raw.size(), "null buffer did not return required size");
        std::array<BYTE, 4> undersized{{}};
        required = static_cast<DWORD>(undersized.size());
        Require(RegQueryValueExA(handle, "value", nullptr, nullptr, undersized.data(), &required) == ERROR_MORE_DATA &&
            required == raw.size(), "undersize query did not return required size");
        // ERROR_MORE_DATA destination contents are deliberately not asserted.
        Require(!app.getRegistryInteger(key.c_str(), integer), "oversize App integer query accepted");
        Require(!app.getRegistryString((path + "\\missing").c_str(), reinterpret_cast<char*>(output.data()),
            static_cast<int>(output.size())), "missing App value accepted");
        Require(app.setRegistryString(key.c_str(), ""), "empty string setter failed");
        output.fill(0xa5);
        Require(app.getRegistryString(key.c_str(), reinterpret_cast<char*>(output.data()), 1) && output[0] == 0 &&
            output[1] == 0xa5, "empty string does not include exactly one NUL");
        Require(!app.setRegistryString("", "invalid") && !app.setRegistryString((path + "\\").c_str(), "invalid"),
            "empty/trailing separator names accepted");
        result = 0;
        std::puts("PASS actual_App_methods raw_integer_LE string_NUL short_cross_type zero_raw capacity required missing");
    } catch (const std::exception& error) {
        std::printf("FAIL %s\n", error.what());
    }
    if (handle) {
        RegCloseKey(handle);
    }
    const LSTATUS cleanup = RegDeleteTreeA(HKEY_CURRENT_USER, path.c_str());
    if (cleanup != ERROR_SUCCESS) {
        std::puts("FAIL owned_registry_cleanup");
        result = 1;
    }
    std::printf("owned_registry_cleanup=%ld exit=%d\n", cleanup, result);
    return result;
}
