#ifndef CSE_COMMON_MAC_SETTINGS_STORE_H
#define CSE_COMMON_MAC_SETTINGS_STORE_H

#include <cstddef>
#include <cstdint>

namespace cse {
namespace mac_settings {

// The native settings domain is shared by Release and Debug, like HKCU.
constexpr char ProductDomain[] = "org.counterstrikeextended.settings";
constexpr std::size_t MaximumValueBytes = UINT32_MAX;

// Keys are NUL-terminated strict UTF8; empty and trailing-backslash names are invalid.
// A null destination queries size. An undersized read reports required bytes and
// leaves the destination untouched. Other failures report zero required bytes.
// Binary values copy opaque bytes. String values are strict UTF8 including NUL;
// reads preserve the measured ACP65001 UTF16 staging residue when capacity fits.
// String reads can therefore modify bytes beyond requiredBytes, within capacity.
// A successful write requires synchronization; a failed sync promises no rollback.
bool ReadBytes(const char* key, void* destination, std::size_t capacity,
    std::size_t* requiredBytes = nullptr) noexcept;
bool WriteBytes(const char* key, const void* source, std::size_t bytes) noexcept;
bool WriteString(const char* key, const char* value) noexcept;
bool ReadInt32(const char* key, std::int32_t& value) noexcept;
bool WriteInt32(const char* key, std::int32_t value) noexcept;

} // namespace mac_settings
} // namespace cse

#endif
