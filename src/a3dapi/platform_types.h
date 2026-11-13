#pragma once

#ifdef _WIN32
#include <windows.h>
#include <mmsystem.h>
#include <mmreg.h>
#else
#include <cstdint>
#include <cstring>

// Scalar widths follow the Windows SDK ABI, including on an LP64 host.
using CHAR = char;
using LONGLONG = std::int64_t;
using ULONGLONG = std::uint64_t;
using BYTE = std::uint8_t;
using WORD = std::uint16_t;
using SHORT = std::int16_t;
using DWORD = std::uint32_t;
using LONG = std::int32_t;
using ULONG = std::uint32_t;
using INT = std::int32_t;
using UINT = std::uint32_t;
using BOOL = std::int32_t;
using FLOAT = float;
using HRESULT = std::int32_t;
using LPVOID = void*;
using LPCVOID = const void*;
using LPSTR = char*;
using LPCSTR = const char*;
using LPBYTE = BYTE*;
using LPWORD = WORD*;
using LPDWORD = DWORD*;
using LPLONG = LONG*;
using HANDLE = void*;
using HGLOBAL = HANDLE;
struct HWND__;
using HWND = HWND__*;

#define STDMETHODCALLTYPE
#define STDAPICALLTYPE
#define WINAPI
#define CALLBACK
#define __int64 long long
#define __forceinline inline __attribute__((always_inline))
#define __stdcall
#define __cdecl
#define FAR
#define TRUE 1
#define FALSE 0

struct GUID {
    std::uint32_t Data1;
    std::uint16_t Data2;
    std::uint16_t Data3;
    std::uint8_t Data4[8];
};
using IID = GUID;
using CLSID = GUID;
using LPGUID = GUID*;
using LPCGUID = const GUID*;
using REFGUID = const GUID&;
using REFIID = const IID&;
using REFCLSID = const CLSID&;

inline bool IsEqualGUID(REFGUID left, REFGUID right) {
    return std::memcmp(&left, &right, sizeof(GUID)) == 0;
}
inline bool operator==(REFGUID left, REFGUID right) {
    return IsEqualGUID(left, right);
}
inline bool operator!=(REFGUID left, REFGUID right) {
    return !IsEqualGUID(left, right);
}
#define IsEqualIID IsEqualGUID
#define IsEqualCLSID IsEqualGUID
#define DEFINE_GUID(name, a, b, c, d, e, f, g, h, i, j, k) \
    inline constexpr GUID name = {a, b, c, {d, e, f, g, h, i, j, k}}
inline constexpr GUID GUID_NULL = {};

constexpr DWORD FACILITY_ITF = 4;
constexpr HRESULT S_OK = 0;
constexpr HRESULT S_FALSE = 1;
constexpr HRESULT E_NOTIMPL = static_cast<HRESULT>(0x80004001u);
constexpr HRESULT E_NOINTERFACE = static_cast<HRESULT>(0x80004002u);
constexpr HRESULT E_POINTER = static_cast<HRESULT>(0x80004003u);
constexpr HRESULT E_ABORT = static_cast<HRESULT>(0x80004004u);
constexpr HRESULT E_ACCESSDENIED = static_cast<HRESULT>(0x80070005u);
constexpr HRESULT E_FAIL = static_cast<HRESULT>(0x80004005u);
constexpr HRESULT E_OUTOFMEMORY = static_cast<HRESULT>(0x8007000eu);
constexpr HRESULT E_INVALIDARG = static_cast<HRESULT>(0x80070057u);
constexpr HRESULT CLASS_E_NOAGGREGATION = static_cast<HRESULT>(0x80040110u);
constexpr HRESULT CLASS_E_CLASSNOTAVAILABLE = static_cast<HRESULT>(0x80040111u);
#define SUCCEEDED(result) (static_cast<HRESULT>(result) >= 0)
#define FAILED(result) (static_cast<HRESULT>(result) < 0)
#define MAKE_HRESULT(severity, facility, code) \
    static_cast<HRESULT>((static_cast<DWORD>(severity) << 31) | \
        (static_cast<DWORD>(facility) << 16) | static_cast<DWORD>(code))

// Multimedia wave records use the SDK's local one-byte packing. Other records
// retain their natural ABI alignment; this is deliberately not a global pack.
#pragma pack(push, 1)
struct WAVEFORMAT {
    WORD wFormatTag;
    WORD nChannels;
    DWORD nSamplesPerSec;
    DWORD nAvgBytesPerSec;
    WORD nBlockAlign;
};
struct PCMWAVEFORMAT {
    WAVEFORMAT wf;
    WORD wBitsPerSample;
};
struct WAVEFORMATEX {
    WORD wFormatTag;
    WORD nChannels;
    DWORD nSamplesPerSec;
    DWORD nAvgBytesPerSec;
    WORD nBlockAlign;
    WORD wBitsPerSample;
    WORD cbSize;
};
struct WAVEFORMATEXTENSIBLE {
    WAVEFORMATEX Format;
    union {
        WORD wValidBitsPerSample;
        WORD wSamplesPerBlock;
        WORD wReserved;
    } Samples;
    DWORD dwChannelMask;
    GUID SubFormat;
};
#pragma pack(pop)
using LPWAVEFORMAT = WAVEFORMAT*;
using LPPCMWAVEFORMAT = PCMWAVEFORMAT*;
using LPWAVEFORMATEX = WAVEFORMATEX*;
using LPCWAVEFORMATEX = const WAVEFORMATEX*;
constexpr WORD WAVE_FORMAT_PCM = 1;
#endif
