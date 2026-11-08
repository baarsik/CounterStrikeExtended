#pragma once

#if !defined(_WIN32)
#include <stdio.h>
#include <strings.h>

// Preserve the SDK's case-insensitive C-string entry points on POSIX systems.
#define stricmp strcasecmp
#define strnicmp strncasecmp
#define _stricmp strcasecmp
#define _strnicmp strncasecmp
#define _snprintf snprintf
#define _vsnprintf vsnprintf
#endif
