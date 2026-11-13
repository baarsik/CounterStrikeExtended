#ifndef CSE_COMMON_INTERFACE_LANGUAGE_PREFERENCE_H
#define CSE_COMMON_INTERFACE_LANGUAGE_PREFERENCE_H

#include <string>

namespace cse {
namespace interface_language {

// Read the CSE-owned native setting before game search paths or VGUI exist.
// Missing, corrupt and unbundled identifiers return the automatic choice.
std::string ReadSavedPreference();

// Empty selects automatic; only bundled identifiers can be persisted.
bool WriteSavedPreference(const char* preference);

} // namespace interface_language
} // namespace cse

#endif
