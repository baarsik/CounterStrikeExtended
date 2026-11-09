#include "cse_posix_crt.h"
#ifdef _snwprintf
#error C code must not receive the C++ wide formatter alias
#endif
#ifdef _vsnwprintf
#error C code must not receive the C++ variadic formatter alias
#endif
int c_prefix_probe(void) {
    return stricmp("a", "A");
}
