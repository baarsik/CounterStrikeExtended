#pragma once
// Native standalone role only. The mandatory owning compiler gate checks the
// frozen complete method signatures/physical slots and exchanged public records.
#include "native_dedicated_abi.h"
#include "native_standalone_interfaces.h"

namespace cse::standalone_abi {
constexpr char Symbol[] = "CseGetStandaloneVguiAbiV1";
constexpr std::uint32_t RoleFamily = 0x56475350;
constexpr std::uint32_t ContractFamily = 0x56475343;
constexpr std::uint64_t ProviderRole = 1;
constexpr std::uint64_t OwnershipRevision = 1;
constexpr std::uint64_t FrozenPhysicalSchema = 0x89504f9ba0837216ull;
constexpr auto Provider = abi::Describe(
    {{RoleFamily, 0, abi::LayoutHash({ProviderRole, OwnershipRevision})},
     {ContractFamily, 0,
      abi::LayoutHash({FrozenPhysicalSchema, filesystem_abi::Profile, filesystem_abi::DestructorSlots, sizeof(void*),
                       sizeof(wchar_t), sizeof(vgui2::VPANEL), sizeof(vgui2::HPanel), sizeof(vgui2::HTexture),
                       sizeof(Color), alignof(Color), filesystem_abi::ContractSignature()})}});

inline CreateInterfaceFn CheckedFactory(void* module) {
    if (!dedicated_abi::Compatible(module, Symbol, Provider)) {
        return nullptr;
    }
    return reinterpret_cast<CreateInterfaceFn>(cse_platform::GetSymbol(module, "CreateInterface"));
}

inline bool ValidateViews(CreateInterfaceFn factory, CreateInterfaceFn filesystemFactory) {
    if (!factory || !filesystemFactory) {
        return false;
    }
    constexpr std::size_t ISurfaceMethods = 105;
    if (!dedicated_abi::ViewCompatible<vgui2::ISurface>(factory("VGUI_Surface026", nullptr), factory,
                                                        ISurfaceMethods)) {
        return false;
    }
    constexpr std::size_t IVGuiMethods = 24;
    if (!dedicated_abi::ViewCompatible<vgui2::IVGui>(factory("VGUI_ivgui006", nullptr), factory, IVGuiMethods)) {
        return false;
    }
    constexpr std::size_t IPanelMethods = 59;
    if (!dedicated_abi::ViewCompatible<vgui2::IPanel>(factory("VGUI_Panel007", nullptr), factory, IPanelMethods)) {
        return false;
    }
    constexpr std::size_t IInputMethods = 21;
    if (!dedicated_abi::ViewCompatible<vgui2::IInput>(factory("VGUI_Input004", nullptr), factory, IInputMethods)) {
        return false;
    }
    constexpr std::size_t ISystemMethods = 33;
    if (!dedicated_abi::ViewCompatible<vgui2::ISystem>(factory("VGUI_System009", nullptr), factory, ISystemMethods)) {
        return false;
    }
    constexpr std::size_t ILocalizeMethods = 20;
    if (!dedicated_abi::ViewCompatible<vgui2::ILocalize>(factory("VGUI_Localize003", nullptr), factory,
                                                         ILocalizeMethods)) {
        return false;
    }
    constexpr std::size_t ISchemeManagerMethods = 13;
    if (!dedicated_abi::ViewCompatible<vgui2::ISchemeManager>(factory("VGUI_Scheme009", nullptr), factory,
                                                              ISchemeManagerMethods)) {
        return false;
    }
    constexpr std::size_t IKeyValuesMethods = 9;
    if (!dedicated_abi::ViewCompatible<IKeyValues>(factory("KeyValues003", nullptr), factory, IKeyValuesMethods)) {
        return false;
    }
    constexpr std::size_t IInputInternalMethods = 37;
    if (!dedicated_abi::ViewCompatible<vgui2::IInputInternal>(factory("VGUI_InputInternal001", nullptr), factory,
                                                              IInputInternalMethods)) {
        return false;
    }
    return dedicated_abi::ViewCompatible<IFileSystem>(filesystemFactory("VFileSystem009", nullptr), filesystemFactory,
                                                      filesystem_abi::PrefixMethods);
}
} // namespace cse::standalone_abi
