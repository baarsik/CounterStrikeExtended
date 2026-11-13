#include <string.h>
#include "../../../src/public/cse_posix_crt.h"

// The runtime forces this compatibility header into C sources as well as C++.
int CseCompileLegacyCTextBoundary(const char* left, const char* right) {
    return _stricmp(left, right);
}
