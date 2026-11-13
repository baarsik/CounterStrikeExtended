#include "interface_language_preference.h"
#include "interface_language.h"
#include "mac_settings_store.h"
#include <CoreFoundation/CoreFoundation.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

namespace {

void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", message);
        std::exit(1);
    }
}

void SetString(const char* value, const char* expected) {
    Require(cse::mac_settings::WriteString(cse::interface_language::PreferenceKey, value),
        "write disposable preference");
    Require(cse::interface_language::ReadSavedPreference() == expected, "read and validate saved preference");
}

void ClearDomain() {
    CFStringRef domain = CFStringCreateWithCString(nullptr, cse::mac_settings::ProductDomain, kCFStringEncodingUTF8);
    Require(domain != nullptr, "create owned fixture domain");
    CFArrayRef keys = CFPreferencesCopyKeyList(domain, kCFPreferencesCurrentUser, kCFPreferencesAnyHost);
    if (keys) {
        for (CFIndex index = 0; index < CFArrayGetCount(keys); ++index) {
            CFPreferencesSetValue(static_cast<CFStringRef>(CFArrayGetValueAtIndex(keys, index)), nullptr,
                domain, kCFPreferencesCurrentUser, kCFPreferencesAnyHost);
        }
        CFRelease(keys);
    }
    Require(CFPreferencesSynchronize(domain, kCFPreferencesCurrentUser, kCFPreferencesAnyHost),
        "synchronize cleared fixture domain");
    CFRelease(domain);
}

} // namespace

int main(int argc, char** argv) {
    Require(std::strstr(cse::mac_settings::ProductDomain, "org.counterstrikeextended.fixture.interface.") ==
        cse::mac_settings::ProductDomain, "fixture cannot touch production settings domain");
    if (argc == 2 && std::strcmp(argv[1], "cleanup") == 0) {
        ClearDomain();
        Require(cse::interface_language::ReadSavedPreference().empty(), "cleanup removes saved preference");
        return 0;
    }
    Require(cse::interface_language::ReadSavedPreference().empty(), "missing preference is automatic");
    SetString("russian", "russian");
    SetString("PoLiSh", "polish");
    SetString("", "");
    SetString("german", "german");
    SetString("../russian", "");
    SetString("ru ssian", "");
    SetString("custom_language", "");
    SetString(std::string(128, 'a').c_str(), "");
    constexpr char EmbeddedNul[] = "russian\0polish";
    Require(cse::mac_settings::WriteBytes(cse::interface_language::PreferenceKey, EmbeddedNul,
        sizeof(EmbeddedNul)), "write corrupt embedded-NUL fixture");
    Require(cse::interface_language::ReadSavedPreference().empty(), "reject hidden bytes after terminator");
    constexpr char MissingNul[] = {'p', 'o', 'l', 'i', 's', 'h'};
    Require(cse::mac_settings::WriteBytes(cse::interface_language::PreferenceKey, MissingNul,
        sizeof(MissingNul)), "write nonterminated fixture");
    Require(cse::interface_language::ReadSavedPreference().empty(), "reject missing terminator");
    std::puts("PASS: native macOS startup preference validation in isolated fixture domain");
}
