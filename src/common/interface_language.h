#ifndef CSE_COMMON_INTERFACE_LANGUAGE_H
#define CSE_COMMON_INTERFACE_LANGUAGE_H

#include "interface_language_catalogue.h"
#include "interface_language_preference.h"

#include <cstddef>
#include <string>

namespace cse {
namespace interface_language {

// BaseUISurface retains a 32-byte language field, including its terminator.
constexpr std::size_t MaximumIdentifierBytes = 31;
constexpr std::size_t LanguageBufferBytes = MaximumIdentifierBytes + 1;
constexpr int CommandLineBufferBytes = 128;
constexpr int PreferenceBufferBytes = 128;
constexpr char PreferenceKey[] = "HKEY_CURRENT_USER\\Software\\CounterStrikeExtended\\Interface\\Language";

inline std::string Normalize(const char* value) {
    if (!value) {
        return {};
    }
    std::string result;
    for (std::size_t index = 0; index < LanguageBufferBytes; ++index) {
        const unsigned char character = static_cast<unsigned char>(value[index]);
        if (!character) {
            return result;
        }
        if (index == MaximumIdentifierBytes) {
            return {};
        }
        const bool uppercase = character >= 'A' && character <= 'Z';
        const bool lowercase = character >= 'a' && character <= 'z';
        const bool digit = character >= '0' && character <= '9';
        if (!uppercase && !lowercase && !digit && character != '_') {
            return {};
        }
        constexpr unsigned char CaseOffset = 'a' - 'A';
        result.push_back(static_cast<char>(uppercase ? character + CaseOffset : character));
    }
    return {};
}

inline std::string Resolve(const char* current, const char* requested, const char* saved = nullptr) {
    const std::string overrideLanguage = Normalize(requested);
    if (!overrideLanguage.empty()) {
        return overrideLanguage;
    }
    const std::string savedLanguage = Normalize(saved);
    if (IsBundled(savedLanguage)) {
        return savedLanguage;
    }
    const std::string currentLanguage = Normalize(current);
    return currentLanguage.empty() ? "english" : currentLanguage;
}

// Dedicated GUI owners already have an initialized ISystem command-line service.
// Native preference validation must agree with early filesystem startup; ISystem's
// string reader does not retain registry record type or length.
// Do not initialize another parser or alter Steam/registry preferences.
template<class System>
inline std::string ResolveFromSystem(const char* current, System* system) {
    char requested[CommandLineBufferBytes]{};
    if (system) {
        if (!system->GetCommandLineParamValue("-language", requested, sizeof(requested))) {
            requested[0] = '\0';
        }
    }
    requested[sizeof(requested) - 1] = '\0';
    const std::string saved = ReadSavedPreference();
    return Resolve(current, requested, saved.c_str());
}

} // namespace interface_language
} // namespace cse

#endif
