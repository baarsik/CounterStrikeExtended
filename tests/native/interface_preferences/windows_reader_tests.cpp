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
LONG writeStatus = ERROR_SUCCESS;
int writes = 0;

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

LONG RegCreateKeyExA(HKEY root, const char* path, DWORD reserved, char* className, DWORD options,
    DWORD access, void* attributes, HKEY* key, DWORD* disposition) {
    Require(root == HKEY_CURRENT_USER && std::strcmp(path, "Software\\CounterStrikeExtended\\Interface") == 0
        && reserved == 0 && !className && !options && access == KEY_SET_VALUE && !attributes && !disposition,
        "write only the CSE-owned language setting");
    ++openedKeys;
    *key = reinterpret_cast<HKEY>(2);
    return ERROR_SUCCESS;
}

LONG RegSetValueExA(HKEY key, const char* name, DWORD reserved, DWORD type, const BYTE* data, DWORD bytes) {
    Require(key == reinterpret_cast<HKEY>(2) && std::strcmp(name, "Language") == 0 && !reserved && type == REG_SZ,
        "write a correctly typed language string");
    if (writeStatus != ERROR_SUCCESS) {
        return writeStatus;
    }
    record.assign(reinterpret_cast<const char*>(data), bytes);
    recordType = type;
    ++writes;
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
    queryStatus = ERROR_SUCCESS;
    Require(cse::interface_language::WriteSavedPreference("PoLiSh"), "persist canonical bundled preference");
    Require(record == std::string("polish\0", 7), "persist canonical terminated bytes");
    Require(cse::interface_language::ReadSavedPreference() == "polish", "writer and strict reader agree");
    const int previousWrites = writes;
    Require(!cse::interface_language::WriteSavedPreference("../russian"), "reject path input before registry write");
    Require(!cse::interface_language::WriteSavedPreference("custom_language"), "reject unbundled saved value");
    Require(!cse::interface_language::WriteSavedPreference(nullptr), "reject null preference");
    Require(writes == previousWrites, "invalid writes do not touch registry");
    writeStatus = ERROR_FILE_NOT_FOUND;
    Require(!cse::interface_language::WriteSavedPreference("german"), "propagate native write failure");
    Require(record == std::string("polish\0", 7), "failed writer leaves controlled record unchanged");
    Require(openedKeys == closedKeys, "close successful read and write key handles");
    std::puts("PASS: actual Windows preference reader/writer against controlled registry API");
}
