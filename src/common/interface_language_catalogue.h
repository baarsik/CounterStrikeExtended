#ifndef CSE_COMMON_INTERFACE_LANGUAGE_CATALOGUE_H
#define CSE_COMMON_INTERFACE_LANGUAGE_CATALOGUE_H

#include <string>

namespace cse {
namespace interface_language {

struct BundledLanguage {
    const char* id;
    const wchar_t* nativeName;
    const char* englishName;
};

constexpr BundledLanguage BundledLanguages[] = {
#include "InterfaceLanguages.generated.inc"
};

inline bool IsBundled(const std::string& identifier) {
    for (const BundledLanguage& language : BundledLanguages) {
        if (identifier == language.id) {
            return true;
        }
    }
    return false;
}

} // namespace interface_language
} // namespace cse

#endif
