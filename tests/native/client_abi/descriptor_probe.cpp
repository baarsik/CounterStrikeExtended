#define CSE_ABI_ENGINE_VIEW
#include "fixture_types.h"
#include "../../../src/common/client_module_contracts.h"
#include "../../../src/public/native_module.h"
#include "../../../src/common/client_module_entry.h"
#include "../../../src/common/client_module_required_exports.h"
#include "../strings/high_offset_buffer.h"
#include <new>
#include <memory>

#include "movement_diagnostics.inc"

int main(int argc, char** argv) {
    const bool entryProbe = argc == 3 && std::strcmp(argv[2], "--entry-probe") == 0;
    if (argc != 2 && !entryProbe) {
        return 2;
    }
    std::unique_ptr<HighOffsetBuffer> entryStorage;
    void* module = cse_platform::LoadModule(argv[1], cse_platform::ModuleFallback::None);
    if (!module) {
        return 3;
    }
    struct ModuleLifetime {
        void* handle;
        ~ModuleLifetime() {
            cse_platform::UnloadModule(handle);
        }
    } lifetime{module};
    const auto get = reinterpret_cast<cse::abi::GetModuleAbi>(cse_platform::GetSymbol(module, "CseGetModuleAbi"));
    const auto* actual = get ? get() : nullptr;
    if (actual) {
        for (std::size_t i = 0; i < actual->tableCount && i < cse::abi::MaxTables; ++i) {
            std::printf("%x expected=%llx offered=%llx\n", actual->tables[i].identifier,
                static_cast<unsigned long long>(cse::abi::ClientModuleDescriptor.tables[i].signature),
                static_cast<unsigned long long>(actual->tables[i].signature));
        }
    }
    const auto fact = reinterpret_cast<unsigned long long (*)(unsigned int)>(
        cse_platform::GetSymbol(module, "CseFixtureMovementFact"));
    if (fact) {
        for (unsigned int index = 0; index < sizeof(fixtureLayoutFacts) / sizeof(fixtureLayoutFacts[0]); ++index) {
            const auto offered = fact(index);
            if (offered != fixtureLayoutFacts[index].value) {
                std::printf("FIELD %s expected=%llx offered=%llx\n", fixtureLayoutFacts[index].name,
                    fixtureLayoutFacts[index].value, offered);
            }
        }
    }
    const auto status = cse::abi::CheckRequired(cse::abi::ClientModuleDescriptor, actual);
    if (status != cse::abi::Compatibility::Compatible) {
        return 1;
    }
    if (entryProbe) {
        const auto exchange = reinterpret_cast<CseExchangeClient>(
            cse_platform::GetSymbol(module, "CseExchangeClientFunctions"));
        const auto initialize = reinterpret_cast<CseInitializeClient>(
            cse_platform::GetSymbol(module, "CseInitializeClientModule"));
        const auto studio = reinterpret_cast<CseClientStudio>(
            cse_platform::GetSymbol(module, "CseGetClientStudioInterface"));
        const auto legacyInitialize = reinterpret_cast<INITIALIZE_FUNC>(
            cse_platform::GetSymbol(module, "Initialize"));
        const auto legacyF = reinterpret_cast<void (*)(void*)>(cse_platform::GetSymbol(module, "F"));
        const auto legacyStudio = reinterpret_cast<HUD_STUDIO_INTERFACE_FUNC>(
            cse_platform::GetSymbol(module, "HUD_GetStudioModelInterface"));
        if (!exchange || !initialize || !studio || !legacyInitialize || !legacyF || !legacyStudio) {
            return 4;
        }
        auto invalid = cse::abi::EngineClientDescriptor;
        invalid.tables[2].signature ^= 1;
        if (exchange(&invalid, reinterpret_cast<cldll_func_t*>(1))
            || initialize(&invalid, reinterpret_cast<cl_enginefunc_t*>(1), CLDLL_INTERFACE_VERSION)
            || legacyInitialize(reinterpret_cast<cl_enginefunc_t*>(1), CLDLL_INTERFACE_VERSION)) {
            return 5;
        }
        r_studio_interface_t* renderer = reinterpret_cast<r_studio_interface_t*>(1);
        if (studio(&invalid, STUDIO_INTERFACE_VERSION, &renderer, reinterpret_cast<engine_studio_api_t*>(1))
            || legacyStudio(STUDIO_INTERFACE_VERSION, &renderer, reinterpret_cast<engine_studio_api_t*>(1))
            || renderer != reinterpret_cast<r_studio_interface_t*>(1)) {
            return 6;
        }
        struct ExchangeData {
            cldll_func_t functions;
            modfuncs_t modifiers;
            cldll_func_dst_t destinations;
        };
        entryStorage.reset(new HighOffsetBuffer(nullptr));
        if (!entryStorage->get()) {
            return 7;
        }
        auto* data = new (entryStorage->get()) ExchangeData{};
        if (sizeof(void*) > sizeof(std::uint32_t)
            && reinterpret_cast<std::uintptr_t>(data) <= UINT32_MAX) {
            return 8;
        }
        data->functions.pInitFunc = reinterpret_cast<INITIALIZE_FUNC>(&data->modifiers);
        data->functions.pHudVidInitFunc = reinterpret_cast<HUD_VIDINIT_FUNC>(&data->destinations);
        if (!exchange(&cse::abi::EngineClientDescriptor, &data->functions)) {
            return 9;
        }
#define CSE_REQUIRE_PROBE_EXPORT(member, name) if (!data->functions.member) { return 10; }
        CSE_CLIENT_REQUIRED_EXPORTS(CSE_REQUIRE_PROBE_EXPORT)
#undef CSE_REQUIRE_PROBE_EXPORT
        const cldll_func_t before = data->functions;
        legacyF(&data->functions);
        if (std::memcmp(&before, &data->functions, sizeof(before)) != 0) {
            return 11;
        }
        std::printf("Actual owned client F exchange/entry rejection passed at %llx; Initialize/providers not started\n",
            static_cast<unsigned long long>(reinterpret_cast<std::uintptr_t>(data)));
    }
    return 0;
}
