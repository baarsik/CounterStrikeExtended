#pragma once

// Include the actual declaring VGUI_App.h first. This foundation performs no
// loading, publication or callbacks. Compiler-derived physical evidence remains
// mandatory: member types cannot detect reordered fields or virtual declarations.
#include "cse_module_abi.h"

#include <limits>
#include <type_traits>

namespace cse { namespace vgui1_app_cold_abi {

// Explicit instantiation's template arguments are exempt from access checking
// ([temp.explicit]). The returned member pointer is used only in decltype; its
// value, representation and address are never inspected or called. These tags
// neither replace nor change any declaration in the actual App/Dar classes.
template<class Tag, class MemberType, MemberType Member>
struct MemberAccess {
    friend auto CseVgui1ColdMember(Tag) {
        return Member;
    }
};

} }

// Invoke only in cse::vgui1_app_cold_abi. C++14 auto return deduction preserves
// the actual private/protected member type, including array extents/qualifiers.
#define CSE_VGUI1_COLD_MEMBER(Tag, Owner, Member) \
    struct Tag { \
        friend auto CseVgui1ColdMember(Tag); \
    }; \
    template struct MemberAccess<Tag, decltype(&Owner::Member), &Owner::Member>; \
    using Tag##Type = decltype(CseVgui1ColdMember(Tag{}))

namespace cse { namespace vgui1_app_cold_abi {

constexpr std::uint32_t RoleFamily = 0x4f4d0001;
constexpr std::uint32_t CppProfileFamily = 0x4f4d0002;
constexpr std::uint32_t AppColdFamily = 0x4f4d0206;
constexpr std::uint64_t ContractRevision = 1;
constexpr std::uint64_t OwnershipRevision = 1;
constexpr bool PublisherReady = false;
constexpr bool ColdLoadReady = false;

// Stable semantic roles. Record pointers below do not assert a complete pointee
// gameplay contract; App/Dar complete storage is supplied by actual field reports.
enum class Record : std::uint64_t {
    App = 1, Panel = 2, SurfaceBase = 3, TickSignal = 4, Scheme = 5, Cursor = 6,
    SurfaceDar = 7, TickDar = 8
};

enum class Kind : std::uint64_t {
    Void = 1, Bool = 2, Char = 3, SignedChar = 4, UnsignedChar = 5,
    Int = 6, UnsignedInt = 7, Long = 8, UnsignedLong = 9,
    Record = 10, Pointer = 11, Reference = 12, Array = 13, Const = 14,
    Volatile = 15, Enum = 16, Function = 17, MemberCall = 18, Signed32Timing = 19, FreeCall = 20
};

template<class T> struct TypeShape;

template<Kind TypeKind, class T>
struct ScalarShape {
    static constexpr std::uint64_t Value = abi::LayoutHash({static_cast<std::uint64_t>(TypeKind),
        sizeof(T), alignof(T), std::is_signed<T>::value});
};

template<> struct TypeShape<void> {
    static constexpr std::uint64_t Value = abi::LayoutHash({static_cast<std::uint64_t>(Kind::Void)});
};

template<> struct TypeShape<bool> : ScalarShape<Kind::Bool, bool> {};
template<> struct TypeShape<char> : ScalarShape<Kind::Char, char> {};
template<> struct TypeShape<signed char> : ScalarShape<Kind::SignedChar, signed char> {};
template<> struct TypeShape<unsigned char> : ScalarShape<Kind::UnsignedChar, unsigned char> {};
template<> struct TypeShape<int> : ScalarShape<Kind::Int, int> {};
template<> struct TypeShape<unsigned int> : ScalarShape<Kind::UnsignedInt, unsigned int> {};
template<> struct TypeShape<long> : ScalarShape<Kind::Long, long> {};
template<> struct TypeShape<unsigned long> : ScalarShape<Kind::UnsignedLong, unsigned long> {};

template<class T> struct TypeShape<const T> {
    static constexpr std::uint64_t Value = abi::LayoutHash({static_cast<std::uint64_t>(Kind::Const),
        TypeShape<T>::Value});
};

template<class T> struct TypeShape<volatile T> {
    static constexpr std::uint64_t Value = abi::LayoutHash({static_cast<std::uint64_t>(Kind::Volatile),
        TypeShape<T>::Value});
};

template<class T> struct TypeShape<T*> {
    static constexpr std::uint64_t Value = abi::LayoutHash({static_cast<std::uint64_t>(Kind::Pointer),
        sizeof(T*), alignof(T*), TypeShape<T>::Value});
};

template<class T> struct TypeShape<T&> {
    static constexpr std::uint64_t Value = abi::LayoutHash({static_cast<std::uint64_t>(Kind::Reference),
        sizeof(T*), alignof(T*), TypeShape<T>::Value});
};

template<class T, std::size_t Count> struct TypeShape<T[Count]> {
    static constexpr std::uint64_t Value = abi::LayoutHash({static_cast<std::uint64_t>(Kind::Array),
        sizeof(T[Count]), alignof(T[Count]), Count, TypeShape<T>::Value});
};

template<Record TypeRole> struct RecordShape {
    static constexpr std::uint64_t Value = abi::LayoutHash({static_cast<std::uint64_t>(Kind::Record),
        static_cast<std::uint64_t>(TypeRole)});
};

template<> struct TypeShape<vgui::App> : RecordShape<Record::App> {};
template<> struct TypeShape<vgui::Panel> : RecordShape<Record::Panel> {};
template<> struct TypeShape<vgui::SurfaceBase> : RecordShape<Record::SurfaceBase> {};
template<> struct TypeShape<vgui::TickSignal> : RecordShape<Record::TickSignal> {};
template<> struct TypeShape<vgui::Scheme> : RecordShape<Record::Scheme> {};
template<> struct TypeShape<vgui::Cursor> : RecordShape<Record::Cursor> {};
template<> struct TypeShape<vgui::Dar<vgui::SurfaceBase*>> : RecordShape<Record::SurfaceDar> {};
template<> struct TypeShape<vgui::Dar<vgui::TickSignal*>> : RecordShape<Record::TickDar> {};

template<class T, std::uint64_t EnumRole> struct EnumShape {
    static constexpr std::uint64_t Value = abi::LayoutHash({static_cast<std::uint64_t>(Kind::Enum),
        EnumRole, sizeof(T), alignof(T), TypeShape<typename std::underlying_type<T>::type>::Value});
};

template<> struct TypeShape<vgui::KeyCode> : EnumShape<vgui::KeyCode, 1> {};
template<> struct TypeShape<vgui::MouseCode> : EnumShape<vgui::MouseCode, 2> {};
template<> struct TypeShape<vgui::Cursor::DefaultCursor> : EnumShape<vgui::Cursor::DefaultCursor, 3> {};

template<class Member> struct MemberValue;
template<class Value, class Owner> struct MemberValue<Value Owner::*> {
    using Type = Value;
};

template<class T> struct TimingFieldShape {
    static_assert(std::is_same<T, int>::value || std::is_same<T, long>::value,
        "Only the measured signed32 int/long timing field spelling may be normalized");
    static_assert(sizeof(T) == sizeof(std::int32_t) && std::is_signed<T>::value,
        "VGUI1 App timing storage must be signed32");
    static constexpr std::uint64_t Value = abi::LayoutHash({static_cast<std::uint64_t>(Kind::Signed32Timing),
        sizeof(T), alignof(T), std::numeric_limits<T>::digits});
};

template<class Result, class... Arguments>
constexpr std::uint64_t FunctionShape() {
    std::uint64_t signature = abi::LayoutHash({static_cast<std::uint64_t>(Kind::Function),
        TypeShape<Result>::Value, sizeof...(Arguments)});
    for (std::uint64_t argument : std::initializer_list<std::uint64_t>{TypeShape<Arguments>::Value...}) {
        signature = abi::LayoutHash({signature, argument});
    }
    return signature;
}

template<class Function> struct FreeCallShape;
template<class Result, class... Arguments>
struct FreeCallShape<Result (*)(Arguments...)> {
    static constexpr std::uint64_t Value = abi::LayoutHash({static_cast<std::uint64_t>(Kind::FreeCall),
        FunctionShape<Result, Arguments...>(), sizeof(Result (*)(Arguments...)),
        alignof(Result (*)(Arguments...))});
};

template<class Member> struct MethodShape;
template<class Result, class Owner, class... Arguments>
struct MethodShape<Result (Owner::*)(Arguments...)> {
    static constexpr std::uint64_t Value = abi::LayoutHash({static_cast<std::uint64_t>(Kind::MemberCall),
        TypeShape<Owner>::Value, FunctionShape<Result, Arguments...>(),
        sizeof(Result (Owner::*)(Arguments...)), alignof(Result (Owner::*)(Arguments...))});
};

// The static complete object is engine-owned; the provider constructs its App
// base, allocates nested resources and retains a borrowed App::_instance pointer.
// The provider must outlive every object/vtable/resource use. No destructor here
// frees those resources or makes the borrowed pointer safe after engine unload.
constexpr std::uint64_t OwnershipSignature() {
    constexpr std::uint64_t EngineStaticStorage = 1;
    constexpr std::uint64_t ProviderBaseConstruction = 2;
    constexpr std::uint64_t ProviderAllocatesNestedResources = 3;
    constexpr std::uint64_t ProviderRetainsBorrowedInstance = 4;
    constexpr std::uint64_t ProviderRetainedThroughObjectUse = 5;
    constexpr std::uint64_t TrivialDestructionDoesNotRelease = 6;
    return abi::LayoutHash({OwnershipRevision, EngineStaticStorage, ProviderBaseConstruction,
        ProviderAllocatesNestedResources, ProviderRetainsBorrowedInstance,
        ProviderRetainedThroughObjectUse, TrivialDestructionDoesNotRelease});
}

enum class Participant : std::uint64_t {
    EngineStaticObjectOwner = 1,
    Vgui1BaseConstructor = 2,
    Vgui1BorrowedInstanceHolder = 3
};

constexpr std::uint64_t ProviderRoleSignature() {
    // A consumer requires this provider role; it does not substitute its own
    // engine role into the provider's offered dependency catalog.
    constexpr std::uint64_t Vgui1ProviderRole = 0x5647554931ull;
    return abi::LayoutHash({ContractRevision, Vgui1ProviderRole, OwnershipSignature(),
        static_cast<std::uint64_t>(Participant::EngineStaticObjectOwner),
        static_cast<std::uint64_t>(Participant::Vgui1BaseConstructor),
        static_cast<std::uint64_t>(Participant::Vgui1BorrowedInstanceHolder)});
}

static_assert(abi::RuntimeVersion == 10, "Coordinated runtime epoch");
static_assert(sizeof(abi::Descriptor) == 288, "Preserve the qualified descriptor envelope");

} }
