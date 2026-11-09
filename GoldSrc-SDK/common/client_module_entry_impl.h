// Included once in each actual client entry TU, after its complete shared type declarations.
#include "client_module_contracts.h"
#include "client_module_entry.h"

#if defined(_WIN32)
#define CSE_CLIENT_ENTRY_EXPORT __declspec(dllexport)
#else
#define CSE_CLIENT_ENTRY_EXPORT __attribute__((visibility("default")))
#endif

namespace {
bool clientEngineAbiBound = false;
bool clientInitializationAuthorized = false;
bool clientFunctionExchangeAuthorized = false;
bool clientStudioExchangeAuthorized = false;
}

bool CseClientEngineAbiBound() {
    return clientEngineAbiBound;
}

bool CseClientConsumeInitializationAuthorization() {
    const bool authorized = clientInitializationAuthorized;
    clientInitializationAuthorized = false;
    return authorized;
}

bool CseClientConsumeFunctionAuthorization() {
    const bool authorized = clientFunctionExchangeAuthorized;
    clientFunctionExchangeAuthorized = false;
    return authorized;
}

bool CseClientConsumeStudioAuthorization() {
    const bool authorized = clientStudioExchangeAuthorized && clientEngineAbiBound;
    clientStudioExchangeAuthorized = false;
    return authorized;
}

extern "C" int CSE_CLIENT_ENTRY_CALL Initialize(cl_enginefunc_t*, int);
extern "C" void CSE_CLIENT_ENTRY_CALL F(void*);
extern "C" int CSE_CLIENT_ENTRY_CALL HUD_GetStudioModelInterface(int, r_studio_interface_t**, engine_studio_api_t*);

extern "C" CSE_CLIENT_ENTRY_EXPORT const cse::abi::Descriptor* CSE_CLIENT_ENTRY_CALL CseGetModuleAbi() {
    return &cse::abi::ClientModuleDescriptor;
}

extern "C" CSE_CLIENT_ENTRY_EXPORT unsigned int CSE_CLIENT_ENTRY_CALL CseExchangeClientFunctions(
    const cse::abi::Descriptor* engineAbi, cldll_func_t* functions) {
    clientEngineAbiBound = false;
    clientFunctionExchangeAuthorized = false;
    if (cse::abi::CheckRequired(cse::abi::EngineClientDescriptor, engineAbi)
            != cse::abi::Compatibility::Compatible
        || !functions || !functions->pInitFunc || !functions->pHudVidInitFunc) {
        return 0;
    }
    clientFunctionExchangeAuthorized = true;
    F(functions);
    const bool consumed = !clientFunctionExchangeAuthorized;
    clientFunctionExchangeAuthorized = false;
    return consumed ? 1 : 0;
}

extern "C" CSE_CLIENT_ENTRY_EXPORT unsigned int CSE_CLIENT_ENTRY_CALL CseInitializeClientModule(
    const cse::abi::Descriptor* engineAbi, cl_enginefunc_t* functions, int version) {
    clientEngineAbiBound = false;
    clientInitializationAuthorized = false;
    if (!functions || version != CLDLL_INTERFACE_VERSION
        || cse::abi::CheckRequired(cse::abi::EngineClientDescriptor, engineAbi)
            != cse::abi::Compatibility::Compatible) {
        return 0;
    }
    clientInitializationAuthorized = true;
    const int initialized = Initialize(functions, version);
    clientEngineAbiBound = initialized != 0 && !clientInitializationAuthorized;
    clientInitializationAuthorized = false;
    return clientEngineAbiBound ? 1 : 0;
}

extern "C" CSE_CLIENT_ENTRY_EXPORT unsigned int CSE_CLIENT_ENTRY_CALL CseGetClientStudioInterface(
    const cse::abi::Descriptor* engineAbi, int version, r_studio_interface_t** renderer, engine_studio_api_t* engine) {
    clientStudioExchangeAuthorized = false;
    if (!clientEngineAbiBound || !renderer || !engine || version != STUDIO_INTERFACE_VERSION
        || cse::abi::CheckRequired(cse::abi::EngineClientDescriptor, engineAbi)
            != cse::abi::Compatibility::Compatible) {
        return 0;
    }
    clientStudioExchangeAuthorized = true;
    const int initialized = HUD_GetStudioModelInterface(version, renderer, engine);
    const bool consumed = !clientStudioExchangeAuthorized;
    clientStudioExchangeAuthorized = false;
    return initialized != 0 && consumed ? 1 : 0;
}

#undef CSE_CLIENT_ENTRY_EXPORT

static_assert(std::is_same<decltype(&CseGetModuleAbi), cse::abi::GetModuleAbi>::value,
    "Client descriptor export changed its C bootstrap prototype");
static_assert(std::is_same<decltype(&CseInitializeClientModule), CseInitializeClient>::value,
    "Client initialization export changed its C prototype");
static_assert(std::is_same<decltype(&CseExchangeClientFunctions), CseExchangeClient>::value,
    "Client F exchange export changed its C prototype");
static_assert(std::is_same<decltype(&CseGetClientStudioInterface), CseClientStudio>::value,
    "Client studio exchange export changed its C prototype");
