#ifndef CSE_LEGACY_WIDE_FORMAT_H
#define CSE_LEGACY_WIDE_FORMAT_H

#include <cstdarg>
#include <cstddef>

// Native text is stored as UTF16 cells even when sizeof(wchar_t) == 4.
// C numeric punctuation is independent of the selected narrow-byte decoder.
enum class CseLegacyWideNarrowEncoding {
    StrictUtf8,
    CByteIdentity
};

struct CseLegacyWideFormatContext {
    CseLegacyWideNarrowEncoding narrow = CseLegacyWideNarrowEncoding::StrictUtf8;
};

// Legacy bounded output: exact fit has no NUL; truncation returns -1 without
// reserving a NUL. A null destination is a length query only when capacity == 0.
// The formatter retains no input/argument memory after return. It uses fixed
// numeric storage and two-pass string measurement/emission, with no dynamic
// allocation, propagated exceptions or locale mutation. These functions model
// _snwprintf/_vsnwprintf, not the different standard swprintf truncation contract.
int CseLegacyVsnwprintfWithContext(CseLegacyWideFormatContext context, wchar_t* destination,
    std::size_t capacity, const wchar_t* format, va_list arguments) noexcept;

int CseLegacySnwprintfWithContext(CseLegacyWideFormatContext context, wchar_t* destination,
    std::size_t capacity, const wchar_t* format, ...) noexcept;

int CseLegacyVsnwprintf(wchar_t* destination, std::size_t capacity, const wchar_t* format,
    va_list arguments) noexcept;

int CseLegacySnwprintf(wchar_t* destination, std::size_t capacity, const wchar_t* format, ...) noexcept;

#endif
