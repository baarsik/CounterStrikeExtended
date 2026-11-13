#pragma once

#if !defined(_WIN32)
#include <stdio.h>
#include <strings.h>
#include <string.h>

#if defined(__cplusplus)
// archtypes.h is also included inside legacy extern "C" blocks.
extern "C++" {
#include "../common/legacy_text_to_integer.h"
#include "../common/legacy_wide_format.h"
}
#define _atoi64 CseLegacyAtoi64
#define _wtoi CseLegacyWtoi
#define _snwprintf CseLegacySnwprintf
#define _vsnwprintf CseLegacyVsnwprintf
#endif

// Preserve the SDK's case-insensitive C-string entry points on POSIX systems.
#define stricmp strcasecmp
#define strnicmp strncasecmp
#define _strdup strdup
#define _stricmp strcasecmp
#define _strnicmp strncasecmp
#define _snprintf snprintf
#define _vsnprintf vsnprintf
#endif
