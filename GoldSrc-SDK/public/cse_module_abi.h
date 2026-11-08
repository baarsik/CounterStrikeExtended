#pragma once

#include <climits>
#include <cstddef>
#include <cstdint>
#include <initializer_list>

#if defined(_WIN32)
#define CSE_ABI_CALL __cdecl
#else
#define CSE_ABI_CALL
#endif

namespace cse::abi {

constexpr std::uint32_t DescriptorVersion = 1;
constexpr std::uint32_t RuntimeVersion = 4;
// Version 1 bounds each module's exchanged tables without following pointers.
constexpr std::size_t MaxTables = 16;
constexpr std::uint32_t NativePointerBits = sizeof(void *) * CHAR_BIT;

enum class Architecture : std::uint32_t { X86 = 1, X64 = 2, Arm64 = 3 };
enum class CompilerAbi : std::uint32_t { Microsoft = 1, Itanium = 2 };

#if defined(_M_ARM64EC)
#error ARM64EC is not a native CounterStrikeExtended module architecture
#elif defined(_M_ARM64) || defined(__aarch64__)
constexpr auto NativeArchitecture = static_cast<std::uint32_t>(Architecture::Arm64);
#elif defined(_M_X64) || defined(__x86_64__)
constexpr auto NativeArchitecture = static_cast<std::uint32_t>(Architecture::X64);
#elif defined(_M_IX86) || defined(__i386__)
constexpr auto NativeArchitecture = static_cast<std::uint32_t>(Architecture::X86);
#else
#error Unsupported CounterStrikeExtended module architecture
#endif

#if defined(_MSC_VER)
constexpr auto NativeCompilerAbi = static_cast<std::uint32_t>(CompilerAbi::Microsoft); // Includes clang-cl.
#elif defined(__clang__) || defined(__GNUC__)
constexpr auto NativeCompilerAbi = static_cast<std::uint32_t>(CompilerAbi::Itanium);
#else
#error Unsupported CounterStrikeExtended compiler ABI
#endif

struct alignas(8) TableLayout {
    std::uint32_t identifier;
    std::uint32_t reserved;
    std::uint64_t signature;
};

struct alignas(8) Descriptor {
    std::uint32_t descriptorBytes;
    std::uint32_t descriptorVersion;
    std::uint32_t runtimeVersion;
    std::uint32_t architecture;
    std::uint32_t pointerBits;
    std::uint32_t compilerAbi;
    std::uint32_t tableCount;
    std::uint32_t reserved;
    TableLayout tables[MaxTables];
};

static_assert(CHAR_BIT == 8);
static_assert(sizeof(TableLayout) == 16);
static_assert(offsetof(Descriptor, tables) == 32);
static_assert(sizeof(Descriptor) == 288);

using GetModuleAbi = const Descriptor *(CSE_ABI_CALL *)();

enum class Compatibility {
    Compatible,
    MissingDescriptor,
    DescriptorSize,
    DescriptorVersion,
    RuntimeVersion,
    Architecture,
    PointerWidth,
    CompilerAbi,
    TableCount,
    TableLayout,
    InvalidDescriptor
};

// Hash explicitly supplied layout facts in little-endian order, independent of
// the hash implementation's host widths. Table owners must include every field
// offset/width/alignment and change RuntimeVersion for semantic contract changes.
constexpr std::uint64_t LayoutHash(std::initializer_list<std::uint64_t> facts) {
    constexpr std::uint64_t fnvOffsetBasis = 14695981039346656037ull;
    constexpr std::uint64_t fnvPrime = 1099511628211ull;
    std::uint64_t hash = fnvOffsetBasis;
    for (std::uint64_t value : facts) {
        for (std::size_t byte = 0; byte < sizeof(value); ++byte) {
            hash ^= value & 0xffu;
            hash *= fnvPrime;
            value >>= CHAR_BIT;
        }
    }
    return hash;
}

constexpr Descriptor Describe(std::initializer_list<TableLayout> tables) {
    Descriptor descriptor{sizeof(Descriptor), DescriptorVersion, RuntimeVersion, NativeArchitecture,
                          NativePointerBits, NativeCompilerAbi, static_cast<std::uint32_t>(tables.size()), 0, {}};
    std::size_t index = 0;
    for (const TableLayout &table : tables) {
        if (index == MaxTables) {
            break; // Check rejects an oversized declaration before using tables.
        }
        descriptor.tables[index++] = table;
    }
    return descriptor;
}

constexpr Compatibility Check(const Descriptor &expected, const Descriptor *actual) {
    if (!actual) {
        return Compatibility::MissingDescriptor;
    }
    if (actual->descriptorBytes != sizeof(Descriptor)) {
        return Compatibility::DescriptorSize;
    }
    if (actual->descriptorVersion != DescriptorVersion) {
        return Compatibility::DescriptorVersion;
    }
    if (actual->runtimeVersion != expected.runtimeVersion) {
        return Compatibility::RuntimeVersion;
    }
    if (actual->architecture != expected.architecture) {
        return Compatibility::Architecture;
    }
    if (actual->pointerBits != expected.pointerBits) {
        return Compatibility::PointerWidth;
    }
    if (actual->compilerAbi != expected.compilerAbi) {
        return Compatibility::CompilerAbi;
    }
    if (expected.tableCount == 0 || expected.tableCount > MaxTables || actual->tableCount != expected.tableCount) {
        return Compatibility::TableCount;
    }
    if (actual->reserved != 0) {
        return Compatibility::InvalidDescriptor;
    }
    for (std::size_t index = 0; index < expected.tableCount; ++index) {
        const TableLayout &wanted = expected.tables[index];
        const TableLayout &offered = actual->tables[index];
        if (wanted.identifier == 0 || offered.reserved != 0) {
            return Compatibility::InvalidDescriptor;
        }
        if (offered.identifier != wanted.identifier || offered.signature != wanted.signature) {
            return Compatibility::TableLayout;
        }
        for (std::size_t earlier = 0; earlier < index; ++earlier) {
            if (offered.identifier == actual->tables[earlier].identifier) {
                return Compatibility::InvalidDescriptor;
            }
        }
    }
    return Compatibility::Compatible;
}

} // namespace cse::abi
