#pragma once

#include <stddef.h>

#define CSE_SAVE_VECTOR_COMPONENTS 3

#ifdef __cplusplus
// Legacy game headers define min/max before including shared save interfaces.
#pragma push_macro("min")
#pragma push_macro("max")
#undef min
#undef max
#include <type_traits>
#pragma pop_macro("max")
#pragma pop_macro("min")

namespace cse { namespace save {

enum NativeStorageFlags : unsigned int {
    NativeSigned = 1u << 0,
    NativeBoolean = 1u << 1
};

namespace detail {

template<typename Type, bool IsEnum = std::is_enum<Type>::value>
struct NativeScalarProperties {
    using Value = typename std::remove_cv<Type>::type;
    static constexpr unsigned int Flags = std::is_same<Value, bool>::value ? NativeBoolean
        : ((std::is_integral<Value>::value && std::is_signed<Value>::value) ? NativeSigned : 0u);
};

template<typename Type>
struct NativeScalarProperties<Type, true>
    : NativeScalarProperties<typename std::underlying_type<typename std::remove_cv<Type>::type>::type> {};

} // namespace detail

// These facts describe native storage only. They never select a disk width.
// Decltype keeps field access unevaluated when used by the save metadata macros.
template<typename Type>
struct NativeStorageTraits {
    static constexpr size_t Stride = sizeof(Type);
    static constexpr unsigned int Flags = detail::NativeScalarProperties<Type>::Flags;
};

template<typename Type, size_t Count>
struct NativeStorageTraits<Type[Count]> {
    static constexpr size_t Stride = sizeof(Type);
    static constexpr unsigned int Flags = detail::NativeScalarProperties<Type>::Flags;
};

} } // namespace cse::save
#endif
