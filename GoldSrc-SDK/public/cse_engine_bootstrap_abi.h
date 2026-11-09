#pragma once

// Include the actual IFileSystem/interface declarations before this header.
// This is metadata and typed validation only: it publishes no module entry and
// performs no loading or callbacks. Promotion also requires the physical slot
// probes bound as a non-optional owning-build prerequisite: member types alone
// cannot detect reordered virtual declarations. This header is not a publisher.
#include "cse_filesystem_abi.h"

#include <type_traits>

namespace cse { namespace engine_bootstrap_abi {
using abi::LayoutHash;

constexpr std::uint32_t RoleFamily = 0x4f4d0001;
constexpr std::uint32_t CppProfileFamily = 0x4f4d0002;
constexpr std::uint32_t BootstrapFamily = 0x4f4d0100;
constexpr std::uint64_t ContractRevision = 1;
constexpr std::uint64_t OwnershipRevision = 1;
constexpr std::uint64_t RestartBufferBytes = 2048; // launcher's NEW_COMMAND_LENGTH.
constexpr std::uint64_t StatusMapBytes = 64; // dedicated caller's map output storage.

enum class Role : std::uint64_t {
    EngineProvider = 1,
    Launcher = 2,
    DedicatedHost = 3
};

enum class RunResult : std::uint64_t {
    None = 0,
    Restart = 1,
    VideoModeFailed = 2
};

enum class Interface : std::uint64_t {
    EngineApi002 = 1,
    DedicatedApi002 = 2,
    DedicatedExports001 = 3
};

enum class ParameterRole : std::uint64_t {
    BorrowedInstanceForRun = 1,
    BorrowedBaseDirectory = 2,
    BorrowedCommandString = 3,
    RetainedRestartOutputDuringRun = 4,
    BorrowedLauncherFactoryDuringRun = 5,
    BorrowedDedicatedFactoryUntilShutdown = 6,
    BorrowedFilesystemFactoryUntilShutdown = 7,
    BorrowedConsoleInputForCall = 8,
    FloatOutputForCall = 9,
    SignedCountOutputForCall = 10,
    MapOutputForCall = 11,
    BorrowedPrintInputForCall = 12,
    BorrowedFilesystemFactoryDuringRun = 13
};

enum class SourceSpelling : std::uint64_t {
    MutableProducerCommand = 1,
    ConstLauncherCommand = 2,
    MutableDedicatedCommand = 3
};

using Factory = void* (CSE_ABI_CALL *)(const char*, int*);
using RunCall = int (*)(void*, char*, char*, char*, Factory, Factory);
using InitCall = bool (*)(char*, char*, Factory, Factory);
using ShutdownCall = int (*)();
using FrameCall = bool (*)();
using ConsoleCall = void (*)(char*);
using StatusCall = void (*)(float*, int*, int*, char*);
using PrintCall = void (*)(char*);

template<class Function> struct CallShape {
    static constexpr std::uint64_t Value = filesystem_abi::TypeShape<Function>::Value;
};

constexpr std::uint64_t Parameter(ParameterRole role) {
    return static_cast<std::uint64_t>(role);
}

struct Method {
    Interface interfaceId;
    std::uint64_t ordinal;
    std::uint64_t callShape;
    std::uint64_t parameterRoles;
};

// Ordering is source owned and checked against real compiler vtable dumps.
// The Run command pointer is normalized only after its two distinct source
// spellings have passed the role-specific exact member-type validators below.
constexpr Method Methods[] = {
    {Interface::EngineApi002, 0, CallShape<RunCall>::Value,
        LayoutHash({Parameter(ParameterRole::BorrowedInstanceForRun), Parameter(ParameterRole::BorrowedBaseDirectory),
            Parameter(ParameterRole::BorrowedCommandString), Parameter(ParameterRole::RetainedRestartOutputDuringRun),
            Parameter(ParameterRole::BorrowedLauncherFactoryDuringRun),
            Parameter(ParameterRole::BorrowedFilesystemFactoryDuringRun)})},
    {Interface::DedicatedApi002, 0, CallShape<InitCall>::Value,
        LayoutHash({Parameter(ParameterRole::BorrowedBaseDirectory), Parameter(ParameterRole::BorrowedCommandString),
            Parameter(ParameterRole::BorrowedDedicatedFactoryUntilShutdown),
            Parameter(ParameterRole::BorrowedFilesystemFactoryUntilShutdown)})},
    {Interface::DedicatedApi002, 1, CallShape<ShutdownCall>::Value, LayoutHash({0})},
    {Interface::DedicatedApi002, 2, CallShape<FrameCall>::Value, LayoutHash({0})},
    {Interface::DedicatedApi002, 3, CallShape<ConsoleCall>::Value,
        LayoutHash({Parameter(ParameterRole::BorrowedConsoleInputForCall)})},
    {Interface::DedicatedApi002, 4, CallShape<StatusCall>::Value,
        LayoutHash({Parameter(ParameterRole::FloatOutputForCall), Parameter(ParameterRole::SignedCountOutputForCall),
            Parameter(ParameterRole::SignedCountOutputForCall), Parameter(ParameterRole::MapOutputForCall)})},
    {Interface::DedicatedExports001, 0, CallShape<PrintCall>::Value,
        LayoutHash({Parameter(ParameterRole::BorrowedPrintInputForCall)})}
};

constexpr std::size_t MethodCount = sizeof(Methods) / sizeof(Methods[0]);

constexpr std::uint64_t ContractSignature() {
    // The nested filesystem pointer leads to the already qualified typed method
    // catalog; it is not treated as an end-to-end opaque void* result.
    std::uint64_t signature = LayoutHash({ContractRevision, OwnershipRevision, MethodCount,
        filesystem_abi::DestructorSlots, RestartBufferBytes, StatusMapBytes,
        static_cast<std::uint64_t>(SourceSpelling::MutableProducerCommand),
        static_cast<std::uint64_t>(SourceSpelling::ConstLauncherCommand),
        static_cast<std::uint64_t>(SourceSpelling::MutableDedicatedCommand),
        CallShape<Factory>::Value, IFACE_OK, IFACE_FAILED,
        filesystem_abi::Public009, filesystem_abi::ContractSignature(),
        filesystem_abi::ProviderRole, filesystem_abi::BorrowedProviderLifetime,
        static_cast<std::uint64_t>(RunResult::None), static_cast<std::uint64_t>(RunResult::Restart),
        static_cast<std::uint64_t>(RunResult::VideoModeFailed)});
    for (const auto& method : Methods) {
        signature = LayoutHash({signature, static_cast<std::uint64_t>(method.interfaceId), method.ordinal,
            method.ordinal + filesystem_abi::DestructorSlots, method.callShape, method.parameterRoles});
    }
    return signature;
}

constexpr std::uint64_t ProfileSignature() {
    using OrdinaryMember = int (IBaseInterface::*)();
    return LayoutHash({ContractRevision, filesystem_abi::Profile, filesystem_abi::DestructorSlots,
        sizeof(Factory), alignof(Factory), CallShape<Factory>::Value,
        sizeof(OrdinaryMember), alignof(OrdinaryMember), sizeof(int), alignof(int),
        sizeof(bool), alignof(bool), sizeof(float), alignof(float)});
}

constexpr std::uint64_t RoleSignature(Role role) {
    // Factories return provider-owned singleton views. Callers do not delete
    // them; provider images and callback targets outlive their borrowed views.
    constexpr std::uint64_t BorrowedProviderSingleton = 1;
    return LayoutHash({ContractRevision, OwnershipRevision, static_cast<std::uint64_t>(role),
        BorrowedProviderSingleton});
}

constexpr abi::Descriptor DescribeRole(Role role) {
    return abi::Describe({
        {RoleFamily, 0, RoleSignature(role)},
        {CppProfileFamily, 0, ProfileSignature()},
        {BootstrapFamily, 0, ContractSignature()}
    });
}

// Only locally typed/validated descriptors are inputs to this expectation
// helper. An untrusted provider descriptor is passed directly to CheckRequired.
constexpr abi::Descriptor RequireRole(abi::Descriptor local, Role expectedRole) {
    for (std::size_t index = 0; index < local.tableCount && index < abi::MaxTables; ++index) {
        if (local.tables[index].identifier == RoleFamily) {
            local.tables[index].signature = RoleSignature(expectedRole);
        }
    }
    return local;
}

#if defined(_MSC_VER) && defined(_M_IX86)
#define CSE_BOOTSTRAP_MEMBER_CALL __thiscall
#else
#define CSE_BOOTSTRAP_MEMBER_CALL
#endif

template<class Type, class FactoryType> struct EngineDeclaration {
    using ProducerRun = int (CSE_BOOTSTRAP_MEMBER_CALL Type::*)(void*, char*, char*, char*, FactoryType, FactoryType);
    using LauncherRun = int (CSE_BOOTSTRAP_MEMBER_CALL Type::*)(void*, char*, const char*, char*, FactoryType,
        FactoryType);
    static constexpr bool Producer = std::is_same<FactoryType, Factory>::value
        && std::is_base_of<IBaseInterface, Type>::value && std::has_virtual_destructor<Type>::value
        && std::is_same<decltype(&Type::Run), ProducerRun>::value;
    static constexpr bool Launcher = std::is_same<FactoryType, Factory>::value
        && std::is_base_of<IBaseInterface, Type>::value && std::has_virtual_destructor<Type>::value
        && std::is_same<decltype(&Type::Run), LauncherRun>::value;
};

template<class Type, class FactoryType> struct DedicatedDeclaration {
    using Init = bool (CSE_BOOTSTRAP_MEMBER_CALL Type::*)(char*, char*, FactoryType, FactoryType);
    using Shutdown = int (CSE_BOOTSTRAP_MEMBER_CALL Type::*)();
    using Frame = bool (CSE_BOOTSTRAP_MEMBER_CALL Type::*)();
    using Console = void (CSE_BOOTSTRAP_MEMBER_CALL Type::*)(char*);
    using Status = void (CSE_BOOTSTRAP_MEMBER_CALL Type::*)(float*, int*, int*, char*);
    static constexpr bool Valid = std::is_same<FactoryType, Factory>::value
        && std::is_base_of<IBaseInterface, Type>::value && std::has_virtual_destructor<Type>::value
        && std::is_same<decltype(&Type::Init), Init>::value
        && std::is_same<decltype(&Type::Shutdown), Shutdown>::value
        && std::is_same<decltype(&Type::RunFrame), Frame>::value
        && std::is_same<decltype(&Type::AddConsoleText), Console>::value
        && std::is_same<decltype(&Type::UpdateStatus), Status>::value;
};

template<class Type> struct ExportsDeclaration {
    using Print = void (CSE_BOOTSTRAP_MEMBER_CALL Type::*)(char*);
    static constexpr bool Valid = std::is_base_of<IBaseInterface, Type>::value
        && std::has_virtual_destructor<Type>::value && std::is_same<decltype(&Type::Sys_Printf), Print>::value;
};

#undef CSE_BOOTSTRAP_MEMBER_CALL

template<class Engine, class Dedicated, class Exports, class FactoryType> constexpr abi::Descriptor DescribeEngine() {
    static_assert(EngineDeclaration<Engine, FactoryType>::Producer, "Actual producer Run declaration differs");
    static_assert(DedicatedDeclaration<Dedicated, FactoryType>::Valid, "Actual dedicated API declaration differs");
    static_assert(ExportsDeclaration<Exports>::Valid, "Actual engine callback declaration differs");
    return DescribeRole(Role::EngineProvider);
}

template<class Engine, class FactoryType> constexpr abi::Descriptor DescribeLauncher() {
    static_assert(EngineDeclaration<Engine, FactoryType>::Launcher, "Actual launcher Run declaration differs");
    return DescribeRole(Role::Launcher);
}

template<class Dedicated, class Exports, class FactoryType> constexpr abi::Descriptor DescribeDedicatedHost() {
    static_assert(DedicatedDeclaration<Dedicated, FactoryType>::Valid, "Actual dedicated caller declaration differs");
    static_assert(ExportsDeclaration<Exports>::Valid, "Actual host callback declaration differs");
    static_assert(std::is_same<FactoryType, Factory>::value, "Actual host factory declaration differs");
    return DescribeRole(Role::DedicatedHost);
}

} } // namespace cse::engine_bootstrap_abi
