#include "interface_language_preference.h"
#include "interface_language.h"
#include "windows.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

namespace {

std::string record;
DWORD recordType = REG_SZ;
LONG openStatus = ERROR_SUCCESS;
LONG queryStatus = ERROR_SUCCESS;
int openedKeys = 0;
int closedKeys = 0;

void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", message);
        std::exit(1);
    }
}

struct System {
    std::string requested;

    bool GetCommandLineParamValue(const char* name, char* output, int capacity) {
        Require(std::strcmp(name, "-language") == 0 && capacity == 128, "bound dedicated CLI read");
        if (requested.empty()) {
            return false;
        }
        std::strncpy(output, requested.c_str(), static_cast<std::size_t>(capacity - 1));
        output[capacity - 1] = '\0';
        return true;
    }

    // Model the actual ISystem string API: it discards type and returned length.
    bool GetRegistryString(const char*, char* output, int capacity) {
        if (openStatus != ERROR_SUCCESS || queryStatus != ERROR_SUCCESS ||
            record.size() > static_cast<std::size_t>(capacity)) {
            return false;
        }
        std::memcpy(output, record.data(), record.size());
        return true;
    }
};

void Check(const std::string& bytes, const char* expected) {
    record = bytes;
    Require(cse::interface_language::ReadSavedPreference() == expected, "validate Windows saved language record");
    System system;
    const std::string selected = *expected ? expected : "english";
    Require(cse::interface_language::ResolveFromSystem("english", &system) == selected,
        "dedicated selection agrees with strict native malformed-record validation");
    system.requested = "custom_language";
    Require(cse::interface_language::ResolveFromSystem("english", &system) == "custom_language",
        "safe custom CLI wins over valid or malformed native record");
    system.requested = "../invalid";
    Require(cse::interface_language::ResolveFromSystem("english", &system) == selected,
        "invalid CLI retains strict native record validation");
    Require(openedKeys == closedKeys, "close every successfully opened registry handle");
}

} // namespace

LONG RegOpenKeyExA(HKEY root, const char* path, DWORD options, DWORD access, HKEY* key) {
    Require(root == HKEY_CURRENT_USER &&
        std::strcmp(path, "Software\\CounterStrikeExtended\\Interface") == 0 && options == 0 &&
        access == KEY_QUERY_VALUE, "read CSE interface key using query-only access");
    if (openStatus == ERROR_SUCCESS) {
        ++openedKeys;
        *key = reinterpret_cast<HKEY>(2);
    }
    return openStatus;
}

LONG RegQueryValueExA(HKEY key, const char* name, DWORD* reserved, DWORD* type, BYTE* data, DWORD* bytes) {
    Require(key == reinterpret_cast<HKEY>(2) && std::strcmp(name, "Language") == 0 && !reserved,
        "read Language value from the opened key");
    Require(*bytes == 128, "bound early native registry read");
    *type = recordType;
    if (queryStatus != ERROR_SUCCESS) {
        return queryStatus;
    }
    if (record.size() > *bytes) {
        *bytes = static_cast<DWORD>(record.size());
        return ERROR_MORE_DATA;
    }
    std::memcpy(data, record.data(), record.size());
    *bytes = static_cast<DWORD>(record.size());
    return ERROR_SUCCESS;
}

LONG RegCloseKey(HKEY key) {
    Require(key == reinterpret_cast<HKEY>(2), "close the same opened key");
    ++closedKeys;
    return ERROR_SUCCESS;
}

int main() {
    Check(std::string("russian\0", 8), "russian");
    Check(std::string("PoLiSh\0", 7), "polish");
    Check(std::string("\0", 1), "");
    Check({}, "");
    Check("polish", "");
    Check(std::string("russian\0polish\0", 15), "");
    Check(std::string("../russian\0", 11), "");
    Check(std::string("custom_language\0", 16), "");
    Check(std::string(128, 'a'), "");
    Check(std::string(129, 'a'), "");
    recordType = REG_BINARY;
    Check(std::string("russian\0", 8), "");
    recordType = REG_SZ;
    openStatus = ERROR_FILE_NOT_FOUND;
    Check(std::string("russian\0", 8), "");
    openStatus = ERROR_SUCCESS;
    queryStatus = ERROR_FILE_NOT_FOUND;
    Check(std::string("russian\0", 8), "");
    std::puts("PASS: actual Windows startup reader against controlled read-only registry API");
}
