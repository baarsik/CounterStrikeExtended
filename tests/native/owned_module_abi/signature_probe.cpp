#include "public/cse_module_abi.h"
#include "common/ISystemModule.h"

#include <type_traits>

// Compile-only catalog spike. These test IDs/signatures are not a published ABI.
namespace {
using cse::abi::LayoutHash;

constexpr std::uint32_t TestLifecycleFamily = 0x7f010001;
constexpr std::uint64_t TestContractRevision = 1;
constexpr std::uint64_t BorrowedUntilProviderUnload = 1;
constexpr std::uint64_t HostSystemRole = 1;
constexpr std::uint64_t LifecycleObjectRole = 2;

template<class T, class Enable = void> struct Shape;

template<class T> struct Shape<T, typename std::enable_if<std::is_arithmetic<T>::value>::type> {
    static constexpr std::uint64_t Value = LayoutHash({1, sizeof(T), alignof(T), std::is_signed<T>::value,
        std::is_floating_point<T>::value, std::is_same<T, bool>::value});
};

template<> struct Shape<void> {
    static constexpr std::uint64_t Value = LayoutHash({2});
};

template<> struct Shape<IBaseSystem> {
    static constexpr std::uint64_t Value = LayoutHash({3, HostSystemRole});
};

template<> struct Shape<ISystemModule> {
    static constexpr std::uint64_t Value = LayoutHash({3, LifecycleObjectRole});
};

template<class T> struct Shape<T*> {
    static constexpr std::uint64_t Value = LayoutHash({4, sizeof(T*), alignof(T*), Shape<T>::Value});
};

template<class T> struct MethodShape;

template<class Class, class Result, class... Arguments> struct MethodShape<Result (Class::*)(Arguments...)> {
    static constexpr std::uint64_t Value = LayoutHash({5, sizeof...(Arguments), Shape<Result>::Value,
        Shape<Arguments>::Value...});
};

// Each member's actual declaration determines its argument/return shape. The
// ordinal is explicit; no member pointer is converted to an address or integer.
constexpr std::uint64_t Methods[] = {
    MethodShape<decltype(&ISystemModule::Init)>::Value,
    MethodShape<decltype(&ISystemModule::RunFrame)>::Value,
    MethodShape<decltype(&ISystemModule::ReceiveSignal)>::Value,
    MethodShape<decltype(&ISystemModule::ExecuteCommand)>::Value,
    MethodShape<decltype(&ISystemModule::RegisterListener)>::Value,
    MethodShape<decltype(&ISystemModule::RemoveListener)>::Value,
    MethodShape<decltype(&ISystemModule::GetSystem)>::Value,
    MethodShape<decltype(&ISystemModule::GetSerial)>::Value,
    MethodShape<decltype(&ISystemModule::GetStatusLine)>::Value,
    MethodShape<decltype(&ISystemModule::GetType)>::Value,
    MethodShape<decltype(&ISystemModule::GetName)>::Value,
    MethodShape<decltype(&ISystemModule::GetState)>::Value,
    MethodShape<decltype(&ISystemModule::GetVersion)>::Value,
    MethodShape<decltype(&ISystemModule::ShutDown)>::Value
};
constexpr std::size_t MethodCount = sizeof(Methods) / sizeof(Methods[0]);

constexpr std::uint64_t Signature(bool swapFirstMethods, std::uint64_t ownership) {
    std::uint64_t result = LayoutHash({TestContractRevision, MethodCount, ownership,
        std::has_virtual_destructor<ISystemModule>::value});
    for (std::size_t ordinal = 0; ordinal < MethodCount; ++ordinal) {
        const std::size_t source = swapFirstMethods && ordinal < 2 ? 1 - ordinal : ordinal;
        result = LayoutHash({result, ordinal, Methods[source]});
    }
    return result;
}

constexpr auto Required = cse::abi::Describe({
    {TestLifecycleFamily, 0, Signature(false, BorrowedUntilProviderUnload)}
});
constexpr auto WrongOrder = cse::abi::Describe({
    {TestLifecycleFamily, 0, Signature(true, BorrowedUntilProviderUnload)}
});
constexpr auto WrongOwnership = cse::abi::Describe({
    {TestLifecycleFamily, 0, Signature(false, BorrowedUntilProviderUnload + 1)}
});

static_assert(MethodCount == 14, "Actual lifecycle interface method inventory changed");
static_assert(Methods[0] != Methods[1], "Actual Init and RunFrame types differ");
static_assert(Signature(false, BorrowedUntilProviderUnload) != Signature(true, BorrowedUntilProviderUnload),
    "Identical interface size does not conceal swapped method order");
static_assert(cse::abi::CheckRequired(Required, &WrongOrder) == cse::abi::Compatibility::TableLayout,
    "A swapped ordered typed method catalog is rejected");
static_assert(cse::abi::CheckRequired(Required, &WrongOwnership) == cse::abi::Compatibility::TableLayout,
    "Ownership revision is independently checked");
}
