// Preload the production dependencies, then expose SDK-style function macros to
// the complete implementation. This tests the macro contract on the real host
// platform; it does not emulate Windows APIs or define a Windows platform macro.
#include "ServerBrowserConfig.h"
#include "FileSystem.h"
#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_stdinc.h>
#include <algorithm>
#include <charconv>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <map>
#include <limits>
#include <mutex>
#include <set>
#include <thread>
#if defined(_WIN32)
#include <Windows.h>
#else
#include <unistd.h>
#endif

#undef min
#undef max
#define min(a, b) (((a) < (b)) ? (a) : (b))
#define max(a, b) (((a) > (b)) ? (a) : (b))
#include "ServerBrowserConfig.cpp"
#undef min
#undef max
