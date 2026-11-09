#pragma once

// Include after APIProxy.h. Keep this declaration layer usable by C++14 entry TUs.
namespace cse { namespace abi { struct Descriptor; } }
#if defined(_WIN32)
#define CSE_CLIENT_ENTRY_CALL __cdecl
#else
#define CSE_CLIENT_ENTRY_CALL
#endif

using CseInitializeClient = unsigned int (CSE_CLIENT_ENTRY_CALL *)(const cse::abi::Descriptor*,
    cl_enginefunc_t*, int);
using CseExchangeClient = unsigned int (CSE_CLIENT_ENTRY_CALL *)(const cse::abi::Descriptor*, cldll_func_t*);
using CseClientStudio = unsigned int (CSE_CLIENT_ENTRY_CALL *)(const cse::abi::Descriptor*, int,
    struct r_studio_interface_s**, struct engine_studio_api_s*);

bool CseClientEngineAbiBound();
bool CseClientConsumeInitializationAuthorization();
bool CseClientConsumeFunctionAuthorization();
bool CseClientConsumeStudioAuthorization();
