#pragma once

#include "cse_module_abi.h"
#include <type_traits>

namespace cse { namespace abi {

// These IDs describe this owned boundary; other DLL roles have not been migrated yet.
enum class ClientRole : std::uint32_t { Engine = 1, Client = 2 };
enum class ClientContract : std::uint32_t {
    Role = 0x1000,
    CProfile = 0x1001,
    EngineFunctions = 0x1100,
    ClientFunctions = 0x1101,
    NestedFunctions = 0x1102,
    ExchangeFunctions = 0x1103,
    ClientRecords = 0x1104,
    MovementRecords = 0x1105,
    ModelRecords = 0x1106
};

constexpr std::uint64_t ClientTextHash(const char* text) {
    constexpr std::uint64_t OffsetBasis = UINT64_C(14695981039346656037);
    constexpr std::uint64_t Prime = UINT64_C(1099511628211);
    std::uint64_t hash = OffsetBasis;
    while (*text) {
        hash ^= static_cast<unsigned char>(*text++);
        hash *= Prime;
    }
    return hash;
}

// Individual-pointer views do not use the complete owner's alignment or stride.
// An unexposed suffix may increase either without changing the shared prefix.
constexpr std::size_t ClientRequiredAlignment(std::initializer_list<std::size_t> alignments) {
    std::size_t required = alignof(unsigned char);
    for (std::size_t alignment : alignments) {
        if (alignment > required) {
            required = alignment;
        }
    }
    return required;
}

// Compiler-resolved type spelling includes return/argument types and calling conventions,
// not argument names or the size/address of a function pointer. The C profile pins its producer.
template<typename Type>
constexpr std::uint64_t ClientFunctionTypeHash() {
#if defined(_MSC_VER)
    return ClientTextHash(__FUNCSIG__);
#elif defined(__clang__) || defined(__GNUC__)
    return ClientTextHash(__PRETTY_FUNCTION__);
#else
#error A canonical function-type spelling is required for the client ABI
#endif
}

// This C boundary exchanges no STL containers, RTTI objects or CRT allocation ownership.
// Buffers remain with their owner or are released using the owner's explicit callbacks.
// Do not reject retained /MT versus /MD or iterator/EH/RTTI policies unrelated to these types.
constexpr std::uint64_t ClientCProfile() {
#if defined(__clang__)
    constexpr std::uint64_t Producer = 1;
    constexpr std::uint64_t Toolset = __clang_major__ * 10000 + __clang_minor__ * 100 + __clang_patchlevel__;
#elif defined(_MSC_VER)
    constexpr std::uint64_t Producer = 2;
    constexpr std::uint64_t Toolset = _MSC_FULL_VER;
#else
    constexpr std::uint64_t Producer = 3;
    constexpr std::uint64_t Toolset = __GNUC__ * 10000 + __GNUC_MINOR__ * 100 + __GNUC_PATCHLEVEL__;
#endif
    constexpr std::uint64_t TypeSpellingVersion = 1;
    return LayoutHash({TypeSpellingVersion, Producer, NativeCompilerAbi, Toolset, sizeof(void*), alignof(void*),
        sizeof(int), alignof(int), sizeof(short), alignof(short), sizeof(float), alignof(float),
        sizeof(double), alignof(double), sizeof(std::size_t), alignof(std::size_t), CHAR_BIT});
}

template<typename Type, bool Function = std::is_pointer<Type>::value
    && std::is_function<typename std::remove_pointer<Type>::type>::value>
struct ClientMemberType {
    static constexpr std::uint64_t Hash() {
        // Nested records and arrays have their own complete field/layout facts.
        return std::is_arithmetic<Type>::value || std::is_enum<Type>::value
            ? ClientFunctionTypeHash<Type>() : 0;
    }
};

template<typename Type>
struct ClientMemberType<Type, true> {
    static constexpr std::uint64_t Hash() {
        return ClientFunctionTypeHash<Type>();
    }
};

template<typename Type, std::size_t Count>
struct ClientMemberType<Type[Count], false> {
    static constexpr std::uint64_t Hash() {
        return LayoutHash({Count, ClientMemberType<Type>::Hash()});
    }
};

} }
