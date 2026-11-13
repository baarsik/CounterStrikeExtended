#ifndef CSE_INTERFACE_FIXTURE_WINDOWS_H
#define CSE_INTERFACE_FIXTURE_WINDOWS_H

#include <cstdint>

using BYTE = unsigned char;
using DWORD = std::uint32_t;
using LONG = std::int32_t;
using HKEY = void*;
#define HKEY_CURRENT_USER reinterpret_cast<HKEY>(1)
constexpr LONG ERROR_SUCCESS = 0;
constexpr LONG ERROR_FILE_NOT_FOUND = 2;
constexpr LONG ERROR_MORE_DATA = 234;
constexpr DWORD KEY_QUERY_VALUE = 1;
constexpr DWORD REG_SZ = 1;
constexpr DWORD REG_BINARY = 3;
LONG RegOpenKeyExA(HKEY root, const char* path, DWORD options, DWORD access, HKEY* key);
LONG RegQueryValueExA(HKEY key, const char* name, DWORD* reserved, DWORD* type, BYTE* data, DWORD* bytes);
LONG RegCloseKey(HKEY key);

#endif
