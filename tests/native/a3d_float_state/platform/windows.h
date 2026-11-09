#pragma once

// Basic LLP64 declarations for the macOS mixer-body component, not a Win32 API.
#include <cstdint>
using BYTE = std::uint8_t;
using WORD = std::uint16_t;
using DWORD = std::uint32_t;
using LONG = std::int32_t;
using SHORT = std::int16_t;
using BOOL = std::int32_t;
#define __cdecl
#define __stdcall
#define __forceinline inline __attribute__((always_inline))
#define __debugbreak() __builtin_trap()
constexpr BOOL FALSE = 0;
constexpr BOOL TRUE = 1;
