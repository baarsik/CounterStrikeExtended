#pragma once

// Included in the client entry TU after its actual Vector/cl_enginefunc_t declarations.
// Select the same movement declaration compiled by that client's movement implementation.
#include "com_model.h"
#include "client_module_api_types.h"
#if defined(CSE_CLIENT_ABI_CS)
#include "../cstrike/pm_shared/pm_defs.h"
#include "../cstrike/cl_dll/kbutton.h"
#else
#include "../pm_shared/pm_defs.h"
#include "../cl_dll/kbutton.h"
#endif
