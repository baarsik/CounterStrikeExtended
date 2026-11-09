#pragma once

#define _USE_MATH_DEFINES
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include <string>
#include <type_traits>
#if defined(_WIN32)
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
using DWORD = std::uint32_t;
using LONG = std::int32_t;
using ULONG = std::uint32_t;
using INT = std::int32_t;
using BOOL = std::int32_t;
using BYTE = unsigned char;
using HRESULT = std::int32_t;
#define STDMETHODCALLTYPE
#define __cdecl
#ifndef TRUE
constexpr BOOL TRUE = 1;
constexpr BOOL FALSE = 0;
#endif
constexpr HRESULT S_OK = 0;
constexpr HRESULT E_FAIL = -1;
constexpr HRESULT E_INVALIDARG = -2;
#endif
// Error values are not exercised by the selected valid Begin modes.
constexpr HRESULT A3DERROR_INVALID_BEGIN_MODE = -3;
using A3DVAL = float;
using LPA3DVAL = float*;
using A3DVERTEX = float[4];
using A3DVECTOR = float[4];
