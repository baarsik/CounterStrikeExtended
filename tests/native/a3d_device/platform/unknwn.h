#pragma once

// macOS component declaration adapter only; Windows uses the real SDK header.
#include <cstdint>
using DWORD = std::uint32_t;
using ULONG = std::uint32_t;
using LONG = std::int32_t;
using HRESULT = std::int32_t;
using WORD = std::uint16_t;
using BYTE = std::uint8_t;
using BOOL = std::int32_t;
struct GUID {
    std::uint32_t Data1;
    std::uint16_t Data2;
    std::uint16_t Data3;
    std::uint8_t Data4[8];
};
using IID = GUID;
using LPGUID = GUID*;
using REFIID = const GUID&;
#define STDMETHODCALLTYPE
constexpr HRESULT S_OK = 0;
constexpr HRESULT E_NOINTERFACE = -2147467262;
constexpr HRESULT E_POINTER = -2147467261;
constexpr HRESULT E_FAIL = -2147467259;
constexpr HRESULT E_NOTIMPL = -2147467263;
constexpr BOOL FALSE = 0;
constexpr BOOL TRUE = 1;
#define FAILED(value) ((value) < 0)
#define SUCCEEDED(value) ((value) >= 0)
class IUnknown {
public:
    virtual HRESULT QueryInterface(REFIID, void**) = 0;
    virtual ULONG AddRef() = 0;
    virtual ULONG Release() = 0;
};
