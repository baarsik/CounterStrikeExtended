#pragma once

// Include the actual IFileSystem declaration used by this translation unit first.
// Hash normalized ABI shapes: Windows long and int share a shape, while LP64 long
// deliberately differs. No STL objects or allocator ownership cross this interface.
#include "cse_module_abi.h"

#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace cse { namespace filesystem_abi {
using cse::abi::LayoutHash;

// These families describe the owned provider and its consumed VFileSystem009 prefix.
constexpr std::uint32_t ProviderRole = 0x46535052;
constexpr std::uint32_t Public009 = 0x46533039;
constexpr std::uint32_t CppProfile = 0x46534350;
constexpr std::size_t PrefixMethods = 54;
static_assert(sizeof(int) == 4, "VFileSystem009 timestamps and counters use signed 32-bit int");
static_assert(std::has_virtual_destructor<IFileSystem>::value, "The public prefix includes a virtual destructor");
constexpr std::uint64_t OwnershipRevision = 1;
constexpr std::uint64_t ContractRevision = 1;
constexpr std::uint64_t BorrowedProviderLifetime = 1;

enum ShapeTag : std::uint64_t {
    VoidShape = 0,
    IntegerShape = 1,
    FloatingPointShape = 2,
    BooleanShape = 3,
    EnumerationShape = 4,
    ConstShape = 5,
    PointerShape = 6,
    FreeFunctionShape = 7,
    MemberFunctionShape = 8
};

#if defined(_MSC_VER) && defined(_M_IX86)
using CdeclFactory = void* (__cdecl *)(const char*, int*);
using ThiscallMount = void (__thiscall IFileSystem::*)();
using CdeclPrint = int (__cdecl IFileSystem::*)(FileHandle_t, char*, ...);
static_assert(std::is_same<CreateInterfaceFn, CdeclFactory>::value, "The filesystem factory uses cdecl");
static_assert(std::is_same<decltype(&IFileSystem::Mount), ThiscallMount>::value,
    "Ordinary Win32 filesystem virtual methods use thiscall");
static_assert(std::is_same<decltype(&IFileSystem::FPrintf), CdeclPrint>::value,
    "The variadic Win32 filesystem virtual method uses cdecl");
#endif

#if defined(_MSC_VER)
constexpr std::uint64_t DestructorSlots = 1;
#if defined(_M_IX86)
constexpr std::uint64_t Profile = 1; // Microsoft x86 thiscall; variadic members/factory use cdecl.
#elif defined(_M_X64)
constexpr std::uint64_t Profile = 2; // Microsoft x64 member/free/variadic call profile.
#elif defined(_M_ARM64)
constexpr std::uint64_t Profile = 3; // Windows ARM64 call profile.
#endif
#else
constexpr std::uint64_t DestructorSlots = 2;
#if defined(__APPLE__) && defined(__aarch64__)
constexpr std::uint64_t Profile = 4; // Darwin ARM64 Itanium object model and variadic call profile.
#elif defined(__x86_64__)
constexpr std::uint64_t Profile = 5; // SysV x64 Itanium object model and variadic call profile.
#elif defined(__aarch64__)
constexpr std::uint64_t Profile = 6; // AAPCS64 Itanium object model and variadic call profile.
#else
#error Unsupported filesystem compiler call profile
#endif
#endif

template<class T, class Enable = void> struct TypeShape {
    static_assert(std::is_arithmetic<T>::value || std::is_enum<T>::value, "Unclassified exchanged type");
    static constexpr std::uint64_t Kind = std::is_enum<T>::value ? EnumerationShape :
        (std::is_same<T, bool>::value ? BooleanShape :
            (std::is_floating_point<T>::value ? FloatingPointShape : IntegerShape));
    static constexpr std::uint64_t Value = LayoutHash({Kind, sizeof(T), alignof(T), std::is_signed<T>::value});
};
template<> struct TypeShape<void> {
    static constexpr std::uint64_t Value = LayoutHash({VoidShape});
};
template<class T> struct TypeShape<const T> {
    static constexpr std::uint64_t Value = LayoutHash({ConstShape, TypeShape<T>::Value});
};
template<class T> struct TypeShape<T*> {
    static constexpr std::uint64_t Value = LayoutHash({PointerShape, sizeof(T*), alignof(T*), TypeShape<T>::Value});
};
template<class R, class... A> struct TypeShape<R (*)(A...)> {
    static constexpr std::uint64_t Value = LayoutHash({FreeFunctionShape, sizeof(void*), false, sizeof...(A),
        TypeShape<R>::Value, TypeShape<A>::Value...});
};
template<class R, class... A> struct TypeShape<R (*)(A..., ...)> {
    static constexpr std::uint64_t Value = LayoutHash({FreeFunctionShape, sizeof(void*), true, sizeof...(A),
        TypeShape<R>::Value, TypeShape<A>::Value...});
};
template<class T> struct MethodShape;
template<class C, class R, class... A> struct MethodShape<R (C::*)(A...)> {
    static constexpr std::uint64_t Value = LayoutHash({MemberFunctionShape, false, sizeof...(A), TypeShape<R>::Value, TypeShape<A>::Value...});
};
template<class C, class R, class... A> struct MethodShape<R (C::*)(A..., ...)> {
    static constexpr std::uint64_t Value = LayoutHash({MemberFunctionShape, true, sizeof...(A), TypeShape<R>::Value, TypeShape<A>::Value...});
};

struct Method {
    const char* name;
    std::uint64_t ordinal;
    std::uint64_t slot;
    std::uint64_t shape;
};
#define CSE_FS_METHOD(index, name, expression) \
    {#name, index, index + DestructorSlots, MethodShape<decltype(expression)>::Value},
constexpr Method Methods[] = {
#include "cse_filesystem_methods.inc"
};
#undef CSE_FS_METHOD
static_assert(sizeof(Methods) / sizeof(Methods[0]) == PrefixMethods, "The consumed interface prefix is bounded");

constexpr std::uint64_t ContractSignature() {
    std::uint64_t value = LayoutHash({ContractRevision, PrefixMethods, DestructorSlots, sizeof(IFileSystem), alignof(IFileSystem),
        OwnershipRevision, TypeShape<FileHandle_t>::Value, TypeShape<FileFindHandle_t>::Value,
        TypeShape<WaitForResourcesHandle_t>::Value, FILESYSTEM_SEEK_HEAD, FILESYSTEM_SEEK_CURRENT,
        FILESYSTEM_SEEK_TAIL, FILESYSTEM_WARNING_QUIET, FILESYSTEM_WARNING_REPORTUNCLOSED,
        FILESYSTEM_WARNING_REPORTUSAGE, FILESYSTEM_WARNING_REPORTALLACCESSES});
    for (const auto& method : Methods) {
        value = LayoutHash({value, method.ordinal, method.slot, method.shape});
    }
    return value;
}

constexpr auto Descriptor = cse::abi::Describe({
    {ProviderRole, 0, LayoutHash({ContractRevision, BorrowedProviderLifetime})}, // owned provider role; singleton borrowed until module unload
    {Public009, 0, ContractSignature()},
    {CppProfile, 0, LayoutHash({ContractRevision, Profile, DestructorSlots, sizeof(CreateInterfaceFn),
        alignof(CreateInterfaceFn), TypeShape<CreateInterfaceFn>::Value})}
});
} } // namespace cse::filesystem_abi
